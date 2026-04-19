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

constexpr uint8_t GATEWAY_ID      = 0xF0;

// ==========================
// CONFIG LoRa
// ==========================
constexpr float LORA_FREQ_MHZ    = 915.0;
constexpr float LORA_BW_KHZ      = 500.0;
constexpr uint8_t LORA_SF        = 7;
constexpr uint8_t LORA_CR        = 5;
constexpr int8_t LORA_POWER_DBM  = 2;
constexpr uint16_t LORA_PREAMBLE = 12;

// ==========================
// PAYLOAD FIXO
// ==========================
struct SensorPayload {
  int16_t temperatura_c_x100;
  uint16_t umidade_x100;
  int16_t peso_kg_x100;
  uint16_t reserv;
};

// ==========================
// CONTROLE DE DUPLICIDADE
// ==========================
struct LastPacketControl {
  bool valid = false;
  uint8_t nodeId = 0;
  uint8_t seq = 0;
};

LastPacketControl lastProcessed;

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
// VALIDA PACOTE DATA
// ==========================
bool validateDataPacket(const uint8_t* buf, size_t len) {
  if (len < 8) return false;

  uint16_t rxCrc = ((uint16_t)buf[len - 2] << 8) | buf[len - 1];
  uint16_t calcCrc = crc16_ccitt(buf, len - 2);

  if (rxCrc != calcCrc) return false;
  if (buf[0] != MAGIC_BYTE) return false;
  if (buf[1] != PROTO_VERSION) return false;
  if (buf[2] != MSG_TYPE_DATA) return false;

  uint8_t payloadLen = buf[5];
  if (len != (size_t)(6 + payloadLen + 2)) return false;

  return true;
}

// ==========================
// MONTA ACK
// ==========================
size_t buildAckPacket(uint8_t seq, uint8_t* outBuf) {
  size_t idx = 0;

  outBuf[idx++] = MAGIC_BYTE;
  outBuf[idx++] = PROTO_VERSION;
  outBuf[idx++] = MSG_TYPE_ACK;
  outBuf[idx++] = GATEWAY_ID;
  outBuf[idx++] = seq;
  outBuf[idx++] = 0x00;   // status OK

  uint16_t crc = crc16_ccitt(outBuf, idx);
  outBuf[idx++] = (crc >> 8) & 0xFF;
  outBuf[idx++] = crc & 0xFF;

  return idx;
}

// ==========================
// RADIO
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

  Serial.println("Radio RX inicializado com sucesso.");
}

// ==========================
// DUPLICIDADE
// ==========================
bool isDuplicate(uint8_t nodeId, uint8_t seq) {
  return lastProcessed.valid &&
         lastProcessed.nodeId == nodeId &&
         lastProcessed.seq == seq;
}

void markProcessed(uint8_t nodeId, uint8_t seq) {
  lastProcessed.valid = true;
  lastProcessed.nodeId = nodeId;
  lastProcessed.seq = seq;
}

// ==========================
// ENVIA ACK
// ==========================
void sendAck(uint8_t seq) {
  uint8_t ackBuf[16];
  size_t ackLen = buildAckPacket(seq, ackBuf);

  int state = radio.transmit(ackBuf, ackLen);

  Serial.print("[ACK TX] seq=");
  Serial.print(seq);
  Serial.print(" state=");
  Serial.println(state);
}

// ==========================
// PROCESSA DADOS
// ==========================
void processPacket(const SensorPayload& p, uint8_t nodeId, uint8_t seq, float rssi, float snr) {
  float temperatura = p.temperatura_c_x100 / 100.0f;
  float umidade = p.umidade_x100 / 100.0f;
  float peso = p.peso_kg_x100 / 100.0f;

  Serial.println("=========== PACOTE NOVO ===========");
  Serial.print("nodeId: "); Serial.println(nodeId);
  Serial.print("seq: "); Serial.println(seq);
  Serial.print("temperatura: "); Serial.println(temperatura);
  Serial.print("umidade: "); Serial.println(umidade);
  Serial.print("peso: "); Serial.println(peso);
  Serial.print("RSSI: "); Serial.println(rssi);
  Serial.print("SNR: "); Serial.println(snr);
  Serial.println("==================================");
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("=== GATEWAY RX ===");

  initRadio();
}

// ==========================
// LOOP
// ==========================
void loop() {
  uint8_t rxBuf[64];
  memset(rxBuf, 0, sizeof(rxBuf));

  int state = radio.receive(rxBuf, sizeof(rxBuf), 1000);

  if (state == RADIOLIB_ERR_NONE) {
    size_t len = radio.getPacketLength();
    float rssi = radio.getRSSI();
    float snr = radio.getSNR();

    Serial.print("[RX] len=");
    Serial.print(len);
    Serial.print(" RSSI=");
    Serial.print(rssi);
    Serial.print(" SNR=");
    Serial.println(snr);

    if (!validateDataPacket(rxBuf, len)) {
      Serial.println("[RX] pacote invalido.");
      return;
    }

    uint8_t nodeId = rxBuf[3];
    uint8_t seq = rxBuf[4];
    uint8_t payloadLen = rxBuf[5];
    
    delay(60);   // ESSENCIAL
    sendAck(seq);

    if (isDuplicate(nodeId, seq)) {
      Serial.print("[RX] pacote duplicado node=");
      Serial.print(nodeId);
      Serial.print(" seq=");
      Serial.println(seq);
      return;
    }

    if (payloadLen == sizeof(SensorPayload)) {
      SensorPayload payload;
      memcpy(&payload, &rxBuf[6], sizeof(SensorPayload));

      processPacket(payload, nodeId, seq, rssi, snr);
      markProcessed(nodeId, seq);
    } else {
      Serial.print("[RX] payloadLen inesperado: ");
      Serial.println(payloadLen);
    }

  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // timeout normal
  } else {
    Serial.print("[RX] erro receive: ");
    Serial.println(state);
  }
}