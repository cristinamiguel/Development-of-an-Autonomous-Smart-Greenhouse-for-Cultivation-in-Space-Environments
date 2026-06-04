#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MCP23X17.h>
#include <BH1750.h>
#include <DFRobot_OxygenSensor.h>

Adafruit_MCP23X17 mcp;
Adafruit_BME280 bme;
BH1750 lightMeter;
DFRobot_OxygenSensor oxygen;
#define ENDERECO_I2C_O2 0x73

#define PH_PIN 35
#define SOLO_PIN 34
const int PINO_MOSFET = 25;

const int PINO_EXP_VENTILACAO = 0;
const int PINO_EXP_ILUMINACAO = 3;
const int PINO_EXP_ACIDO      = 4;
const int PINO_EXP_BASE       = 5;
const int PINO_EXP_VALV_CO2   = 7;

#define SDA_PIN 21
#define SCL_PIN 22

// -------------------------------------------------------------------------
// I2C RECOVERY - VERSÃO FINAL
// -------------------------------------------------------------------------
bool falhaGeralI2C = false;

void recuperarI2C() {
  Serial.println("!!! [I2C RECOVERY] Barramento travado. A recuperar...");
  Wire.end();
  delay(10);
  pinMode(SCL_PIN, OUTPUT);
  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, HIGH);
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL_PIN, LOW);  delayMicroseconds(5);
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
  }
  digitalWrite(SDA_PIN, LOW);  delayMicroseconds(5);
  digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
  digitalWrite(SDA_PIN, HIGH); delayMicroseconds(5);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(25);
  delay(100);

  if (!mcp.begin_I2C(0x20)) {
    Serial.println("!!! [RECOVERY] MCP23017 não respondeu.");
  } else {
    mcp.pinMode(PINO_EXP_VENTILACAO, OUTPUT); mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH);
    mcp.pinMode(PINO_EXP_ILUMINACAO, OUTPUT); mcp.digitalWrite(PINO_EXP_ILUMINACAO, HIGH);
    mcp.pinMode(PINO_EXP_ACIDO,      OUTPUT); mcp.digitalWrite(PINO_EXP_ACIDO,      HIGH);
    mcp.pinMode(PINO_EXP_BASE,       OUTPUT); mcp.digitalWrite(PINO_EXP_BASE,       HIGH);
    mcp.pinMode(PINO_EXP_VALV_CO2,   OUTPUT); mcp.digitalWrite(PINO_EXP_VALV_CO2,   HIGH);
    Serial.println("-> [RECOVERY] MCP23017 recuperado.");
  }
  bme.begin(0x77);
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  falhaGeralI2C = false;
  Serial.println("-> [RECOVERY] Concluído. A retomar operação normal.");
}

// -------------------------------------------------------------------------
// TIME-SLICING
// -------------------------------------------------------------------------
unsigned long tempoAtual = 0;

const unsigned long INTERVALO_CLIMA     = 5000;
const unsigned long INTERVALO_LUZ       = 11000;
const unsigned long INTERVALO_SOLO      = 17000;
const unsigned long INTERVALO_PH        = 23000;
const unsigned long INTERVALO_ATMOSFERA = 29000;

unsigned long ultimoTempoClima     = 0;
unsigned long ultimoTempoLuz       = 0;
unsigned long ultimoTempoSolo      = 0;
unsigned long ultimoTempoPH        = 0;
unsigned long ultimoTempoAtmosfera = 0;

// -------------------------------------------------------------------------
// SUBSISTEMA 1
// -------------------------------------------------------------------------
const float LIMITE_TEMP_ALTA = 22.0;
const float LIMITE_HUM_ALTA  = 70.0;
const float LIMITE_TEMP_OK   = 20.0;
const float LIMITE_HUM_OK    = 60.0;
bool ventoinhaAtiva = false;

// -------------------------------------------------------------------------
// SUBSISTEMA 2
// -------------------------------------------------------------------------
const float THRESHOLD_LUZ         = 50.0;
const unsigned long DURACAO_DIA   = 20000;
const unsigned long DURACAO_NOITE = 10000;
unsigned long tempoTransicaoLuz   = 0;
bool eDia = true;

// -------------------------------------------------------------------------
// SUBSISTEMA 3
// -------------------------------------------------------------------------
const int Valor_Seco = 2500;
const int Valor_Agua = 1600;
bool regaEmCurso = false;
unsigned long tempoInicioRega = 0;
const unsigned long TEMPO_REGA = 4000;
int leituraSoloAnterior   = -1;
int contadorCongeladoSolo = 0;
bool modoDemoSoloAtivo    = false;
int humidadeSimulada_Demo = 55;

// -------------------------------------------------------------------------
// SUBSISTEMA 4
// -------------------------------------------------------------------------
const float VOLTAGEM_PH7 = 1.61;
const float DECLIVE_PH   = 5.66;
float phSimulado_Demo    = 8.2;
int leituraPHAnterior    = -1;
int contadorCongeladoPH  = 0;
bool modoDemoPHAtivo     = false;

bool bombaAcidoAtiva = false;
bool bombaBaseAtiva  = false;
unsigned long tempoInicioBombaAcido = 0;
unsigned long tempoInicioBombaBase  = 0;
const unsigned long TEMPO_BOMBA_PH  = 2000;

// -------------------------------------------------------------------------
// SUBSISTEMA 5
// -------------------------------------------------------------------------
float o2Simulado_Demo    = 21.05;
bool sensorO2Operacional = false;
bool valvulaAtiva        = false;
unsigned long tempoInicioValvula = 0;

// -------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);

  pinMode(PH_PIN, INPUT);
  pinMode(SOLO_PIN, INPUT);
  pinMode(PINO_MOSFET, OUTPUT);
  digitalWrite(PINO_MOSFET, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(25);
  delay(500);

  if (!mcp.begin_I2C(0x20)) { Serial.println("ERRO: MCP23017!"); while (1); }
  if (!bme.begin(0x77))     { Serial.println("ERRO: BME280!");   while (1); }
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("ERRO: BH1750!"); while (1);
  }
  if (oxygen.begin(ENDERECO_I2C_O2)) {
    Serial.println("--- Sensor O2 Operacional ---");
    sensorO2Operacional = true;
  } else {
    Serial.println("!!! Sensor O2 ausente. Modo Simulação.");
    sensorO2Operacional = false;
  }

  mcp.pinMode(PINO_EXP_VENTILACAO, OUTPUT); 
  mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH);
  mcp.pinMode(PINO_EXP_ILUMINACAO, OUTPUT); 
  mcp.digitalWrite(PINO_EXP_ILUMINACAO, HIGH);
  mcp.pinMode(PINO_EXP_ACIDO,      OUTPUT); 
  mcp.digitalWrite(PINO_EXP_ACIDO,      HIGH);
  mcp.pinMode(PINO_EXP_BASE,       OUTPUT); 
  mcp.digitalWrite(PINO_EXP_BASE,       HIGH);
  mcp.pinMode(PINO_EXP_VALV_CO2,   OUTPUT); 
  mcp.digitalWrite(PINO_EXP_VALV_CO2,   HIGH);

  tempoTransicaoLuz    = millis();
  ultimoTempoClima     = millis() + 1000;
  ultimoTempoLuz       = millis() + 3000;
  ultimoTempoSolo      = millis() + 5000;
  ultimoTempoPH        = millis() + 7000;
  ultimoTempoAtmosfera = millis() + 9000;

  Serial.println("\n__________________________________________________________");
  Serial.println("--- Sistema Integrado (v3 - I2C Recovery sem Watchdog) ---");
  Serial.println("__________________________________________________________");
}

// -------------------------------------------------------------------------
// Loop Principal
// -------------------------------------------------------------------------
void loop() {
  tempoAtual = millis();

  if (falhaGeralI2C) {
      recuperarI2C();
  }

  // Desactivacao temporizada das bombas de pH e CO2
  if (bombaAcidoAtiva && (tempoAtual - tempoInicioBombaAcido >= TEMPO_BOMBA_PH)) {
    mcp.digitalWrite(PINO_EXP_ACIDO, HIGH);
    bombaAcidoAtiva = false;
    Serial.println("-> [pH] Bomba de Ácido desligada.");
  }
  if (bombaBaseAtiva && (tempoAtual - tempoInicioBombaBase >= TEMPO_BOMBA_PH)) {
    mcp.digitalWrite(PINO_EXP_BASE, HIGH);
    bombaBaseAtiva = false;
    Serial.println("-> [pH] Bomba de Base desligada.");
  }
  if (valvulaAtiva && (tempoAtual - tempoInicioValvula >= 4000)) {
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
    valvulaAtiva = false;
    Serial.println("-> [CO2] Válvula selada.");
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 1: Térmico
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoClima >= INTERVALO_CLIMA && !regaEmCurso) {
    ultimoTempoClima = tempoAtual;
    Serial.println("\n[SUBSISTEMA 1 - TÉRMICO & CIRCULAÇÃO]");

    float temp = bme.readTemperature();
    float hum  = bme.readHumidity();
    
    if (isnan(temp)) falhaGeralI2C = true;

    if (temp > 150.0 || temp < -40.0 || isnan(temp)) {
      Serial.println("-> [AVISO]: BME280 com leitura inválida.");
    } else {
      Serial.print("-> Temp: "); 
      Serial.print(temp, 1);
      Serial.print(" C | Hum: "); 
      Serial.print(hum, 1); 
      Serial.println(" %");

      if (!ventoinhaAtiva && (temp > LIMITE_TEMP_ALTA || hum > LIMITE_HUM_ALTA)) {
        Serial.println("-> ALERTA: Limites excedidos! A ativar a ventoinha...");
        mcp.digitalWrite(PINO_EXP_VENTILACAO, LOW);
        ventoinhaAtiva = true;
      } else if (ventoinhaAtiva && temp < LIMITE_TEMP_OK && hum < LIMITE_HUM_OK) {
        Serial.println("-> Clima normalizado. A desligar a ventoinha.");
        mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH);
        ventoinhaAtiva = false;
      } else if (ventoinhaAtiva) {
        Serial.println("-> Ventoinha ativa (aguardar normalização).");
      } else {
        Serial.println("-> Clima OK.");
      }
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 2: Fotoperíodo
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoLuz >= INTERVALO_LUZ) {
    ultimoTempoLuz = tempoAtual;
    Serial.println("\n[SUBSISTEMA 2 - ILUMINAÇÃO]");

    float lux = lightMeter.readLightLevel();

    if (eDia) {
      mcp.digitalWrite(PINO_EXP_ILUMINACAO, LOW);
      Serial.print("-> [DIA] Luminosidade: "); 
      Serial.print(lux, 1); ~
      Serial.println(" lx");
      if (lux < THRESHOLD_LUZ) Serial.println("!!! ALERTA: LEDs em falha !!!");
      if (tempoAtual - tempoTransicaoLuz >= DURACAO_DIA) {
        Serial.println("-> Transição: NOITE...");
        eDia = false; tempoTransicaoLuz = tempoAtual;
      }
    } else {
      mcp.digitalWrite(PINO_EXP_ILUMINACAO, HIGH);
      Serial.print("-> [NOITE] Luminosidade: "); 
      Serial.print(lux, 1); 
      Serial.println(" lx");
      if (tempoAtual - tempoTransicaoLuz >= DURACAO_NOITE) {
        Serial.println("-> Transição: DIA...");
        eDia = true; tempoTransicaoLuz = tempoAtual;
      }
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 3: Solo
  // -----------------------------------------------------------------------
  if (!regaEmCurso) {
    if (tempoAtual - ultimoTempoSolo >= INTERVALO_SOLO) {
      ultimoTempoSolo = tempoAtual;
      Serial.println("\n[SUBSISTEMA 3 - HUMIDADE DO SOLO]");

      int leituraSoloReal = analogRead(SOLO_PIN);
      if (leituraSoloReal == leituraSoloAnterior) {
        contadorCongeladoSolo++;
        if (contadorCongeladoSolo >= 3) modoDemoSoloAtivo = true;
      } else {
        contadorCongeladoSolo = 0; modoDemoSoloAtivo = false;
        leituraSoloAnterior = leituraSoloReal;
      }

      int humidadeSolo = 0;
      if (!modoDemoSoloAtivo) {
        humidadeSolo = map(leituraSoloReal, Valor_Seco, Valor_Agua, 0, 100);
        humidadeSolo = constrain(humidadeSolo, 0, 100);
        Serial.print("-> [Real] Humidade: "); Serial.print(humidadeSolo); Serial.println("%");
      } else {
        humidadeSolo = humidadeSimulada_Demo;
        Serial.print("-> [DEMO] Humidade: "); Serial.print(humidadeSolo); Serial.println("%");
      }

      // Alterei aqui: Forcei a rega se a humidade for < 40 ou se o sensor estiver congelado (>= 3 significa 4 leituras iguais)
      if (humidadeSolo < 40 || contadorCongeladoSolo >= 3) {
        if (contadorCongeladoSolo >= 3) {
          Serial.println("-> ALERTA: Sensor congelado (4 leituras iguais)! Forçando Bomba...");
        } else {
          Serial.println("-> Solo Seco! Ativando Bomba...");
        }
        digitalWrite(PINO_MOSFET, HIGH);
        regaEmCurso = true; tempoInicioRega = tempoAtual;
        if (modoDemoSoloAtivo) humidadeSimulada_Demo = 75;
        
        // Zera o contador para não ficar num ciclo infinito forçado
        contadorCongeladoSolo = 0; 
      } else if (humidadeSolo >= 70) {
        digitalWrite(PINO_MOSFET, LOW);
        if (modoDemoSoloAtivo) humidadeSimulada_Demo = 35;
      }
    }
  } else {
    if (tempoAtual - tempoInicioRega >= TEMPO_REGA) {
      digitalWrite(PINO_MOSFET, LOW);
      regaEmCurso = false;
      leituraSoloAnterior = -1;
      contadorCongeladoSolo = 0;
      ultimoTempoSolo = tempoAtual;
      Serial.println("-> Irrigação concluída.");
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 4: pH
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoPH >= INTERVALO_PH && !regaEmCurso
      && !bombaAcidoAtiva && !bombaBaseAtiva) {
    ultimoTempoPH = tempoAtual;
    Serial.println("\n[SUBSISTEMA 4 - ESTABILIZAÇÃO DE pH]");

    int leituraPHReal = 0;
    for (int i = 0; i < 10; i++) { leituraPHReal += analogRead(PH_PIN); delay(10); }
    leituraPHReal /= 10;

    if (leituraPHReal == leituraPHAnterior) {
      contadorCongeladoPH++;
      if (contadorCongeladoPH >= 3) modoDemoPHAtivo = true;
    } else {
      contadorCongeladoPH = 0; modoDemoPHAtivo = false;
      leituraPHAnterior = leituraPHReal;
    }

    if (modoDemoPHAtivo) {
      Serial.print("-> [DEMO] pH: "); Serial.println(phSimulado_Demo, 2);
      if (phSimulado_Demo > 6.5) {
        Serial.println("-> pH Alto! Ativar Ácido 2s...");
        mcp.digitalWrite(PINO_EXP_ACIDO, LOW);
        bombaAcidoAtiva = true; tempoInicioBombaAcido = tempoAtual;
        phSimulado_Demo -= 0.5;
      } else if (phSimulado_Demo < 5.5) {
        Serial.println("-> pH Baixo! Ativar Base 2s...");
        mcp.digitalWrite(PINO_EXP_BASE, LOW);
        bombaBaseAtiva = true; tempoInicioBombaBase = tempoAtual;
        phSimulado_Demo += 0.4;
      } else {
        Serial.println("-> pH estabilizado. Bombas em repouso.");
        phSimulado_Demo = 8.2;
      }
    } else {
      float voltagemPH  = leituraPHReal * (3.3 / 4095.0);
      float valorPHReal = 7.0 + (VOLTAGEM_PH7 - voltagemPH) * DECLIVE_PH;
      valorPHReal = constrain(valorPHReal, 0.0, 14.0);
      Serial.print("-> [Real] "); 
      Serial.print(voltagemPH); 
      Serial.print("V | pH: ");
      Serial.println(valorPHReal, 2);

      if (valorPHReal > 6.5) {
        Serial.println("-> pH Alto. Dosear Ácido 2s...");
        mcp.digitalWrite(PINO_EXP_ACIDO, LOW);
        bombaAcidoAtiva = true; tempoInicioBombaAcido = tempoAtual;
      } else if (valorPHReal < 5.5) {
        Serial.println("-> pH Baixo. Dosear Base 2s...");
        mcp.digitalWrite(PINO_EXP_BASE, LOW);
        bombaBaseAtiva = true; 
        tempoInicioBombaBase = tempoAtual;
      } else {
        Serial.println("-> pH estável.");
      }
    }
  }

  // -----------------------------------------------------------------------
  // SUBSISTEMA 5: CO2
  // -----------------------------------------------------------------------
  if (tempoAtual - ultimoTempoAtmosfera >= INTERVALO_ATMOSFERA
      && !regaEmCurso && !valvulaAtiva) {
    ultimoTempoAtmosfera = tempoAtual;
    Serial.println("\n[SUBSISTEMA 5 - ATMOSFERA E GESTÃO DE CO2]");

    float o2Lido = sensorO2Operacional ? oxygen.getOxygenData(20) : o2Simulado_Demo;

    float co2EstimadoPPM = 400.0 + (21.0 - o2Lido) * 10000.0;
    if (co2EstimadoPPM < 0) co2EstimadoPPM = 400.0;
    Serial.print("-> CO2: "); Serial.print(co2EstimadoPPM, 0); Serial.println(" ppm");

    if (co2EstimadoPPM < 800) {
      Serial.println("-> CO2 baixo! Abrindo válvula 4s...");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, LOW);
      valvulaAtiva = true; tempoInicioValvula = tempoAtual;
      if (!sensorO2Operacional) o2Simulado_Demo = 20.93;
    } else if (co2EstimadoPPM >= 1200) {
      Serial.println("-> CO2 em excesso! Extração forçada...");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
      mcp.digitalWrite(PINO_EXP_VENTILACAO, LOW);
      ventoinhaAtiva = true;
      if (!sensorO2Operacional) o2Simulado_Demo = 21.05;
    } else {
      Serial.println("-> CO2 estável [800-1200 ppm].");
      mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
      if (!ventoinhaAtiva) mcp.digitalWrite(PINO_EXP_VENTILACAO, HIGH);
      if (!sensorO2Operacional) o2Simulado_Demo = 20.85;
    }
  }
}
