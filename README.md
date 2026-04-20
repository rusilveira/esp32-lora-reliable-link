# ESP32 LoRa Reliable Link

Reliable point-to-point LoRa communication using **ESP32 + SX1276**, implementing a lightweight application-layer protocol with **ACK, CRC16 and sequence control**.

This project is part of a distributed IoT architecture for a **smart beehive monitoring system**, where a sensor node communicates with a gateway via LoRa.

---

## 📡 Overview

This repository demonstrates a **robust bidirectional LoRa communication system**, solving common issues such as:

- packet loss
- data corruption
- unreliable delivery
- lack of confirmation

A custom protocol was implemented to guarantee **data integrity and delivery confirmation**.

---

## 🧠 Key Features

- Reliable communication over LoRa
- ACK-based confirmation system
- CRC16 integrity verification
- Sequence-based packet control
- Duplicate packet detection
- Automatic retransmission (retry logic)
- Clean modular TX/RX architecture

---

## 🏗️ System Architecture

[ TX Node (ESP32) ] ---> LoRa ---> [ RX Gateway (ESP32) ]
│ │
Sensor data ACK + validation

### TX Node
- Sends structured packets
- Waits for ACK
- Retries on failure

### RX Gateway
- Validates packets
- Sends ACK
- Detects duplicates

---

## 📦 Packet Structure

### DATA Packet

| Field | Description |
|------|------------|
| Magic Byte | Packet identifier |
| Version | Protocol version |
| Type | DATA |
| Node ID | Sender ID |
| Sequence | Packet number |
| Length | Payload size |
| Payload | Sensor data |
| CRC16 | Integrity check |

---

### ACK Packet

| Field | Description |
|------|------------|
| Magic Byte | Packet identifier |
| Version | Protocol version |
| Type | ACK |
| Gateway ID | Receiver ID |
| Sequence | Confirmed packet |
| Status | OK |
| CRC16 | Integrity check |

---

## ⚙️ LoRa Configuration

- Frequency: **915 MHz**
- Bandwidth: **500 kHz**
- Spreading Factor: **7**
- Coding Rate: **4/5**
- Power: **2 dBm**
- Preamble: **12**

---

## 🔍 Validation Results

The system successfully validated:

- Stable bidirectional communication
- ACK reception after TX fix
- Correct CRC validation
- Sequence progression (0 → N)
- Duplicate detection at receiver
- Reliable packet confirmation

A critical issue was identified and solved:

> The SX1276 transceiver was not automatically switching from TX to RX mode.  
> The solution required explicitly forcing RX mode before waiting for ACK.

---

## 📁 Project Structure

esp32-lora-reliable-link/
├── tx-node/
├── rx-gateway/
├── docs/
│ └── lora-validation-report.md

---

## 📊 Current Status

✅ Reliable LoRa link validated  
✅ Protocol implemented and tested  
✅ Clean TX/RX code  
🔜 Next: real sensor integration + backend communication  

---

## 🚀 Future Improvements

- HX711 integration (weight)
- Environmental sensors
- Gateway → Backend (HTTP)
- Dashboard integration
- Field testing and optimization

---

## 👨‍💻 Author

**Ruan Silveira**  
Electrical Engineering Student  
Industrial Maintenance Technician  

Focus areas:
- Embedded Systems
- IoT
- Industrial Automation
- Agro 4.0