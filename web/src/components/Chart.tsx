"use client";

import { useEffect, useState, useRef } from "react";
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from "chart.js";
import { Line } from "react-chartjs-2";

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

const MAX_DATA_POINTS = 50;

const COLORS = [
  "#ef4444", // red
  "#eab308", // yellow
  "#22c55e", // green
  "#06b6d4", // cyan
  "#3b82f6", // blue
];

export function RealtimeChart({ latestRaw = [], latestMapped = [] }: { latestRaw: number[], latestMapped: number[] }) {
  const [labels, setLabels] = useState<number[]>([]);
  const [rawDatasets, setRawDatasets] = useState<number[][]>(Array(5).fill([]));
  const [mappedDatasets, setMappedDatasets] = useState<number[][]>(Array(5).fill([]));
  const counter = useRef(0);

  // Remove prevDataString ref as we want real-time updates always
  useEffect(() => {
    if (!latestRaw || latestRaw.length === 0) return;

    setLabels((prev) => [...prev, counter.current++].slice(-MAX_DATA_POINTS));
    
    setRawDatasets((prev) => {
      const newDatasets = [...prev];
      for (let i = 0; i < 5; i++) {
        const val = latestRaw[i] ?? 0;
        newDatasets[i] = [...(newDatasets[i] || []), val].slice(-MAX_DATA_POINTS);
      }
      return newDatasets;
    });

    setMappedDatasets((prev) => {
      const newDatasets = [...prev];
      for (let i = 0; i < 5; i++) {
        const val = latestMapped[i] ?? 0;
        newDatasets[i] = [...(newDatasets[i] || []), val].slice(-MAX_DATA_POINTS);
      }
      return newDatasets;
    });

  }, [latestRaw, latestMapped]); // React strictly relies on Object.is, which now changes due to [...array] in useWebSerial

  const data = {
    labels,
    datasets: [
      ...rawDatasets.map((data, i) => ({
        label: `Raw ${i}`,
        data,
        borderColor: COLORS[i],
        backgroundColor: COLORS[i],
        borderWidth: 2,
        pointRadius: 0,
        tension: 0.1,
      })),
      ...mappedDatasets.map((data, i) => ({
        label: `Mapped ${i}`,
        data,
        borderColor: COLORS[i],
        backgroundColor: COLORS[i],
        borderWidth: 4,
        borderDash: [10, 5], // Longer dashes for mapped
        pointRadius: 0,
        tension: 0.1,
      }))
    ],
  };

  const options = {
    responsive: true,
    maintainAspectRatio: false,
    animation: {
      duration: 0 // Disable animation for performance
    },
    scales: {
      y: {
        min: 0,
        max: 4096,
        grid: {
          color: "#1e293b",
        },
        ticks: {
          color: "#64748b",
        },
      },
      x: {
        display: false, // hide x axis labels
        grid: {
          display: false,
        }
      }
    },
    plugins: {
      legend: {
        position: 'bottom' as const,
        labels: {
          color: "#e2e8f0",
          usePointStyle: true,
          boxWidth: 8,
        }
      }
    }
  };

  return (
    <div className="w-full h-full min-h-[400px]">
      <Line data={data} options={options} />
    </div>
  );
}
