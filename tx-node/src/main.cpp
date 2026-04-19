#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

// ==========================
// PINOS LoRa
// ==========================
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_CS    18
#define LORA_DIO0  26
#define LORA_RST   14

SPIClass spiLoRa(VSPI);
SX1276 radio = new Module(LORA_CS, LORA_DIO0, LORA_RST, RADIOLIB_NC, spiLoRa);

// ==========================
// CONFIG PROTOCOLO
// ==========================
constexpr uint8_t MAGIC_BYTE      = 0xA5;
constexpr uint8_t PROTO_VERSION   = 0x01;
constexpr uint8_t MSG_TYPE_DATA   = 0x10;
constexpr uint8_t MSG_TYPE_ACK    = 0x20;

constexpr uint8_t NODE_ID         = 0x01;
constexpr uint8_t GATEWAY_ID      = 0xF0;

// ==========================
// CONFIG LoRa
// ==========================
constexpr float LORA_FREQ_MHZ    = 915.0;
constexpr float LORA_BW_KHZ      = 500.0;
constexpr uint8_t LORA_SF        = 7;
constexpr uint8_t LORA_CR        = 5;      // 4/5
constexpr int8_t LORA_POWER_DBM  = 2;
constexpr uint16_t LORA_PREAMBLE = 12;

// ==========================
// CONTROLE DE ENVIO
// ==========================
constexpr uint32_t SEND_INTERVAL_MS = 2000;
constexpr uint16_t ACK_TIMEOUT_MS   = 800;
constexpr uint8_t MAX_RETRIES       = 3;

// ==========================
// PAYLOAD FIXO DE TESTE
// ==========================
struct SensorPayload {
  int16_t temperatura_c_x100;   // 25.34 -> 2534
  uint16_t umidade_x100;        // 65.43 -> 6543
  int16_t peso_kg_x100;         // 12.34 -> 1234
  uint16_t reserv;              // reservado
};

// ==========================
// CRC16-CCITT
// ==========================
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;

    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

// ==========================
// DEBUG HEX
// ==========================
void printHexBuffer(const uint8_t* buf, size_t len) {
  Serial.print("[HEX] ");
  for (size_t i = 0; i < len; i++) {
    if (buf[i] < 16) Serial.print("0");
    Serial.print(buf[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ==========================
// MONTA PACOTE DATA
// ==========================
size_t buildDataPacket(uint8_t seq, const uint8_t* payload, uint8_t payloadLen, uint8_t* outBuf) {
  size_t idx = 0;

  outBuf[idx++] = MAGIC_BYTE;
  outBuf[idx++] = PROTO_VERSION;
  outBuf[idx++] = MSG_TYPE_DATA;
  outBuf[idx++] = NODE_ID;
  outBuf[idx++] = seq;
  outBuf[idx++] = payloadLen;

  memcpy(&outBuf[idx], payload, payloadLen);
  idx += payloadLen;

  uint16_t crc = crc16_ccitt(outBuf, idx);
  outBuf[idx++] = (crc >> 8) & 0xFF;
  outBuf[idx++] = crc & 0xFF;

  return idx;
}

// ==========================
// VALIDA ACK COM DEBUG
// ==========================
bool parseAndValidateAckVerbose(const uint8_t* buf, size_t len, uint8_t expectedSeq) {
  Serial.println("[ACK DEBUG] iniciando validacao...");

  if (len < 8) {
    Serial.print("[ACK DEBUG] tamanho invalido: ");
    Serial.println(len);
    return false;
  }

  uint16_t rxCrc = ((uint16_t)buf[len - 2] << 8) | buf[len - 1];
  uint16_t calcCrc = crc16_ccitt(buf, len - 2);

  Serial.print("[ACK DEBUG] magic=0x");
  Serial.println(buf[0], HEX);

  Serial.print("[ACK DEBUG] version=0x");
  Serial.println(buf[1], HEX);

  Serial.print("[ACK DEBUG] type=0x");
  Serial.println(buf[2], HEX);

  Serial.print("[ACK DEBUG] gatewayId=0x");
  Serial.println(buf[3], HEX);

  Serial.print("[ACK DEBUG] seq=");
  Serial.println(buf[4]);

  Serial.print("[ACK DEBUG] status=0x");
  Serial.println(buf[5], HEX);

  Serial.print("[ACK DEBUG] crc recebido=0x");
  Serial.println(rxCrc, HEX);

  Serial.print("[ACK DEBUG] crc calculado=0x");
  Serial.println(calcCrc, HEX);

  if (rxCrc != calcCrc) {
    Serial.println("[ACK DEBUG] CRC invalido");
    return false;
  }

  if (buf[0] != MAGIC_BYTE) {
    Serial.println("[ACK DEBUG] MAGIC invalido");
    return false;
  }

  if (buf[1] != PROTO_VERSION) {
    Serial.println("[ACK DEBUG] VERSION invalida");
    return false;
  }

  if (buf[2] != MSG_TYPE_ACK) {
    Serial.println("[ACK DEBUG] TYPE invalido");
    return false;
  }

  if (buf[3] != GATEWAY_ID) {
    Serial.println("[ACK DEBUG] GATEWAY_ID invalido");
    return false;
  }

  if (buf[4] != expectedSeq) {
    Serial.println("[ACK DEBUG] SEQ inesperado");
    return false;
  }

  if (buf[5] != 0x00) {
    Serial.println("[ACK DEBUG] STATUS invalido");
    return false;
  }

  Serial.println("[ACK DEBUG] ACK valido");
  return true;
}

// ==========================
// INICIALIZA RADIO
// ==========================
void initRadio() {
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  delay(100);

  int state = radio.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha no begin: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setFrequency(LORA_FREQ_MHZ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setFrequency: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setBandwidth(LORA_BW_KHZ);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setBandwidth: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setSpreadingFactor(LORA_SF);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setSpreadingFactor: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setCodingRate(LORA_CR);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setCodingRate: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setOutputPower(LORA_POWER_DBM);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setOutputPower: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  state = radio.setPreambleLength(LORA_PREAMBLE);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("Falha setPreambleLength: ");
    Serial.println(state);
    while (true) delay(1000);
  }

  Serial.println("Radio TX inicializado com sucesso.");
}

// ==========================
// ENVIO COM ACK E RETRY
// ==========================
uint8_t currentSeq = 0;
uint32_t lastSendMs = 0;

bool sendWithAck(const uint8_t* payload, uint8_t payloadLen) {
  uint8_t txBuf[64];
  size_t txLen = buildDataPacket(currentSeq, payload, payloadLen, txBuf);

  for (uint8_t attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    Serial.println("====================================");
    Serial.print("[TX] seq=");
    Serial.print(currentSeq);
    Serial.print(" tentativa=");
    Serial.println(attempt + 1);

    printHexBuffer(txBuf, txLen);

    int state = radio.transmit(txBuf, txLen);

    if (state != RADIOLIB_ERR_NONE) {
      Serial.print("[TX] erro transmit: ");
      Serial.println(state);
      delay(100);
      continue;
    }

    Serial.println("[TX] enviado com sucesso");
    Serial.println("[TX] aguardando ACK...");
    radio.startReceive(); 
    delay(10);

    uint8_t ackBuf[16];
    memset(ackBuf, 0, sizeof(ackBuf));

    state = radio.receive(ackBuf, sizeof(ackBuf), ACK_TIMEOUT_MS);

    Serial.print("[ACK] state receive = ");
    Serial.println(state);

    if (state == RADIOLIB_ERR_NONE) {
      size_t ackLen = radio.getPacketLength();
      float rssi = radio.getRSSI();
      float snr  = radio.getSNR();

      Serial.print("[ACK] len=");
      Serial.print(ackLen);
      Serial.print(" RSSI=");
      Serial.print(rssi);
      Serial.print(" SNR=");
      Serial.println(snr);

      printHexBuffer(ackBuf, ackLen);

      if (parseAndValidateAckVerbose(ackBuf, ackLen, currentSeq)) {
        Serial.println("[ACK] confirmado");
        currentSeq++;
        return true;
      } else {
        Serial.println("[ACK] recebido mas invalido");
      }
    } 
    else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
      Serial.println("[ACK] timeout");
    } 
    else {
      Serial.print("[ACK] erro receive: ");
      Serial.println(state);
    }

    delay(150);
  }

  Serial.print("[FALHA] seq=");
  Serial.print(currentSeq);
  Serial.println(" nao confirmado");
  return false;
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1500);
  randomSeed(micros());

  Serial.println();
  Serial.println("=== NO COLMEIA TX ===");

  initRadio();
}

// ==========================
// LOOP
// ==========================
void loop() {
  if (millis() - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = millis();

    SensorPayload payload;
    payload.temperatura_c_x100 = 2534;
    payload.umidade_x100 = 6543;
    payload.peso_kg_x100 = 1234;
    payload.reserv = 0;

    bool ok = sendWithAck((uint8_t*)&payload, sizeof(payload));

    if (ok) {
      Serial.println("[STATUS] envio concluido com sucesso.");
    } else {
      Serial.println("[STATUS] envio falhou apos retries.");
    }
  }
}