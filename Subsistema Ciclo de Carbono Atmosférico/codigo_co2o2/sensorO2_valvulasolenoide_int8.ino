#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// configurar expansor
Adafruit_MCP23X17 mcp;

const int PINO_EXP_VALV_CO2 = 7; // Pino A7 do expansor (para o IN8 do Relé)

// Variáveis de controlo para passar para a Demonstração
bool modoDemoAtivo = false; 
int contadorEstavel = 0;       // Conta quantas leituras seguidas dão estáveis
int passoDemo = 1;             // Controla as fases da demonstração forçada
float o2Simulado_Demo = 21.05; // Valor inicial da demo para forçar CO2 baixo (0 ppm)

void setup() {
  Serial.begin(115200);
  
  // Inicializar o Expansor MCP23017
  if (!mcp.begin_I2C()) {
    Serial.println("-> ERRO: Expansor MCP23017 não encontrado!");
    while (1); 
  }

  // Configurar o pino da válvula como saída e garantir que começa fechada
  mcp.pinMode(PINO_EXP_VALV_CO2, OUTPUT);   
  mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // HIGH mantém o relé desligado -> Válvula selada
}

void loop() {
  Serial.println("\n--- Nova Leitura dos níveis atmosféricos ---");

  float leituraO2Final;

  if (!modoDemoAtivo) {
    //Leitura Real (Ambiente equilibrado)
    float leituraO2Real = 20.93; 
    leituraO2Final = leituraO2Real;
  } else {
    // Demonstração Ativada
    Serial.print("-> [Aviso]: Sistema Real estável há muito tempo. MODO DEMO ATIVO (Passo ");
    Serial.print(passoDemo); Serial.println(")");
    leituraO2Final = o2Simulado_Demo;
  }

  // Fórmula matemática de conversão inversa de o2 para co2 -> contrario da fotossintese
  float co2EstimadoPPM = 400.0 + (21.0 - leituraO2Final) * 10000.0;
  if (co2EstimadoPPM < 0) co2EstimadoPPM = 400.0;

  // Mostra os valores iniciais lidos/calculados no monitor
  Serial.print("-> Oxigénio Atmosférico Lido: "); 
  Serial.print(leituraO2Final, 2); 
  Serial.println("%");
  Serial.print("-> Dióxido de Carbono Calculado: "); 
  Serial.print(co2EstimadoPPM, 0); 
  Serial.println(" ppm");

  // -----------------------------------------------------------------------
  // Llógica do intervalo [800 - 1200] ppm 
  // -----------------------------------------------------------------------
  if (co2EstimadoPPM < 800) { 
    contadorEstavel = 0; // Reset 
    
    Serial.println("-> ALERTA: CO2 < 800 ppm! Abrindo Válvula Solenoide (LED IN8 ON)...");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, LOW);  // Abrir a válvula (Liga o LED do INT8)
    delay(4000);                               // Injetar por 4 segundos
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Fechar a válvula (Desliga o LED do INT8)
    Serial.println("-> Injeção concluída. Válvula selada.");

    if (modoDemoAtivo && passoDemo == 1) {
      o2Simulado_Demo = 1000.0; // Configura o passo seguinte da demo (Ideal)
      passoDemo = 2;
    }
  } 
  else if (co2EstimadoPPM >= 1200) {
    contadorEstavel = 0; // Reset 
    
    Serial.println("-> CRÍTICO: CO2 >= 1200 ppm. Garantindo válvula FECHADA (LED IN8 OFF).");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Trancar a válvula por segurança

    if (modoDemoAtivo && passoDemo == 3) {
      o2Simulado_Demo = 21.05; // Reiniciar o ciclo de demonstração
      passoDemo = 1;
    }
  } 
  else {
    // nível estável (Entre 800 e 1200 ppm)
    Serial.println("-> Nível de CO2 estável na meta [800 - 1200] ppm. Válvula em repouso.");
    mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Válvula fechada e segura

    if (!modoDemoAtivo) {
      contadorEstavel++;
      Serial.print("   [Tentativas estáveis seguidas: "); 
      Serial.print(contadorEstavel); 
      Serial.println("/4]");
      
    
      if (contadorEstavel >= 4) {
        modoDemoAtivo = true;
        o2Simulado_Demo = 21.05; 
        passoDemo = 1;
        
        // Força IMEDIATAMENTE a abertura do relé na quarta tentativa
        Serial.println("\n-> [GATILHO DE SEGURANÇA]: Limite de estabilidade atingido!");
        Serial.println("-> ALERTA(FORÇADO): Força-se CO2 a ser 0 ppm para demonstrar atuador...");
        Serial.println("-> ALERTA: CO2 < 800 ppm! A abrir a Válvula Solenoide (LED IN8 ON)...");
        
        mcp.digitalWrite(PINO_EXP_VALV_CO2, LOW);  
        delay(4000);                               // Mantém ativo por 4 segundos
        mcp.digitalWrite(PINO_EXP_VALV_CO2, HIGH); // Desliga
        
        Serial.println("-> Injeção concluída. Válvula selada.");
        
        // Prepara o passo 2 para a leitura seguinte
        o2Simulado_Demo = 1000.0; 
        passoDemo = 2;
      }
    } else {
      // Se já estiver em modo demo e passou pelo estado ideal, força-se o crítico a seguir
      if (passoDemo == 2) {
        o2Simulado_Demo = 20.89; // Próximo ciclo força valor crítico (1500 ppm)
        passoDemo = 3;
      }
    }
  }

  delay(3000); // Intervalo de 3 segundos
}
