# 🛡️ CNG-Protect: The Immutable Industrial Safety Protocol

**Built on 0G | Powered by Ogbontor Engineering Enterprise**

[![0G Network](https://img.shields.io/badge/Network-0G_Testnet-blue.svg)](https://0g.ai/)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32-green.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

## 🚨 The Problem: "The Trust Gap"
In industrial fleets (like CNG vehicles), safety logs are stored on centralized databases. When a critical gas leak occurs, corrupt fleet managers facing massive liability costs have a financial incentive to simply press "Delete." If the server says the leak didn't happen, insurance fraud succeeds, and lives remain at risk.

## 💡 The Solution: Automated Accountability
CNG-Protect is an IoT-to-Blockchain bridge that replaces "Trust Me" with cryptographic proof. Using an ESP32 and gas sensors at the edge, we log high-frequency telemetry data directly to the **0G Storage (DA)** network. If gas levels cross a critical threshold, the system bypasses its normal heartbeat and instantly mints an **Incident iNFT** on the **0G Chain**.

The safety record becomes permanent. No human can alter or delete it.

---

## 🏗️ The "Triple Threat" Architecture

1. **The Edge (Hardware):** ESP32 + MQ2 Gas Sensor. Monitors ambient gas continuously. If ppm > 1100, it triggers a critical alert.
2. **The Pipeline (Middleware):** A Node.js backend connected to Firebase Realtime Database. It aggregates the high-frequency telemetry.
3. **The Layer 1 (Storage & Execution):** - **0G Data Availability (DA):** Stores the massive stream of second-by-second sensor logs (The Black Box).
   - **0G Chain:** Executes the smart contract to mint an Incident iNFT only when `STATUS = DANGER`.

---

## 🎥 Hackathon Submission Links

* **Live Demo Video:** [INSERT YOUTUBE LINK HERE]
* **Pitch Deck:** [INSERT GOOGLE DRIVE PDF LINK HERE]
* **0G Transaction Proof:** [INSERT 0G EXPLORER TX HASH LINK HERE]

---

## 💻 Tech Stack
* **Hardware:** ESP32 Microcontroller, MQ2 Gas Sensor.
* **Backend:** Node.js, ethers.js, Firebase Admin SDK.
* **Web3 / DePIN:** 0G Storage DA, 0G Chain (EVM Compatible).

---

## 🛠️ How to Run Locally

If you want to run the middleware locally to bridge your own IoT devices to 0G, follow these steps:

### 1. Clone the Repo
```bash
git clone [https://github.com/yourusername/CNG-Protect-0G.git](https://github.com/yourusername/CNG-Protect-0G.git)
cd CNG-Protect-0G/backend
