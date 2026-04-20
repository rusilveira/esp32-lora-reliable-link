#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

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
constexpr uint8_t MAGIC_BYTE    = 0xA5;
constexpr uint8_t PROTO_VERSION = 0x01;

constexpr uint8_t MSG_DATA = 0x10;
constexpr uint8_t MSG_ACK  = 0x20;

constexpr uint8_t GATEWAY_ID = 0xF0;

// ==========================
struct Payload {
  int16_t temp;
  uint16_t hum;
  int16_t weight;
  uint16_t reserved;
};

// ==========================
uint16_t crc16(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
    }
  }
  return crc;
}

// ==========================
bool validPacket(uint8_t* buf, size_t len) {
  if (len < 8) return false;

  uint16_t crc_rx = (buf[len - 2] << 8) | buf[len - 1];
  uint16_t crc_calc = crc16(buf, len - 2);

  return (
    crc_rx == crc_calc &&
    buf[0] == MAGIC_BYTE &&
    buf[1] == PROTO_VERSION &&
    buf[2] == MSG_DATA
  );
}

// ==========================
size_t buildAck(uint8_t seq, uint8_t* buf) {
  size_t i = 0;

  buf[i++] = MAGIC_BYTE;
  buf[i++] = PROTO_VERSION;
  buf[i++] = MSG_ACK;
  buf[i++] = GATEWAY_ID;
  buf[i++] = seq;
  buf[i++] = 0x00;

  uint16_t crc = crc16(buf, i);
  buf[i++] = crc >> 8;
  buf[i++] = crc & 0xFF;

  return i;
}

// ==========================
void initRadio() {
  spiLoRa.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  delay(100);

  if (radio.begin() != RADIOLIB_ERR_NONE) {
    Serial.println("Erro init radio");
    while (true);
  }

  radio.setFrequency(915.0);
  radio.setBandwidth(500.0);
  radio.setSpreadingFactor(7);
  radio.setCodingRate(5);
  radio.setOutputPower(2);
  radio.setPreambleLength(12);

  Serial.println("Radio RX OK");
}

// ==========================
uint8_t lastSeq = 255;

// ==========================
void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("=== RX GATEWAY ===");
  initRadio();
}

// ==========================
void loop() {
  uint8_t buf[64];

  int state = radio.receive(buf, sizeof(buf), 1000);

  if (state != RADIOLIB_ERR_NONE) return;

  size_t len = radio.getPacketLength();

  if (!validPacket(buf, len)) return;

  uint8_t seq = buf[4];

  delay(100);

  uint8_t ack[16];
  size_t ackLen = buildAck(seq, ack);

  radio.transmit(ack, ackLen);

  if (seq == lastSeq) {
    Serial.print("DUP seq=");
    Serial.println(seq);
    return;
  }

  lastSeq = seq;

  Payload p;
  memcpy(&p, &buf[6], sizeof(Payload));

  Serial.print("RX seq=");
  Serial.print(seq);
  Serial.print(" temp=");
  Serial.print(p.temp / 100.0);
  Serial.print(" hum=");
  Serial.print(p.hum / 100.0);
  Serial.print(" weight=");
  Serial.println(p.weight / 100.0);
}