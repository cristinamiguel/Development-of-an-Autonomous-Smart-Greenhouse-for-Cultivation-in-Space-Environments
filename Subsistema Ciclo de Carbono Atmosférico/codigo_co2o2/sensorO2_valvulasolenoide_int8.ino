#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// configurar expansor
Adafruit_MCP23X17 mcp;


const int PINO_EXP_VALV_CO2 = 7; // Pino A7 do expansor (para o IN8 do Relé)

// Variável de controlo -> para demonstração
float o2Simulado_Demo = 20.93; // começa no valor ideal que medi (1100 ppm)
bool modoDemoAtivo = true;     // altera para "false" se quiser as leituras reais brutas

void setup() {
  Serial.begin(115200);
  
  // Inicializar o barramento I2C e o Expansor MCP23017
  if (!mcp.begin_I2C()) {
    Serial.println("-> ERRO: Expansor MCP23017 não encontrado!");
    while (1); 
  }

  // Configurar o pino da válvula como saída e garantir que começa fechada (HIGH)
  mcp.pinMode(PINO_EXP_VALV_CO2, OUTPUT);   
  mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Fio no NO do relé: HIGH desliga o relé -> Válvula fecha

  Serial.println("\n_________________________________________________________________");
  Serial.println("--- SUBSISTEMA DE CO2: MODO DE DEMONSTRAÇÃO COM VÁLVULA ATIVA ---");
  Serial.println("_________________________________________________________________");
}

void loop() {
  Serial.println("\n--- Nova Leitura dos níveis atmosféricos ---");

  // Leitura real do sensor
  float leituraO2Real = 20.93; // Base estável do interior do quarto (janela aberta)
  // Aqui a biblioteca lê o valor real

  float leituraO2Final = leituraO2Real;

  // Lógica de modo de demonstração (Igual ao que fiz no algoritmo do subsistema de pH)
  if (modoDemoAtivo) {
    Serial.println("-> [MODO DEMONSTRAÇÃO ATIVO]: A forçar flutuação para exibir o atuador em funcionamento.");
    leituraO2Final = o2Simulado_Demo;
  }

  // Fórmula de conversão de CO2 para O2 (Inverso do que ocorre na Fotossíntese)
  float co2EstimadoPPM = 400.0 + (21.0 - leituraO2Final) * 10000.0;
  if (co2EstimadoPPM < 0) co2EstimadoPPM = 400.0;

  Serial.print("-> Oxigénio Atmosférico em Exibição: "); Serial.print(leituraO2Final, 2); Serial.println("%");
  Serial.print("-> Dióxido de Carbono Calculado: "); Serial.print(co2EstimadoPPM, 0); Serial.println(" ppm");

 
 // lógica de controlo (como estava no relatório intemédio) com ciclos para demonstração
  if (co2EstimadoPPM < 800) {
    Serial.println("-> ALERTA TR1: CO2 < 800 ppm! Abrindo Válvula Solenoide (LED IN8 ON)...");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, LOW);  
    delay(4000); // Reduzi para 4 segundos para exigir menos tempo de esforço do microcontrolador
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); 
    Serial.println("-> Injeção concluída. Válvula selada.");
    
    if (modoDemoAtivo) o2Simulado_Demo = 20.93; // Próximo ciclo vai para Estável (1100 ppm)
  } 
  else if (co2EstimadoPPM >= 1200) {
    Serial.println("-> CRÍTICO: CO2 >= 1200 ppm. Garantindo válvula FECHADA (LED IN8 OFF).");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
    
    if (modoDemoAtivo) o2Simulado_Demo = 20.97; 
    // Isto vai dar exatamente 700 ppm. Um valor ligeiramente abaixo dos 800, 
    // ativa o relé 
  } 
  else {
    Serial.println("-> Nível de CO2 estável na meta [800 - 1200] ppm. Válvula em repouso.");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH);
    
    if (modoDemoAtivo) o2Simulado_Demo = 20.89; 
    // Isto vai dar exatamente 1500 ppm. O suficiente para testar o valor crítico sem esforço em demasia
  }}
