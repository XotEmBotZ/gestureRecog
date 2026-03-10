"use client";

import { useState, useRef, useCallback, useEffect } from "react";
import { serial as polyfillSerial } from "web-serial-polyfill";

// Helper to check if we are on Android
const isAndroid = () => {
  return typeof navigator !== "undefined" && /Android/i.test(navigator.userAgent);
};

// Helper to get the correct serial object
const getSerial = () => {
  if (typeof navigator === "undefined") return null;
  
  // On Android, native Web Serial often only supports Bluetooth.
  // We force the Polyfill (WebUSB) for wired USB support.
  if (isAndroid()) {
    console.log("Android detected: Using WebUSB Polyfill for Serial");
    return polyfillSerial;
  }

  if ("serial" in navigator) {
    return (navigator as any).serial;
  }
  
  return polyfillSerial;
};

// ESP32-S3 and common Serial converter filters
const USB_FILTERS = [
  { usbVendorId: 0x303a, usbProductId: 0x1001 }, // ESP32-S3 USB-Serial/JTAG
  { usbVendorId: 0x303a, usbProductId: 0x8000 }, // ESP32-S3 
  { usbVendorId: 0x10c4, usbProductId: 0xea60 }, // CP210x
  { usbVendorId: 0x1a86, usbProductId: 0x7523 }, // CH340
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
  const [logs, setLogs] = useState<string[]>([]);
  const [latestData, setLatestData] = useState<SerialData | null>(null);

  const portRef = useRef<SerialPort | null>(null);
  const readerRef = useRef<ReadableStreamDefaultReader | null>(null);
  const keepReadingRef = useRef(true);

  const addLog = useCallback((msg: string) => {
    setLogs((prev) => [...prev, msg].slice(-100)); // Keep last 100 logs
  }, []);

  const disconnect = useCallback(async () => {
    keepReadingRef.current = false;
    
    try {
      if (readerRef.current) {
        await readerRef.current.cancel().catch(console.warn);
        readerRef.current = null;
      }
      if (portRef.current) {
        await portRef.current.close().catch(console.warn);
        portRef.current = null;
      }
    } catch (e) {
      console.warn("Disconnect error:", e);
    } finally {
      setIsConnected(false);
      addLog("Disconnected.");
    }
  }, [addLog]);

  const connect = useCallback(async () => {
    try {
      const serial = getSerial();
      if (!serial) {
        addLog("Serial API (and polyfill) not supported in this browser.");
        return;
      }

      // Request port - include filters to help Android/WebUSB identify the device
      const port = await serial.requestPort({ filters: USB_FILTERS });
      await port.open({ baudRate: 115200 }); // Adjust based on your ESP32
      portRef.current = port;
      setIsConnected(true);
      keepReadingRef.current = true;
      addLog("Connected to device.");

      const textDecoder = new TextDecoderStream();
      const readableStreamClosed = port.readable?.pipeTo(textDecoder.writable as any).catch((e: Error) => {
        console.warn("pipeTo error:", e);
      });
      const reader = textDecoder.readable.getReader();
      readerRef.current = reader;

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
        while (port.readable && keepReadingRef.current) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value) {
            buffer += value;
            const lines = buffer.split("\n");
            buffer = lines.pop() || ""; // Keep incomplete line in buffer

            for (const line of lines) {
              const trimmed = line.trim();
              if (trimmed) {
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
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchRaw) {
                    currentData.raw[parseInt(matchRaw[1])] = parseInt(matchRaw[2]);
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchMax) {
                    currentData.max[parseInt(matchMax[1])] = parseInt(matchMax[2]);
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchMin) {
                    currentData.min[parseInt(matchMin[1])] = parseInt(matchMin[2]);
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchD) {
                    currentData.disabled = matchD[1] !== "0";
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchMode) {
                    currentData.mode = matchMode[1];
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchList) {
                    const id = parseInt(matchList[1]);
                    const label = matchList[2];
                    // If id is 1, it's a new list, clear the old one
                    if (id === 1) currentData.datasetList = [];
                    currentData.datasetList.push({ id, label });
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList] });
                  } else if (matchDataStart) {
                    currentData.selectedSampleData = {
                      id: parseInt(matchDataStart[1]),
                      label: matchDataStart[2],
                      channels: []
                    };
                    isDataLine = true;
                  } else if (matchDataCh && currentData.selectedSampleData) {
                    const ch = parseInt(matchDataCh[1]);
                    const data = matchDataCh[2].split(',').map(v => parseFloat(v));
                    currentData.selectedSampleData.channels[ch] = data;
                    isDataLine = true;
                  } else if (matchDataEnd && currentData.selectedSampleData) {
                    isDataLine = true;
                    setLatestData({ ...currentData, raw: [...currentData.raw], mapped: [...currentData.mapped], min: [...currentData.min], max: [...currentData.max], datasetList: [...currentData.datasetList], selectedSampleData: { ...currentData.selectedSampleData, channels: [...currentData.selectedSampleData.channels] } });
                  }
                } else if (trimmed.startsWith("Stored items in dataset:")) {
                   // Clear list when we start receiving list output
                   currentData.datasetList = [];
                   isDataLine = true;
                } else if (trimmed.startsWith("Best Match:")) {
                  // Example format: Best Match: [Tap] (Dist: 0.00)
                  const matchPred = trimmed.match(/Best Match: \[(.*?)\] \(Dist: ([\d.]+)\)/);
                  if (matchPred) {
                    currentData.prediction = matchPred[1];
                    currentData.predictionDistance = matchPred[2];
                    // Don't update state here, let the next frame trigger it to avoid double rendering
                    isDataLine = true;
                  } else {
                    // Fallback to just label if distance is missing
                    const matchPredOnly = trimmed.match(/Best Match: \[(.*?)\]/);
                    if (matchPredOnly) {
                      currentData.prediction = matchPredOnly[1];
                      isDataLine = true;
                    }
                  }
                } else if (trimmed.includes("Time:") || trimmed.startsWith("===") || trimmed.startsWith("Samples Processed:") || trimmed.startsWith("---------------------------") || trimmed.startsWith("Top ") || trimmed.startsWith(" ") || trimmed.startsWith("===========================")) {
                   // Parse additional info from inference bubble to hide it from console to prevent spam
                   isDataLine = true;
                }
                
                // Only log lines that are not part of the high-frequency telemetry
                if (!isDataLine) {
                  addLog(`> ${trimmed}`);
                }
              }
            }
          }
        }
      } catch (e) {
        console.error("Read error:", e);
        addLog(`Read Error: ${(e as Error).message}`);
      } finally {
        reader.releaseLock();
        disconnect();
      }
      
    } catch (error) {
      console.error(error);
      addLog(`Error: ${(error as Error).message}`);
      disconnect();
    }
  }, [addLog, disconnect]);

  const sendData = useCallback(async (data: string) => {
    if (portRef.current && portRef.current.writable) {
      try {
        const encoder = new TextEncoder();
        const writer = portRef.current.writable.getWriter();
        await writer.write(encoder.encode(data + "\n"));
        writer.releaseLock();
      } catch (e) {
        console.error("Write error:", e);
        addLog(`Write Error: ${(e as Error).message}`);
      }
    }
  }, [addLog]);

  useEffect(() => {
    const handleDisconnect = (e: Event) => {
      console.log("Device disconnected event", e);
      addLog("Device lost physically.");
      disconnect();
    };

    if ("serial" in navigator) {
      navigator.serial.addEventListener("disconnect", handleDisconnect);
    }

    return () => {
      if ("serial" in navigator) {
        navigator.serial.removeEventListener("disconnect", handleDisconnect);
      }
      disconnect(); // On unmount
    };
  }, [disconnect, addLog]);

  return { isConnected, connect, disconnect, logs, latestData, sendData };
}
