const int Valor_Seco = 2500;  
const int Valor_Agua = 1600;  

const int PINO_MOSFET = 25;                  // conexao do mosfet ao microcontrolador
const unsigned long TEMPO_REGA = 20000;      // 20 segundos de rega
const unsigned long PAUSA_LEITURA = 5000;    // 5 segundos de pausa de teste

void setup() {
  Serial.begin(115200);
  
  // para estabilizar
  delay(1500); 
  Serial.println("\n--- A iniciar subsistema de controlo de humidade de solo ---");

  // configurar mosfet no ttgo
  pinMode(PINO_MOSFET, OUTPUT);
  digitalWrite(PINO_MOSFET, LOW);            // Começo com ele desligado por precauçao

  // inicializar sensor analogico
  pinMode(34, INPUT); 
  
  // Limpa o canal analógico no boot
  for (int i = 0; i < 3; i++) { analogRead(34); delay(200); }
  
  Serial.println("--- SUBSISTEMA DE IRRIGAÇÃO CONFIGURADO DIRETAMENTE ---");
}

void loop() {
  int lecturaSolo = analogRead(34);

  int humidadeSolo = map(lecturaSolo, Valor_Seco, Valor_Agua, 0, 100);
  humidadeSolo = constrain(humidadeSolo, 0, 100);

  Serial.print("Leitura Bruta Real: "); Serial.print(lecturaSolo);
  Serial.print(" | Humidade: "); Serial.print(humidadeSolo);
  Serial.println("%");

  if (humidadeSolo < 40) { 
    Serial.println("-> ALERTA: Solo Seco! A iniciar fluxo via bomba submersível...");
    digitalWrite(PINO_MOSFET, HIGH);  // liga o mosfet
    delay(TEMPO_REGA);          
    digitalWrite(PINO_MOSFET, LOW);   // desliga o mosfet
    Serial.println("-> Irrigacao concluida. Aguardando...");
    delay(PAUSA_LEITURA);       
  }
  else if (humidadeSolo >= 70) { 
    Serial.println("-> Humidade Segura (>=70%). Bomba em repouso.");
    digitalWrite(PINO_MOSFET, LOW);   // manter desligado
  }

  delay(2000); 
}
