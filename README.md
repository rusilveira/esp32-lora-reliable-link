# ESP32 LoRa Reliable Link

Reliable point-to-point LoRa communication using **ESP32 + SX1276**, with a lightweight application-layer protocol based on:

- packet sequence number
- CRC16 integrity check
- ACK confirmation
- duplicate packet detection
- retransmission logic

This repository is part of a larger smart beehive IoT project, where one ESP32 acts as the **sensor node** and another ESP32 acts as the **gateway**.

---

## Project Overview

The objective of this project is to validate a **robust bidirectional LoRa link** between two ESP32 devices using SX1276 transceivers.

### Architecture

- **TX Node (`tx-node/`)**
  - Simulates the beehive node
  - Sends structured LoRa packets
  - Waits for ACK confirmation
  - Retransmits if confirmation is not received

- **RX Gateway (`rx-gateway/`)**
  - Receives and validates packets
  - Sends ACK back to the transmitter
  - Detects duplicate packets
  - Prevents duplicated processing

---

## Communication Strategy

The system uses a lightweight reliable protocol over LoRa.

### DATA packet structure

- Magic byte
- Protocol version
- Message type
- Node ID
- Sequence number
- Payload length
- Payload
- CRC16

### ACK packet structure

- Magic byte
- Protocol version
- Message type
- Gateway ID
- Sequence number
- Status
- CRC16

---

## Reliability Features

- **Sequence control**: each DATA packet has a sequence number
- **CRC16-CCITT**: packet integrity verification at application layer
- **ACK confirmation**: receiver confirms successful reception
- **Retransmission**: sender retries if ACK is not received
- **Duplicate detection**: receiver identifies repeated packets and avoids reprocessing

---

## Hardware Used

- 2x ESP32 boards
- 2x SX1276 LoRa modules / onboard LoRa ESP32 boards
- PlatformIO
- RadioLib library

---

## LoRa Configuration

Validated configuration:

- **Frequency:** 915 MHz
- **Bandwidth:** 500 kHz
- **Spreading Factor:** 7
- **Coding Rate:** 4/5
- **Output Power:** 2 dBm
- **Preamble Length:** 12

---

## Validation Summary

During development, multiple issues were identified and solved:

- corrupted payload
- communication instability
- timeouts
- hardware assembly issues
- SPI initialization issues
- ACK reception failure after transmission

The final fix required forcing the SX1276 transceiver into **receive mode** after transmission before waiting for ACK. This solved the main communication reliability issue.

After correction, the system successfully validated:

- bidirectional LoRa communication
- correct ACK reception
- CRC validation
- sequence progression
- stable packet confirmation flow

---

## Repository Structure

```text
esp32-lora-reliable-link/
├── docs/
│   └── lora-validation-report.md
├── rx-gateway/
│   ├── include/
│   ├── src/
│   ├── test/
│   └── platformio.ini
├── tx-node/
│   ├── include/
│   ├── src/
│   ├── test/
│   └── platformio.ini
└── README.md
```

## Current Status

✅ LoRa link validated
✅ ACK-based reliable communication working
✅ Sequence progression validated
✅ Duplicate detection implemented

## Author

Ruan Silveira
Industrial Maintenance Technician
Electrical Engineering Student
Focus: Embedded Systems, IoT, Industrial Applications, Agro 4.0