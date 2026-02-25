import admin from 'firebase-admin';
import { ethers } from 'ethers';
import { Indexer, ZgFile } from '@0glabs/0g-ts-sdk';
import fs from 'fs';

// 1. Safely load the Firebase key
const serviceAccount = JSON.parse(fs.readFileSync('./firebase-key.json', 'utf8'));

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: "https://cng-protect-default-rtdb.firebaseio.com/" 
});
const db = admin.database();

// 2. Connect to the 0G Labs Newton Testnet
const EVM_RPC_URL = "https://evmrpc-testnet.0g.ai";
const provider = new ethers.JsonRpcProvider(EVM_RPC_URL);

// DANGER: Put your private key back here, but keep it out of GitHub!
const privateKey = "b6915727b66aecb95c6691e3b4f67b06f04ffc682cfdd6041b46565283d1c628"; 
const wallet = new ethers.Wallet(privateKey, provider);

// --- NEW: Print the Public Address for the 0G Storage Explorer ---
console.log("\n=======================================================");
console.log("YOUR PUBLIC WALLET ADDRESS IS:");
console.log(wallet.address);
console.log("Search this exact address on storagescan-newton.0g.ai");
console.log("=======================================================\n");

// 3. Connect to 0G Storage
const INDEXER_RPC = "https://indexer-storage-testnet-turbo.0g.ai"; 
const indexer = new Indexer(INDEXER_RPC);

// 4. Connect to your 0G Smart Contract
const contractAddress = "0xD74Dd42d21e4784232E85A38485d7eD8Af3D7beB";
const abi = [
    "function mintIncidentReport(string memory deviceId, uint256 gasLevel) public"
];
const incidentContract = new ethers.Contract(contractAddress, abi, wallet);

// 5. State Management (Prevents Network Spam & Memory Leaks)
let isCurrentlyInDanger = false;
let isUploadingDA = false; 

console.log("CNG-Protect Cloud Server Running. Watching for hardware alerts...");

// 6. The Core Engine
db.ref('/cng_protect/devices/device_01/live_data').on('value', async (snapshot) => {
    const data = snapshot.val();
    if (!data) return;

    // --- INTEGRATION 2: 0G CHAIN & iNFTs (Smart Execution) ---
    // State Change: SAFE -> DANGER 
    if (data.is_danger === true && !isCurrentlyInDanger) {
        console.log(`\n⚠️ CRITICAL LEAK (Gas: ${data.gas_level}). Minting 0G Incident iNFT...`);
        isCurrentlyInDanger = true; 

        try {
            // Force the transaction to grab the absolute latest nonce to prevent collisions
            const nonce = await provider.getTransactionCount(wallet.address);
            const tx = await incidentContract.mintIncidentReport("Device_01", data.gas_level, { nonce: nonce });

            console.log(`Transaction sent to 0G Chain! Waiting for confirmation...`);
            await tx.wait(); 
            console.log(`✅ Incident iNFT Minted! Tx: ${tx.hash}\n`);
        } catch (error) {
            console.error("0G Blockchain Error:", error.message || error);
        }
    }

    // State Change: DANGER -> SAFE 
    if (data.is_danger === false && isCurrentlyInDanger) {
        console.log("🟢 Leak resolved. Hardware returned to Safe Mode.\n");
        isCurrentlyInDanger = false; 
    }

    // --- INTEGRATION 1: 0G STORAGE (High-Frequency Data Publishing) ---
    // The Lock: Only upload if we are NOT already uploading, AND NOT in an active emergency.
    if (!isUploadingDA && !isCurrentlyInDanger) {
        isUploadingDA = true; // Lock the gate

        try {
            const filePath = './temp_log.json';
            // Synchronous write is now safe because the lock prevents spam
            fs.writeFileSync(filePath, JSON.stringify(data));

            const file = await ZgFile.fromFilePath(filePath);
            const [txLog, errLog] = await indexer.upload(file, EVM_RPC_URL, wallet);

            if (errLog === null) {
                console.log(`📡 Telemetry logged to 0G Storage. Hash: ${txLog}`);
            }

            // Explicitly close the file to stop the garbage collection warning
            if (file.close) file.close(); 
        } catch (e) {
            // Fail silently on storage sync errors to keep the console clean
        } finally {
            isUploadingDA = false; // Unlock the gate for the next telemetry packet
        }
    }
});
