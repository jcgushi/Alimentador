#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <LittleFS.h>
#include "motbepled.h"

// ===== Configurações do AP =====
const char* AP_SSID = "ESP32C3_AP";
const char* AP_PASSWORD = "12345678"; // mínimo 8 caracteres


motbepled motores(1, 2); // 1 motor no modo 2


int velocidade = 8; // velocidade do motor
boolean cw = true; // sentido horário

// ===== Limites =====
#define MAX_TIMERS 20 // máximo de 20 timers programáveis

// ===== Estrutura de timer =====
struct TimerItem {
  int hour;
  int minute;
  int steps; // dose em passos
};

TimerItem timers[MAX_TIMERS];
int timerCount = 0;

WebServer server(80);

// Controle de disparo único por minuto
int lastProcessedMinute = -1;
int lastProcessedHour = -1;
int lastProcessedYday = -1;

// Status de rede
String localIp = ""; // IP obtido em modo STA
String apIp = "";    // IP do AP

// ===== Tempo (NTP) =====
void setupTimeNTP() {
  // Fuso: Brasil (UTC-3). Ajuste se necessário.
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}
// Ajusta hora manualmente
bool setManualTime(int hour, int minute) {
  struct tm t = {0};
  time_t now = time(nullptr);
  if (now == 0) {
    // base aproximada (1 Jan 2025)
    t.tm_year = 125; // 1900 + 125 = 2025
    t.tm_mon = 0;
    t.tm_mday = 1;
  } else {
    localtime_r(&now, &t);
  }
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;

  time_t newTime = mktime(&t);
  struct timeval tv = { .tv_sec = newTime, .tv_usec = 0 };
  return settimeofday(&tv, nullptr) == 0;
}
// ===== inicia motor =====
void setupMotor() {
  
  motores.pinsStep0(0, 1, 2, 3, -1, -1);
  motores.begin();
  // stepper.setMaxSpeed(1200);     // ajuste conforme fonte/motor
  // stepper.setAcceleration(300);  // aceleração
}
// Registra uma dose no arquivo report.txt
// event -> "INICIO" / "CONCLUIDO" (ou NULL para sem tag)
void logDoseEvent(int steps, const char *event) {
  struct tm info;
  if (getLocalTime(&info)) {
    File file = LittleFS.open("/report.txt", "a");
    if (file) {
      char buf[128];
      if (event && strlen(event) > 0) {
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d - %s: %d passos\n",
                 info.tm_mday, info.tm_mon + 1, info.tm_year + 1900,
                 info.tm_hour, info.tm_min, event, steps);
      } else {
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d - %d passos\n",
                 info.tm_mday, info.tm_mon + 1, info.tm_year + 1900,
                 info.tm_hour, info.tm_min, steps);
      }
      file.print(buf);
      file.close();
    }
  }
}

void logDoseStart(int steps) { logDoseEvent(steps, "INICIO"); }
void logDoseComplete(int steps) { logDoseEvent(steps, "CONCLUIDO"); }
// Estado da dose em andamento (monitorado por checkTimersAndTrigger/loop)
volatile bool motorDoseInProgress = false;
volatile uint32_t motorDosePendingSteps = 0;

// Fornece uma dose de ração (antiga — agora mantida como wrapper não recomendado)
void runMotorDose(int steps) {
  Serial.printf("Motor: dose de %d passos (dispatch antigo)\n", steps);
  motores.runStep(0, steps, velocidade, cw);
  logDoseStart(steps);
  motorDoseInProgress = true;
  motorDosePendingSteps = steps;
}

// Versão segura: valida parâmetros, evita sobreposição e protege a escrita para o ISR
// Retorna true se a dose foi agendada, false caso motor esteja ocupado ou parâmetro inválido
bool runMotorDoseSafe(uint32_t steps) {
  if (steps == 0) {
    Serial.println("Ignorado: steps == 0");
    return false;
  }
  // limite de segurança (evita execuções muito longas)
  const uint32_t MAX_STEPS = 2000000UL; // arbitrário — ajuste conforme seu motor
  if (steps > MAX_STEPS) {
    Serial.printf("Rejecting: steps (%lu) > MAX_STEPS (%lu)\n", steps, MAX_STEPS);
    return false;
  }

  // Se o motor já estiver em movimento, recusamos para evitar sobrescrever a tarefa
  if (motores.stepstogo(0) > 0) {
    Serial.println("Motor ocupado — recusando nova dose");
    return false;
  }

  uint8_t v = constrain(velocidade, 1, 255);
  noInterrupts();
  motores.runStep(0, steps, v, cw);
  interrupts();

  Serial.printf("Motor: dose agendada %lu passos\n", (unsigned long)steps);
  logDoseStart(steps);
  motorDoseInProgress = true;
  motorDosePendingSteps = steps;
  return true;
}
// Ordena os timers por hora e minuto (ordem crescente)
void sortTimers() {
  for (int i = 0; i < timerCount - 1; i++) {
    for (int j = i + 1; j < timerCount; j++) {
      if (timers[j].hour < timers[i].hour ||
         (timers[j].hour == timers[i].hour && timers[j].minute < timers[i].minute)) {
        // troca os elementos
        TimerItem tmp = timers[i];
        timers[i] = timers[j];
        timers[j] = tmp;
      }
    }
  }
}
// ===== Carregar e salvar timers em /timers.txt =====
bool parseLineToTimer(const String& line, TimerItem& out) {
  String l = line;// copia para manipular
  l.trim();
  if (l.length() == 0) return false;

  int sep1 = l.indexOf(':');
  int sep2 = l.indexOf(',');
  if (sep1 == -1 || sep2 == -1) return false;

  int h = l.substring(0, sep1).toInt();
  int m = l.substring(sep1 + 1, sep2).toInt();
  int s = l.substring(sep2 + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59 || s <= 0) return false;

  out.hour = h;
  out.minute = m;
  out.steps = s;
  Serial.printf("Parse timer: %02d:%02d -> %d passos\n", h, m, s);
  return true;
}
// Carrega timers de /timers.txt
void loadTimersFromTxt() {
  File file = LittleFS.open("/timers.txt", "r");
  if (!file) {
    Serial.println("Arquivo /timers.txt não encontrado. Criando vazio.");
    File nf = LittleFS.open("/timers.txt", "w");
    if (nf) nf.close();
    timerCount = 0;
    return;
  }
  timerCount = 0;
  while (file.available() && timerCount < MAX_TIMERS) {
    String line = file.readStringUntil('\n');
    TimerItem t;
    if (parseLineToTimer(line, t)) {
      timers[timerCount++] = t;
    }
  }
  file.close();
  Serial.printf("Timers carregados: %d\n", timerCount);
}
// Salva timers em /timers.txt
void saveTimersToTxt() {
  File file = LittleFS.open("/timers.txt", "w");
  if (!file) {
    Serial.println("Erro ao abrir /timers.txt para escrita");
    return;
  }
  for (int i = 0; i < timerCount; i++) {
    file.printf("%02d:%02d,%d\n", timers[i].hour, timers[i].minute, timers[i].steps);
  }
  file.close();
  Serial.println("Timers salvos em /timers.txt");
}
// ===== Lógica de disparo =====
void checkTimersAndTrigger() {
  struct tm info;
  if (!getLocalTime(&info)) {
    // Sem hora válida: não dispara
    return;
  }

  // Evita múltiplos disparos no mesmo minuto
  if (info.tm_min == lastProcessedMinute &&
      info.tm_hour == lastProcessedHour &&
      info.tm_yday == lastProcessedYday) {
    return;
  }

  // Dispara todos timers que batem neste minuto
  for (int i = 0; i < timerCount; i++) {
    if (info.tm_hour == timers[i].hour && info.tm_min == timers[i].minute) {
      Serial.printf("Timer: %02d:%02d -> %d passos\n", timers[i].hour, timers[i].minute, timers[i].steps);
      if (!runMotorDoseSafe(timers[i].steps)) {
        Serial.println("Falha ao agendar dose via timer (motor ocupado ou parâmetro inválido)");
      }
    }
  }

  // Marca minuto processado
  lastProcessedMinute = info.tm_min;
  lastProcessedHour = info.tm_hour;
  lastProcessedYday = info.tm_yday;

  // Se uma dose estava em progresso e o motor terminou, registra conclusão
  if (motorDoseInProgress && motores.stepstogo(0) == 0) {
    Serial.printf("Motor: dose concluída (%lu passos)\n", (unsigned long)motorDosePendingSteps);
    logDoseComplete(motorDosePendingSteps);
    motorDoseInProgress = false;
    motorDosePendingSteps = 0;
  }
}

// ===== Servir arquivos estáticos =====
void serveFile(const char* path, const char* contentType) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "Arquivo não encontrado");
    return;
  }
  server.streamFile(file, contentType);
  file.close();
}
// ===== Endpoints estáticos =====
void handleRoot() { serveFile("/index.html", "text/html"); }
void handleStyle() { serveFile("/style.css", "text/css"); }
void handleScript() { serveFile("/script.js", "application/javascript"); }

// ===== Endpoints de dados =====
void handleTimersJson() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  server.sendContent("[");
  for (int i = 0; i < timerCount; i++) {
    String item = "{";
    item += "\"hour\":" + String(timers[i].hour) + ",";
    item += "\"minute\":" + String(timers[i].minute) + ",";
    item += "\"steps\":" + String(timers[i].steps);
    item += "}";
    if (i < timerCount - 1) item += ",";
    server.sendContent(item);
  }
  server.sendContent("]");
}
// Adicionar novo timer
void handleAddTimer() {
  if (timerCount >= MAX_TIMERS) {
    server.send(400, "text/plain", "Limite de timers atingido.");
    return;
  }
  if (!server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("steps")) {
    server.send(400, "text/plain", "Parametros ausentes.");
    return;
  }
  int h = server.arg("hour").toInt();
  int m = server.arg("minute").toInt();
  int s = server.arg("steps").toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59 || s <= 0) {
    server.send(400, "text/plain", "Valores invalidos.");
    return;
  }
  timers[timerCount++] = {h, m, s};// adiciona novo timer
  saveTimersToTxt();// salva em arquivo
  server.send(200, "text/plain", "Timer adicionado");
}
// Excluir timer
void handleDeleteTimer() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Index ausente.");
    return;
  }
  int idx = server.arg("index").toInt();
  if (idx < 0 || idx >= timerCount) {
    server.send(400, "text/plain", "Index invalido.");
    return;
  }
  for (int i = idx; i < timerCount - 1; i++) {
    timers[i] = timers[i + 1];
  }
  timerCount--;
  saveTimersToTxt();
  server.send(200, "text/plain", "Timer excluido");
}
// Editar timer
void handleEditTimer() {
  if (!server.hasArg("index") || !server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("steps")) {
    server.send(400, "text/plain", "Parametros ausentes.");
    return;
  }
  int idx = server.arg("index").toInt();
  int h = server.arg("hour").toInt();
  int m = server.arg("minute").toInt();
  int s = server.arg("steps").toInt();
  if (idx < 0 || idx >= timerCount || h < 0 || h > 23 || m < 0 || m > 59 || s <= 0) {
    server.send(400, "text/plain", "Valores invalidos.");
    return;
  }
  timers[idx] = {h, m, s};
  saveTimersToTxt();
  server.send(200, "text/plain", "Timer editado");
}
// Teste manual do motor
void handleTestTimer() {
  if (!server.hasArg("steps")) {
    server.send(400, "text/plain", "Steps ausente.");
    return;
  }
  int s = server.arg("steps").toInt();
  if (s <= 0) {
    server.send(400, "text/plain", "Steps invalido.");
    return;
  }
  if (!runMotorDoseSafe(s)) {
    server.send(500, "text/plain", "Falha ao agendar teste (motor ocupado ou steps inválido)");
    return;
  }
  server.send(200, "text/plain", "Teste acionado");
}
// Hora atual em JSON
void handleTimeJson() {
  struct tm info;
  if (!getLocalTime(&info)) {
    server.send(200, "application/json", "{\"error\":\"hora nao disponivel\"}");
    return;
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", info.tm_hour, info.tm_min);
  String json = "{\"hour\":" + String(info.tm_hour) +
                ",\"minute\":" + String(info.tm_min) +
                ",\"formatted\":\"" + String(buf) + "\"}";
  server.send(200, "application/json", json);
}
// Ajuste manual de hora
void handleSetTime() {
  if (server.hasArg("hour") && server.hasArg("minute")) {
    int h = server.arg("hour").toInt();
    int m = server.arg("minute").toInt();
    bool ok = (h >= 0 && h <= 23 && m >= 0 && m <= 59) && setManualTime(h, m);
    server.send(200, "text/plain", ok ? "Hora ajustada." : "Falha ao ajustar hora.");
  } else {
    server.send(400, "text/plain", "Parametros ausentes.");
  }
}
// Status de Wi-Fi (AP + STA)
void handleWifiStatus() {
  String json = "{";
  json += "\"mode\":\"" + String(WiFi.getMode() == WIFI_STA ? "STA" : (WiFi.getMode() == WIFI_AP ? "AP" : "AP_STA")) + "\",";
  json += "\"localIp\":\"" + (localIp.length() ? localIp : "—") + "\",";
  json += "\"apIp\":\"" + apIp + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
// Configurar Wi-Fi STA (salva em wifi.txt e tenta conectar)
void handleSetWifi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String ssid = server.arg("ssid");
    String pass = server.arg("password");

    // Salva no LittleFS
    File file = LittleFS.open("/wifi.txt", "w");
    if (file) {
      file.println(ssid);
      file.println(pass);
      file.close();
    }

    // Tenta conectar em STA mantendo AP ativo
    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Tentando conectar a " + ssid);

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(250);
      Serial.print(".");
      tries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      localIp = WiFi.localIP().toString();
      server.send(200, "text/plain", "Conectado ao Wi‑Fi: " + ssid + " (IP: " + localIp + ")");
      Serial.println("Conectado! IP STA: " + localIp);
    } else {
      localIp = "Falha ao conectar";
      server.send(200, "text/plain", "Falha ao conectar");
      Serial.println("Falha ao conectar em STA. Mantendo AP.");
    }
  } else {
    server.send(400, "text/plain", "Parametros ausentes");
  }
}
// relatório de doses ministradas
void handleReport() {
  File file = LittleFS.open("/report.txt", "r");
  if (!file) {
    server.send(200, "text/plain", "Relatório vazio");
    return;
  }
  server.streamFile(file, "text/plain");
  file.close();
}
// limpar relatório
void handleClearReport() {
  File file = LittleFS.open("/report.txt", "w"); // sobrescreve vazio
  if (file) file.close();
  server.send(200, "text/plain", "Relatório limpo");
}
// ===== Setup e Loop =====
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Iniciando...");
  if (!LittleFS.begin()) {
    Serial.println("Erro ao montar LittleFS");
    // segue sem arquivos, interface não funcionará corretamente
  }
  // Carregar timers salvos
  loadTimersFromTxt();
  sortTimers(); // ordena por hora/minuto
  // Configura Wi-Fi em modo AP + STA
  // Modo simultâneo AP + STA
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  apIp = WiFi.softAPIP().toString();
  Serial.println("Access Point iniciado.");
  Serial.print("IP do AP: ");
  Serial.println(apIp);
    // Tenta conectar STA com credenciais salvas
  File wf = LittleFS.open("/wifi.txt", "r");
  if (wf) {
    String ssid = wf.readStringUntil('\n'); ssid.trim();
    String pass = wf.readStringUntil('\n'); pass.trim();
    wf.close();
    if (ssid.length() > 0) {
      Serial.println("Credenciais salvas encontradas. Tentando STA: " + ssid);
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
  }
 // Aguarda brevemente pela conexão STA (sem bloquear muito)
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    delay(100);
  }

  if (WiFi.status() == WL_CONNECTED) {
    localIp = WiFi.localIP().toString();
    Serial.println("Conectado em STA. IP: " + localIp);
  }
  else {
    Serial.println("Não conectado em STA. Apenas AP ativo.");
  }
  setupMotor();// inicializa motor
  Serial.println("Motor inicializado.");
  // Rotas estáticas
  server.on("/", handleRoot);
  server.on("/style.css", handleStyle);
  server.on("/script.js", handleScript);

  // Rotas de dados
  server.on("/timers.json", handleTimersJson);
  server.on("/motorStatus.json", handleMotorStatus);
  server.on("/addTimer", handleAddTimer);
  server.on("/deleteTimer", handleDeleteTimer);
  server.on("/editTimer", handleEditTimer);
  server.on("/testTimer", handleTestTimer);
  server.on("/time.json", handleTimeJson);
  server.on("/setTime", handleSetTime);
  server.on("/wifiStatus.json", handleWifiStatus);
  server.on("/setWifi", handleSetWifi);
  server.on("/report.txt", handleReport);
  server.on("/clearReport", handleClearReport);

  server.begin();

  setupTimeNTP();   // tenta NTP
  loadTimersFromTxt();
  Serial.println("Setup concluído.");
// ===== Loop principal =====
  server.handleClient();
  checkTimersAndTrigger();
  delay(1);      // cooperatividade
}
void loop() {
  server.handleClient();
  checkTimersAndTrigger();
  delay(1);      // cooperatividade
}

// Status do motor (JSON)
void handleMotorStatus() {
  uint32_t stepsLeft = motores.stepstogo(0);
  bool busy = (stepsLeft > 0) || motorDoseInProgress;
  String json = "{";
  json += "\"busy\":" + String(busy ? "true" : "false") + ",";
  json += "\"stepsToGo\":" + String(stepsLeft) + ",";
  json += "\"pending\":" + String(motorDosePendingSteps);
  json += "}";
  server.send(200, "application/json", json);
}