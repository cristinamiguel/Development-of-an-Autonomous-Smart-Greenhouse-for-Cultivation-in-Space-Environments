#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MCP23X17.h>
#include <BH1750.h>

// -------------------------------------------------------------------------
// Configurações gerais e hardware
// -------------------------------------------------------------------------
Adafruit_MCP23X17 mcp; 
Adafruit_BME280 bme; 
BH1750 lightMeter; 

// Pinos Diretos no TTGO
#define PH_PIN 35      // Sonda de pH 
#define SOLO_PIN 34    // Sensor de humidade do solo
const int PINO_MOSFET = 25; // Controlo da bomba de rega via MOSFET 

// Mapeamento de Pinos no Expansor MCP23017 
const int PINO_EXP_VENTILACAO = 0; // IN1 -> Ventoinha
const int PINO_EXP_ILUMINACAO = 3; // IN4 -> LEDs de Iluminação 
const int PINO_EXP_ACIDO      = 4; // IN5 -> Bomba Peristáltica Ácido 
const int PINO_EXP_BASE       = 5; // IN6 -> Bomba Peristáltica Base 
const int PINO_EXP_VALV_CO2   = 7; // IN8 -> Válvula Solenoide de CO2 

// -------------------------------------------------------------------------
// Variáveis para controlar o tempo (TIME-SLICING)
// -------------------------------------------------------------------------
unsigned long tempoAtual = 0; 

// Intervalos de execução de cada subsistema para evitar sobrecarga
const unsigned long INTERVALO_CLIMA      = 5000;  // Clima verifica a cada 5s
const unsigned long INTERVALO_LUZ        = 11000; // Luz monitoriza a cada 11s 
const unsigned long INTERVALO_SOLO       = 17000; // Solo verifica a cada 17s
const unsigned long INTERVALO_PH         = 23000; // pH analisa a cada 23s 
const unsigned long INTERVALO_ATMOSFERA  = 29000; // CO2 verifica a cada 29s

// Registos do último milissegundo em que cada subsistema rodou
unsigned long ultimoTempoClima      = 0;
unsigned long ultimoTempoLuz        = 0;
unsigned long ultimoTempoSolo       = 0;
unsigned long ultimoTempoPH         = 0;
unsigned long ultimoTempoAtmosfera  = 0;

// -------------------------------------------------------------------------
// Variáveis específicas de cada subsistema
// -------------------------------------------------------------------------
// Subsistema 1: Regulação Térmica e Circulação de Ar (BME280)
const float LIMITE_TEMP_ALTA = 22.0;       // Limite para ativar ventoinha 
const float LIMITE_HUM_ALTA  = 70.0;       // Limite da humidade do ar 
const float LIMITE_TEMP_OK   = 18.0;       // Limite de conservação para desligar 
const float LIMITE_HUM_OK    = 50.0;       // Humidade segura para desligar 

// Subsistema 2: Iluminação e Fotoperíodo (BH1750)
const float THRESHOLD_LUZ = 50.0;          
const unsigned long DURACAO_DIA = 20000;   // 20s de simulação
const unsigned long DURACAO_NOITE = 10000; // 10s de simulação 
unsigned long tempoTransicaoLuz = 0;
bool eDia = true;                          

// Subsistema 3: Humidade do Solo (Irrigação)
const int Valor_Seco = 2500;               
const int Valor_Agua = 1600;               
bool regaEmCurso = false;
unsigned long tempoInicioRega = 0;
const unsigned long TEMPO_REGA = 4000;     // Reduzido para 4s para prevenir contra os resets elétricos

// Subsistema 4: Controlo de pH (Nutrientes)
const float VOLTAGEM_PH7 = 1.61;           
const float DECLIVE_PH = 5.66;             
float phSimulado_Demo = 8.2;               
bool forçarDemoPH = true;          // <<< esta linha é para ter sempre uma demonstração

// Subsistema 5: Atmosfera (CO2 / O2)
float o2Simulado_Demo = 21.02;             
bool modoDemoAtivo = true;                 

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); 
  analogReadResolution(12); // ESP32 a 12 bits (0-4095) 
  
  pinMode(PH_PIN, INPUT); 
  pinMode(SOLO_PIN, INPUT); 
  pinMode(PINO_MOSFET, OUTPUT);
  digitalWrite(PINO_MOSFET, LOW); // Garante que a bomba de rega está off 

  // Inicializar o barramento I2C partilhado nos pinos da TTGO
  Wire.begin(21, 22);
  delay(500); 

  // Inicializar Expansor MCP23017 (0x20)
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("ERRO CRÍTICO: MCP23017 não encontrado!"); 
    while (1); 
  }

  // Inicializar Sensor Clima BME280 (0x77)
  if (!bme.begin(0x77)) { 
    Serial.println("Erro crítico: BME280 não encontrado!"); 
    while (1); 
  }

  // Inicializar Sensor Luminosidade BH1750
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("--- BH1750 configurado com sucesso ---"); 
  } else {
    Serial.println("ERRO CRÍTICO: BH1750 não encontrado!");
    while (1); 
  }

  // Configurei as saídas do Expansor e forcei o estado OFF por segurança (HIGH)
  mcp.pinMode(PINO_EXP_VENTILACAO, OUTPUT); 
  mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH); 
  mcp.pinMode(PINO_EXP_ILUMINACAO, OUTPUT); 
  mcp.digitalWrite(PINO_EXP_ILUMINACAO, HIGH); 
  mcp.pinMode(PINO_EXP_ACIDO, OUTPUT);      
  mcp.digitalWrite(PINO_EXP_ACIDO, HIGH); 
  mcp.pinMode(PINO_EXP_BASE, OUTPUT);       
  mcp.digitalWrite(PINO_EXP_BASE, HIGH); 
  mcp.pinMode(PINO_EXP_VALV_CO2, OUTPUT);   
  mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); 

  tempoTransicaoLuz = millis(); 
  // desfasamento no arranque (Evita choque de carga inicial)
tempoTransicaoLuz = millis(); 
  // Desfasamento no arranque mais espaçado
  ultimoTempoClima      = millis() + 1000;   // 1 segundo após ligar
  ultimoTempoLuz        = millis() + 3000;   // 3 segundos após ligar
  ultimoTempoSolo       = millis() + 5000;   // 5 segundos após ligar
  ultimoTempoPH         = millis() + 7000;   // 7 segundos após ligar
  ultimoTempoAtmosfera  = millis() + 9000;   // 9 segundos após ligar
  Serial.println("\n__________________________________________________________");
  Serial.println("--- Sistema Integrado---");
  Serial.println("__________________________________________________________");
}

// ------------------------------------------------
// Loop Principal
// ------------------------------------------------
void loop() {
  tempoAtual = millis(); 

  // -----------------------------------------------------------------------
  // SUBSISTEMA 1: Regulação Térmica e Clima (BME280)
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoClima >= INTERVALO_CLIMA && !regaEmCurso) {
    ultimoTempoClima = tempoAtual;
    Serial.println("\n[SUBSISTEMA 1 - TÉRMICO & CIRCULAÇÃO]");
    
    float temp = bme.readTemperature(); 
    float hum  = bme.readHumidity(); 
    
    Serial.print("-> Temp: "); Serial.print(temp, 1); Serial.print(" C | Hum: "); Serial.print(hum, 1); Serial.println(" %"); 

    if (temp > 22 || hum > 70) {
      Serial.println("-> ALERTA: Limites excedidos! Ativando Ventoinha (IN1)..."); 
      mcp.digitalWrite(PINO_EXP_VENTILACAO, LOW); // Ativa por nível baixo
    } else if (temp < 18 && hum < 50) { 
      Serial.println("-> Clima OK. Desligando Ventoinha."); 
      mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH); 
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 2: Fotoperíodo e Iluminação (BH1750)
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoLuz >= INTERVALO_LUZ) {
    ultimoTempoLuz = tempoAtual;
    Serial.println("\n[SUBSISTEMA 2 - ILUMINAÇÃO]");
    
    float lux = lightMeter.readLightLevel(); 

    if (eDia) { 
      mcp.digitalWrite(PINO_EXP_ILUMINACAO, LOW); // LEDs ON 
      Serial.print("-> [Janela Ativa - DIA] Luminosidade: "); 
      Serial.print(lux, 1); 
      Serial.println(" lx");

      if (lux < THRESHOLD_LUZ) { 
        Serial.println("!!! ALERTA DE HARDWARE: LEDs em falha ou sem potência !!!"); 
      }
      if (tempoAtual - tempoTransicaoLuz >= DURACAO_DIA) { 
        Serial.println("-> Transição Horária: Iniciando Janela de Descanso (NOITE)...");
        eDia = false; 
        tempoTransicaoLuz = tempoAtual;
      }
    } else {
      mcp.digitalWrite(PINO_EXP_ILUMINACAO, HIGH); // LEDs OFF 
      Serial.print("-> [JANELA DESCANSO - NOITE] Luminosidade: ");
      Serial.print(lux, 1); 
      Serial.println(" lx"); 

      if (tempoAtual - tempoTransicaoLuz >= DURACAO_NOITE) {
        Serial.println("-> Transição Horária: Iniciando Janela Activa (DIA)..."); 
        eDia = true; 
        tempoTransicaoLuz = tempoAtual;
      }
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 3: Irrigação do Solo (SENSOR CAPACITIVO e MOSFET)
  // -----------------------------------------------------------------------
  if (!regaEmCurso) {
    if (tempoAtual - ultimoTempoSolo >= INTERVALO_SOLO) {
      ultimoTempoSolo = tempoAtual;
      Serial.println("\n[SUBSISTEMA 3 - HUMIDADE DO SOLO]");
      
      int lecturaSolo = analogRead(SOLO_PIN); 
      int humidadeSolo = map(lecturaSolo, Valor_Seco, Valor_Agua, 0, 100);
      humidadeSolo = constrain(humidadeSolo, 0, 100); 

      Serial.print("-> Solo Humidade: "); 
      Serial.print(humidadeSolo); 
      Serial.println("%"); 

      if (humidadeSolo < 40) { 
        Serial.println("-> ALERTA: Solo Seco! Disparando Bomba Submersível isoladamente..."); 
        digitalWrite(PINO_MOSFET, HIGH); // Ligar o MOSFET 
        regaEmCurso = true;
        tempoInicioRega = tempoAtual;
      } else if (humidadeSolo >= 70) {
        digitalWrite(PINO_MOSFET, LOW); 
      }
    }
  } else {
    // Gestão do tempo de rega para não parar a placa
    if (tempoAtual - tempoInicioRega >= TEMPO_REGA) {
      digitalWrite(PINO_MOSFET, LOW); // Desligar a bomba após o tempo de rega 
      regaEmCurso = false;
      ultimoTempoSolo = tempoAtual; // Adicionar margem para a próxima leitura
      Serial.println("-> Fluxo de irrigação concluído. Voltando à monitorização.");
    }
  }

 
 // -----------------------------------------------------------------------
  // SUBSISTEMA 4: Controlo do pH (MODO DEMONSTRAÇÃO DIRETAMENTE)
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoPH >= INTERVALO_PH && !regaEmCurso) {
    ultimoTempoPH = tempoAtual;
    Serial.println("\n[SUBSISTEMA 4 - ESTABILIZAÇÃO DE pH]");
    
    // Deixar a leitura física aqui para o caso de o "else" ser ativado
    int leituraPH = 0; 
    for(int i=0; i<10; i++) { leituraPH += analogRead(PH_PIN); delay(10); } 
    leituraPH /= 10;

    float voltagemPH = leituraPH * (3.3 / 4095.0); 
    float valorPHReal = 7.0 + (VOLTAGEM_PH7 - voltagemPH) * DECLIVE_PH; 
    valorPHReal = constrain(valorPHReal, 0.0, 14.0); 

    // --- SE FORÇAR A DEMONSTRAÇÃO POR CÓDIGO ---
    // Comentar esta secção para testar a sonda real
    if (true) { 
      Serial.print("-> [pH SYSTEM - MODO DEMO ATIVO] pH Atual no Copo: "); Serial.println(phSimulado_Demo, 2);

      if (phSimulado_Demo > 6.5) { 
        Serial.println("-> ALERTA: pH Alto! A ativar A4 (IN5 - Bomba de Ácido)..."); 
        mcp.digitalWrite(PINO_EXP_ACIDO, LOW); 
        delay(2000);                          
        mcp.digitalWrite(PINO_EXP_ACIDO, HIGH); 
        phSimulado_Demo -= 0.5; 
        Serial.println("-> Dose de pH Down aplicada com sucesso."); 
      } 
      else if (phSimulado_Demo < 5.5) {
        Serial.println("-> ALERTA: pH Baixo! A ativar A5 (IN6 - Bomba de Base)..."); 
        mcp.digitalWrite(PINO_EXP_BASE, LOW);  
        delay(2000);                          
        mcp.digitalWrite(PINO_EXP_BASE, HIGH); 
        phSimulado_Demo += 0.4; 
        Serial.println("-> Dose de pH Up aplicada com sucesso."); 
      } 
      else {
        Serial.println("-> pH quimicamente estabilizado em 6.0 (Ideal). Bombas em repouso."); 
        phSimulado_Demo = 8.2; 
      }
    } 
    // --- MODO REAL (SÓ CORRE SE MUDARES O 'if(true)' PARA 'if(false)') ---
    else {
      Serial.print("-> [pH SYSTEM - REAL] Voltagem: "); 
      Serial.print(voltagemPH); 
      Serial.print("V | pH Calculado: "); 
      Serial.println(valorPHReal, 2); 
      
      if (valorPHReal > 6.5) { 
        mcp.digitalWrite(PINO_EXP_ACIDO, LOW); 
        delay(2000); 
        mcp.digitalWrite(PINO_EXP_ACIDO, HIGH); 
      } else if (valorPHReal < 5.5) { 
        mcp.digitalWrite(PINO_EXP_BASE, LOW); 
        delay(2000); 
        mcp.digitalWrite(PINO_EXP_BASE, HIGH); 
      }
    }
  } 
// -----------------------------------------------------------------------
  // SUBSISTEMA 5: Atmosfera/CO2/O2 (MODO DEMONSTRAÇÃO DIRETAMENTE)
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoAtmosfera >= INTERVALO_ATMOSFERA && !regaEmCurso) {
    ultimoTempoAtmosfera = tempoAtual;
    Serial.println("\n[SUBSISTEMA 5 - ATMOSFERA E INJEÇÃO CO2]");
    
    // Converte o Oxigénio Simulado da Demo diretamente para a fórmula que tinha no relatório intermédio
    float co2EstimadoPPM = 400.0 + (21.0 - o2Simulado_Demo) * 10000.0;
    if (co2EstimadoPPM < 0) co2EstimadoPPM = 400.0;

    Serial.print("-> Oxigénio Simulado em Exibição: "); Serial.print(o2Simulado_Demo, 2); Serial.println("%");
    Serial.print("-> Dióxido de Carbono Calculado: "); Serial.print(co2EstimadoPPM, 0); Serial.println(" ppm");

    if (co2EstimadoPPM < 800) {
      Serial.println("-> ALERTA TR1: CO2 < 800 ppm! Abrir Válvula Solenoide por 4s...");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, LOW);  // Liga o relé -> Abre a válvula
      delay(4000);                               // Pulso nítido de 4s para o vídeo
      mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Desliga o relé -> Fecha a válvula
      Serial.println("-> Injeção de gás concluída. Válvula selada.");
      
      o2Simulado_Demo = 20.93; // Passa para o valor estável (1100 ppm) na próxima iteração
    } 
    else if (co2EstimadoPPM >= 1200) {
      Serial.println("-> CRÍTICO: CO2 >= 1200 ppm. Garantindo válvula FECHADA (LED IN8 OFF).");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
      
      o2Simulado_Demo = 21.02; // Altera para forçar uma queda drástica abaixo dos 800 ppm e vermos a valvula a trabalhar
    } 
    else {
      Serial.println("-> Nível de CO2 estável na meta [800 - 1200] ppm. Válvula em repouso.");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
      
      o2Simulado_Demo = 20.89; // Altera para forçar uma subida crítica acima dos 1200 ppm
    }
  }
}
