#include <Wire.h>
#include <Adafruit_MCP23X17.h> 

#define PH_PIN 35 // sonda de pH ligada diretamente no pino IO35 dp TTGO


Adafruit_MCP23X17 mcp;


const int PINO_EXP_ACIDO = 4; // Pino A4 -> IN5 do Módulo de Relés (Bomba Peristáltica Ácido / pH Down)
const int PINO_EXP_BASE  = 5; // Pino A5 -> IN6 do Módulo de Relés (Bomba Peristáltica Base / pH Up)

// Valores que me deram na calibraçai
const float VOLTAGEM_PH7 = 1.61; 
const float DECLIVE_PH = 5.66; 

// Caso a sonda fixe congelada nos 2.05V o código seguinte serve para demonstrar os atuadores.
// Assim mesmo que o sensor não colabore consigo demonstrar na mesma que o circuito consegue ativar os atuadores (bombas peristálticas)
float phSimulado_Demo = 8.2; //isto é um valor básico logo ativo a bomba que está no int 4 ligado à solucao ácida (pH=4.0)

void setup() {
  Serial.begin(115200);
  analogReadResolution(12); // Definição de 12 bits (0-4095) para a ESP32
  
  pinMode(PH_PIN, INPUT); // Configura o pino 35 da TTGO como entrada

  // Inicializar expansor
  if (!mcp.begin_I2C()) {
    Serial.println("-> ERRO: Expansor MCP23017 não encontrado! Verifica os cabos SDA/SCL.");
    while (1); 
  }

  // Configurar os pinos do Expansor como saída
  // Como a maioria dos relés ativa em LOW, começa-se em HIGH para estarem desligados
  mcp.pinMode(PINO_EXP_ACIDO, OUTPUT);
  mcp.digitalWrite(PINO_EXP_ACIDO, HIGH); 
  
  mcp.pinMode(PINO_EXP_BASE, OUTPUT);
  mcp.digitalWrite(PINO_EXP_BASE, HIGH);

  delay(1500);
  Serial.println("\n--- Subsistema de controlo de pH ativo ---");
}

void loop() {
  Serial.println("\n==========================================");

  //Leio o valor e filtro-o ao fazer a média de 10 amostras
  int leituraPH = 0;
  for(int i=0; i<10; i++) { 
    leituraPH += analogRead(PH_PIN); 
    delay(10); 
  }
  leituraPH /= 10;

  // Conversão para tensão e cálculo do pH Real
  float voltagemPH = leituraPH * (3.3 / 4095.0);
  float valorPHReal = 7.0 + (VOLTAGEM_PH7 - voltagemPH) * DECLIVE_PH;
  valorPHReal = constrain(valorPHReal, 0.0, 14.0);

  // Lógica de controlo (REAL VS MODO DEMONSTRACAO)
  
  // se a sonda ficar presa num valor é ativado o modo demonstração
  if (voltagemPH >= 2.02 && voltagemPH <= 2.07) {
    Serial.print("[DETEÇÃO] Sonda física fixa em "); Serial.print(voltagemPH); Serial.println("V.");
    Serial.print("[pH SYSTEM - DEMO] pH Atual no Copo: "); Serial.println(phSimulado_Demo, 2);
    
    // Se estiver muito Alcalino (> 6.5) -> Ativa o Ácido (IN5 / A4)
    if (phSimulado_Demo > 6.5) {
      Serial.println("-> ALERTA: pH Alto! Ativando A4 (IN5 - Bomba de Ácido)...");
      mcp.digitalWrite(PINO_EXP_ACIDO, HIGH); // Liga o Relé 5
      delay(2000);                           // Injeta por 2 segundos
      mcp.digitalWrite(PINO_EXP_ACIDO, LOW); // Desliga o Relé 5
      
      phSimulado_Demo -= 0.5; // Simula a descida do pH para o próximo ciclo
      Serial.println("-> Dose de pH Down aplicada.");
    } 
    // Se estiver muito Ácido (< 5.5) -> Ativa a Base (IN6 / A5)
    else if (phSimulado_Demo < 5.5) {
      Serial.println("-> ALERTA: pH Baixo! Ativando A5 (IN6 - Bomba de Base)...");
      mcp.digitalWrite(PINO_EXP_BASE, LOW);  // Liga o Relé 6
      delay(2000);
      mcp.digitalWrite(PINO_EXP_BASE, HIGH); // Desliga o Relé 6
      
      phSimulado_Demo += 0.4; // Simula a subida do pH
      Serial.println("-> Dose de pH Up aplicada.");
    } 
    else {
      Serial.println("-> pH Estabilizado em 6.0 (Ideal). Bombas em repouso.");
      delay(2000);
      phSimulado_Demo = 8.2; // Faz reset ao valor da demonstração para mostrar o ciclo outra vez
    }
  } 
  
  // se a sonda não ficar congelado num valor -> uso o valor real dado pela sonda 
  else {
    Serial.print("[pH SYSTEM - REAL] Voltagem: "); Serial.print(voltagemPH); Serial.print("V");
    Serial.print(" | pH Calculado: "); Serial.println(valorPHReal, 2);
    
    if (valorPHReal > 6.5) {
      Serial.println("-> ALERTA: pH Alto! Ativando A4 (IN5 - Bomba de Ácido)...");
      mcp.digitalWrite(PINO_EXP_ACIDO, LOW);
      delay(2000);
      mcp.digitalWrite(PINO_EXP_ACIDO, HIGH);
    } 
    else if (valorPHReal < 5.5) {
      Serial.println("-> ALERTA: pH Baixo! Ativando A5 (IN6 - Bomba de Base)...");
      mcp.digitalWrite(PINO_EXP_BASE, LOW);
      delay(2000);
      mcp.digitalWrite(PINO_EXP_BASE, HIGH);
    } 
    else {
      Serial.println("-> pH dentro dos parâmetros ideais. Bombas em repouso.");
    }
  }

  delay(4000); // Aguardar 4 segundos antes da próxima verificação
}
