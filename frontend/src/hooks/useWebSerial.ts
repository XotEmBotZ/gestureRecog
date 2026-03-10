"use client";

import { useState, useRef, useCallback, useEffect } from "react";
import { serial as polyfillSerial } from "web-serial-polyfill";

// Helper to check if we are on Android
const isAndroid = () => {
  return typeof navigator !== "undefined" && /Android/i.test(navigator.userAgent);
};

// Nordic UART Service (NUS) UUIDs
const NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
const NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

// Helper to get the correct serial object
const getSerial = () => {
  if (typeof navigator === "undefined") return null;
  if (isAndroid()) return polyfillSerial;
  if ("serial" in navigator) return (navigator as any).serial;
  return polyfillSerial;
};

const USB_FILTERS = [
  { usbVendorId: 0x303a, usbProductId: 0x1001 },
  { usbVendorId: 0x303a, usbProductId: 0x8000 },
  { usbVendorId: 0x10c4, usbProductId: 0xea60 },
  { usbVendorId: 0x1a86, usbProductId: 0x7523 },
];

export interface DatasetRecord {
  id: number;
  label: string;
}

export interface SerialData {
  raw: number[];
  mapped: number[];
  min: number[];
  max: number[];
  prediction?: string;
  predictionDistance?: string;
  mode?: string;
  disabled?: boolean;
  datasetList: DatasetRecord[];
  selectedSampleData?: {
    id: number;
    label: string;
    channels: number[][];
  };
}

export function useWebSerial() {
  const [isConnected, setIsConnected] = useState(false);
  const [connectionType, setConnectionType] = useState<"USB" | "BLE" | null>(null);
  const [logs, setLogs] = useState<string[]>([]);
  const [latestData, setLatestData] = useState<SerialData | null>(null);

  const portRef = useRef<any>(null); // Can be SerialPort or BLE Device
  const readerRef = useRef<ReadableStreamDefaultReader | null>(null);
  const bleCharRxRef = useRef<BluetoothRemoteGATTCharacteristic | null>(null);
  const keepReadingRef = useRef(true);

  const addLog = useCallback((msg: string) => {
    setLogs((prev) => [...prev, msg].slice(-100));
  }, []);

  const disconnect = useCallback(async () => {
    keepReadingRef.current = false;
    
    try {
      if (readerRef.current) {
        await readerRef.current.cancel().catch(console.warn);
        readerRef.current = null;
      }
      if (connectionType === "USB" && portRef.current) {
        await portRef.current.close().catch(console.warn);
      } else if (connectionType === "BLE" && portRef.current) {
        await (portRef.current as BluetoothDevice).gatt?.disconnect();
      }
      portRef.current = null;
    } catch (e) {
      console.warn("Disconnect error:", e);
    } finally {
      setIsConnected(false);
      setConnectionType(null);
      addLog("Disconnected.");
    }
  }, [addLog, connectionType]);

  const processStream = useCallback(async (reader: ReadableStreamDefaultReader) => {
    let buffer = "";
    let currentData: SerialData = {
      raw: Array(5).fill(0),
      mapped: Array(5).fill(0),
      min: Array(5).fill(0),
      max: Array(5).fill(0),
      prediction: undefined,
      predictionDistance: undefined,
      mode: "UNKNOWN",
      disabled: false,
      datasetList: [],
      selectedSampleData: undefined
    };

    try {
      while (keepReadingRef.current) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          buffer += value;
          const lines = buffer.split("\n");
          buffer = lines.pop() || "";

          for (const line of lines) {
            const trimmed = line.trim();
            if (!trimmed) continue;

            let isDataLine = false;
            if (trimmed.startsWith(">")) {
              const matchMapped = trimmed.match(/^>(\d+):(-?\d+)$/);
              const matchRaw = trimmed.match(/^>r(\d+):(-?\d+)$/);
              const matchMax = trimmed.match(/^>max(\d+):(-?\d+)$/);
              const matchMin = trimmed.match(/^>min(\d+):(-?\d+)$/);
              const matchMode = trimmed.match(/^>mode:(.+)$/);
              const matchD = trimmed.match(/^>d:(\d+)$/);
              const matchList = trimmed.match(/^>list:(\d+):(.+)$/);
              const matchDataStart = trimmed.match(/^>data_start:(\d+):(.+)$/);
              const matchDataCh = trimmed.match(/^>ch:(\d+):(.+)$/);
              const matchDataEnd = trimmed.match(/^>data_end$/);
              
              if (matchMapped) {
                currentData.mapped[parseInt(matchMapped[1])] = parseInt(matchMapped[2]);
                isDataLine = true;
              } else if (matchRaw) {
                currentData.raw[parseInt(matchRaw[1])] = parseInt(matchRaw[2]);
                isDataLine = true;
              } else if (matchMax) {
                currentData.max[parseInt(matchMax[1])] = parseInt(matchMax[2]);
                isDataLine = true;
              } else if (matchMin) {
                currentData.min[parseInt(matchMin[1])] = parseInt(matchMin[2]);
                isDataLine = true;
              } else if (matchD) {
                currentData.disabled = matchD[1] !== "0";
                isDataLine = true;
              } else if (matchMode) {
                currentData.mode = matchMode[1];
                isDataLine = true;
              } else if (matchList) {
                const id = parseInt(matchList[1]);
                const label = matchList[2];
                if (id === 1) currentData.datasetList = [];
                currentData.datasetList.push({ id, label });
                isDataLine = true;
              } else if (matchDataStart) {
                currentData.selectedSampleData = { id: parseInt(matchDataStart[1]), label: matchDataStart[2], channels: [] };
                isDataLine = true;
              } else if (matchDataCh && currentData.selectedSampleData) {
                const ch = parseInt(matchDataCh[1]);
                const data = matchDataCh[2].split(',').map(v => parseFloat(v));
                currentData.selectedSampleData.channels[ch] = data;
                isDataLine = true;
              } else if (matchDataEnd && currentData.selectedSampleData) {
                isDataLine = true;
                setLatestData({ ...currentData, datasetList: [...currentData.datasetList], selectedSampleData: { ...currentData.selectedSampleData, channels: [...currentData.selectedSampleData.channels] } });
              }
              
              if (isDataLine && !matchDataStart && !matchDataCh) {
                 setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
              }
            } else if (trimmed.startsWith("Stored items in dataset:")) {
               currentData.datasetList = [];
               isDataLine = true;
            } else if (trimmed.startsWith("Best Match:")) {
              const matchPred = trimmed.match(/Best Match: \[(.*?)\] \(Dist: ([\d.]+)\)/);
              if (matchPred) {
                currentData.prediction = matchPred[1];
                currentData.predictionDistance = matchPred[2];
                isDataLine = true;
              }
            } else if (trimmed.includes("Time:") || trimmed.startsWith("===") || trimmed.startsWith("Samples Processed:") || trimmed.startsWith("---------------------------") || trimmed.startsWith("Top ") || trimmed.startsWith(" ") || trimmed.startsWith("===========================")) {
               isDataLine = true;
            }
            
            if (!isDataLine) addLog(`> ${trimmed}`);
          }
        }
      }
    } catch (e) {
      console.error("Read error:", e);
      addLog(`Read Error: ${(e as Error).message}`);
    } finally {
      disconnect();
    }
  }, [addLog, disconnect]);

  const connectUSB = useCallback(async () => {
    try {
      const serial = getSerial();
      if (!serial) {
        addLog("Serial API not supported.");
        return;
      }
      const port = await serial.requestPort({ filters: USB_FILTERS });
      await port.open({ baudRate: 115200 });
      portRef.current = port;
      setConnectionType("USB");
      setIsConnected(true);
      keepReadingRef.current = true;
      addLog("Connected via USB.");

      const textDecoder = new TextDecoderStream();
      port.readable?.pipeTo(textDecoder.writable);
      const reader = textDecoder.readable.getReader();
      readerRef.current = reader;
      processStream(reader);
    } catch (e) {
      addLog(`USB Error: ${(e as Error).message}`);
    }
  }, [addLog, processStream]);

  const connectBLE = useCallback(async () => {
    try {
      if (!navigator.bluetooth) {
        addLog("Web Bluetooth not supported.");
        return;
      }
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [NUS_SERVICE_UUID] }],
        optionalServices: [NUS_SERVICE_UUID]
      });

      const server = await device.gatt?.connect();
      const service = await server?.getPrimaryService(NUS_SERVICE_UUID);
      const rxChar = await service?.getCharacteristic(NUS_RX_UUID);
      const txChar = await service?.getCharacteristic(NUS_TX_UUID);

      if (!rxChar || !txChar) throw new Error("NUS characteristics not found");

      bleCharRxRef.current = rxChar;
      portRef.current = device;
      setConnectionType("BLE");
      setIsConnected(true);
      keepReadingRef.current = true;
      addLog("Connected via BLE.");

      // Create a readable stream for BLE notifications
      const stream = new ReadableStream({
        start(controller) {
          txChar.startNotifications().then(() => {
            txChar.addEventListener("characteristicvaluechanged", (event: any) => {
              const value = event.target.value;
              const decoder = new TextDecoder();
              controller.enqueue(decoder.decode(value));
            });
          });
        },
        cancel() {
          txChar.stopNotifications();
        }
      });

      const reader = stream.getReader();
      readerRef.current = reader;
      processStream(reader);
      
      device.addEventListener('gattserverdisconnected', () => {
          addLog("BLE Disconnected.");
          disconnect();
      });

    } catch (e) {
      addLog(`BLE Error: ${(e as Error).message}`);
    }
  }, [addLog, processStream, disconnect]);

  const sendData = useCallback(async (data: string) => {
    try {
      const encoder = new TextEncoder();
      const payload = encoder.encode(data + "\n");
      
      if (connectionType === "USB" && portRef.current?.writable) {
        const writer = portRef.current.writable.getWriter();
        await writer.write(payload);
        writer.releaseLock();
      } else if (connectionType === "BLE" && bleCharRxRef.current) {
        // BLE NUS RX is usually Write Without Response
        await bleCharRxRef.current.writeValueWithoutResponse(payload);
      }
    } catch (e) {
      addLog(`Write Error: ${(e as Error).message}`);
    }
  }, [connectionType, addLog]);

  return { isConnected, connectionType, connectUSB, connectBLE, disconnect, logs, latestData, sendData };
}
