#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_MCP23X17.h>

Adafruit_BME280 bme;
Adafruit_MCP23X17 mcp;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  Serial.println("Sistema de atuação: Regulação Térmica e Circulação de Ar (Sensor Ambiente -> MODO VENTOINHA) :IN1 ");

  // Inicializar o sensor BME280 (0x77)
  if (!bme.begin(0x77)) {
    Serial.println("Erro no BME280! Verifica as ligacoes.");
    while (1);
  }

  // Inicializar o expansor MCP23017 (0x20)
  if (!mcp.begin_I2C(0x20)) {
    Serial.println("Erro no MCP23017! Verifica os pinos.");
    while (1);
  }

  // Definir o pino 0 para a saída
  mcp.pinMode(0, OUTPUT); 
  mcp.digitalWrite(0, HIGH); // Começar desligado
  
  Serial.println("Pronto para monitorizar temperatura...");
}

void loop() {
  float temp = bme.readTemperature(); // LER TEMPERATURA DO BME280
  float hum  = bme.readHumidity(); // LER A HUMIDADE DO BME280
  
  Serial.print("Temperatura atual: ");
  Serial.print(temp);
  Serial.print(" C | Humidade (relativa) atual: ");
  Serial.print(hum);
  Serial.println(" %");


  // Liga se a temperatura for superior a 22 graus OU se a humidade for superior a 70%
  if (temp > 22 || hum > 70) { 
    Serial.println("ALERTA: Limites excedidos! Ventoinha ON (IN1).");
    mcp.digitalWrite(0, LOW);  // Ativar o Relé (Ativa por nível baixo, low)
  } 
  // Desliga se a temperatura for inferior a 18 graus E a humidade for inferior a 50%
  else if (temp < 18 && hum < 50) { // modo de conservação
    Serial.println("Temperatura e Humidade OK. Ventoinha OFF.");
    mcp.digitalWrite(0, HIGH); // Desligar Relé
  }

  delay(2000); // Espera 2 segundos para a próxima leitura
}
