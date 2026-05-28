#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <BH1750.h>

Adafruit_MCP23X17 mcp;
BH1750 lightMeter;

// valores que obtive na calibracao
const float THRESHOLD_LUZ = 50.0;          // Corte consoante os testes  (Luz normal 45lx, Lâmpada 150lx)
const unsigned long DURACAO_DIA = 20000;   // 20 segundos de Dia (Simula o Fotoperíodo de 16h-> depois meter os 16*3600*1000 para passar a 16horas)
const unsigned long DURACAO_NOITE = 10000; // 10 segundos de Noite (Simula o Período Escuro de 8h-> depois meter os 8*3600*1000 para passar a 8horas)

// Variáveis para controlar tempo (para não bloquear)
unsigned long tempoAnterior = 0; 
bool eDia = true; //bool ou é true ou falso

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inicializar o barramento I2C nos pinos da TTGO (Partilhados pelo MCP e BH1750)
  Wire.begin(21, 22); 
  delay(500);

  // Inicializar do Expansor I2C
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("ERRO CRÍTICO: MCP23017 nao encontrado no barramento I2C!");
    while (1);
  }

  // Inicializar o Sensor de Luz
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("--- BH1750: Sensor de Luminosidade Configurado ---");
  } else {
    Serial.println("ERRO CRÍTICO: Sensor BH1750 nao encontrado!");
    while (1);
  }

  //Configurar Atuador (Pino A3 do MCP -> Canal IN4 do Relé)
  mcp.pinMode(3, OUTPUT);
  mcp.digitalWrite(3, HIGH); // começa desligado (Active-Low)

  Serial.println("---  Subsistema de Fotoperíodo e Iluminação ativado---");
  tempoAnterior = millis();
}

void loop() {
  unsigned long tempoAtual = millis();
  
  // Leitura digital contínua da quantidade de fotões (lumens) que a planta está a receber
  float lux = lightMeter.readLightLevel();

  
  // máquina de estados: controlo do horário dia -> noite
  
  if (eDia) {
    //  dia: para imitar o estado dia os LEDs devem estar ACESOS
    mcp.digitalWrite(3, LOW); //  Ativa o Relé (LEDs ON)
    
    Serial.print("[JANELA ATIVA - DIA] Luminosidade Alvo: "); 
    Serial.print(lux); 
    Serial.println(" lx");

    // VALIDAÇAO- deteção de falhas
    // Se o código diz para ligar, mas o sensor lê escuridão (menos de 50 lx), há falha física
    if (lux < THRESHOLD_LUZ) {
      Serial.println("!!! ALERTA DE HARDWARE: LEDs em falha física ou fonte sem energia durante Janela Activa !!!");
    }

    // Gestão da transição do tempo do Dia para a Noite (Passaram-se os 20 segundos/16horas)
    if (tempoAtual - tempoAnterior >= DURACAO_DIA) {
      Serial.println("\n--- Transicao Horaria: A terminar Janela Activa -> A iniciar Janela de Descanso (Noite) ---");
      eDia = false;
      tempoAnterior = tempoAtual;
    }
  } 
  else {
    // noite: para simular a noite os LEDs devem estar APAGADOS
    mcp.digitalWrite(3, HIGH); // Envia 5V para o IN4 -> Corta o Relé (LEDs OFF)
    
    Serial.print("[JANELA DE DESCANSO - NOITE] Luminosidade Alvo: "); 
    Serial.print(lux); 
    Serial.println(" lx (Periodo Escuro Regulamentar)");

    // Gestão da transição de tempo da Noite para o Dia (Passaram-se os 10 segundos/8horas)
    if (tempoAtual - tempoAnterior >= DURACAO_NOITE) {
      Serial.println("\n--- Transicao Horaria: A terminar Janela de Descanso -> A iniciar Janela Activa (Dia) ---");
      eDia = true;
      tempoAnterior = tempoAtual;
    }
  }

  delay(2000); // Ler de 2 em 2 segundos 
}
