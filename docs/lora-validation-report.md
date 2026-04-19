
---

# docs/lora-validation-report.md

```md
# LoRa Validation Report – ESP32 + SX1276 Reliable Link

## 1. Objective

The objective of this validation stage was to establish a reliable LoRa communication link between two ESP32 devices using SX1276 transceivers, creating a robust basis for the smart beehive distributed architecture.

The goal was not only to transmit packets, but also to ensure:

- packet integrity
- reception confirmation
- duplicate detection
- retransmission support
- bidirectional operation

---

## 2. System Architecture

The validation was performed with two ESP32-based devices:

- **TX Node**: responsible for sending structured data packets
- **RX Gateway**: responsible for receiving packets and sending ACK confirmation

This communication model represents the future architecture of the smart beehive project:

- sensor node on the hive
- gateway node connected to the backend

---

## 3. Initial Problems Observed

During the first tests, the communication presented several issues:

- corrupted payload
- CRC failures
- repeated timeout events
- unstable communication behavior
- inconsistent reception

Additionally, physical assembly problems were identified, including pin/solder issues, which directly affected link reliability.

---

## 4. LoRa Configuration Tests

Different LoRa bandwidth settings were tested.

### Results observed

- **125 kHz** → CRC FAIL + timeout
- **250 kHz** → total timeout
- **500 kHz** → functional communication

### Best configuration found

- **Frequency:** 915 MHz
- **Bandwidth:** 500 kHz
- **Spreading Factor:** 7
- **Coding Rate:** 4/5
- **Output Power:** 2 dBm
- **Preamble Length:** 12

This configuration produced the best link performance during validation.

---

## 5. SPI Initialization Issue

A relevant issue was identified during transceiver initialization.

Initially, the SX1276 was not always detected correctly by the library. The problem was traced to SPI initialization and board pin mapping. The correction required explicit SPI initialization and correct association of SPI pins with the radio module.

After explicit SPI configuration, the transceiver initialization became stable.

---

## 6. Protocol Design

In order to make the link reliable, a lightweight protocol was implemented at application level.

### DATA Packet Fields

- magic byte
- protocol version
- message type
- node ID
- sequence number
- payload length
- payload
- CRC16

### ACK Packet Fields

- magic byte
- protocol version
- message type
- gateway ID
- confirmed sequence number
- status byte
- CRC16

---

## 7. Reliability Mechanism

The communication strategy was based on a stop-and-wait approach.

### Workflow

1. TX node sends one DATA packet
2. RX gateway validates packet integrity
3. RX gateway sends ACK with the same sequence number
4. TX node waits for ACK
5. if ACK is valid, TX increments sequence number
6. if ACK is not received, TX retransmits

### Additional protection

- duplicate packet detection at the receiver
- CRC16 validation at application layer
- timeout and retry control

---

## 8. ACK Reception Problem

After implementing ACK, the receiver successfully received DATA packets and transmitted ACK messages, but the transmitter still timed out waiting for confirmation.

The observed behavior was:

- RX correctly received the packet
- RX sent ACK successfully
- TX continuously timed out
- RX identified repeated packets with the same sequence number

This showed that the forward link was functional, but the transmitter was not actually receiving the return confirmation.

---

## 9. Root Cause

The root cause was identified in the transition from **transmit mode** to **receive mode** in the SX1276.

After transmitting a packet, the transceiver was not reliably switching into reception state before waiting for ACK. Therefore, even though the ACK was being sent by the gateway, the transmitter was not effectively listening to the channel.

---

## 10. Implemented Fix

The solution was to explicitly force the transceiver into receive mode after transmission and before waiting for ACK.

This correction ensured that the TX node entered the correct RX state and became able to receive the confirmation packet.

After the fix, the communication logs showed:

- ACK reception success
- valid CRC verification
- sequence progression
- confirmed packet delivery

---

## 11. Final Validation Result

After correction, the system achieved:

- successful bidirectional LoRa communication
- correct ACK reception
- correct CRC16 validation
- sequence number progression
- duplicate packet detection at receiver
- stable confirmation mechanism

The sequence values progressed correctly (`seq = 0, 1, 2, 3, 4...`), proving the protocol was working as intended.

---

## 12. Technical Conclusion

This validation stage demonstrated that it is possible to build a reliable communication layer over LoRa using ESP32 and SX1276, even with hardware and RF limitations, by adding a lightweight application-level protocol.

The validated solution includes:

- sequence-based control
- CRC16 integrity verification
- ACK-based confirmation
- retransmission support
- duplicate detection

This result establishes a robust foundation for the next stages of the smart beehive project, especially the integration of real sensors and gateway-to-backend communication.

---

## 13. Next Steps

The recommended next development steps are:

1. replace simulated payload with real sensor data
2. integrate HX711 and environmental sensors
3. send data from gateway to backend
4. connect backend to dashboard
5. evaluate long-term field performance