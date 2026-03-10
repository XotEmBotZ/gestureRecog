"use client";

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

const COLORS = [
  "#ef4444", // red
  "#eab308", // yellow
  "#22c55e", // green
  "#06b6d4", // cyan
  "#3b82f6", // blue
];

export function StaticSampleChart({ channels }: { channels: number[][] }) {
  if (!channels || channels.length === 0) return null;

  const labels = Array.from({ length: channels[0].length }, (_, i) => i);

  const data = {
    labels,
    datasets: channels.map((channelData, i) => ({
      label: `CH ${i}`,
      data: channelData,
      borderColor: COLORS[i],
      backgroundColor: COLORS[i],
      borderWidth: 2,
      pointRadius: 0,
      tension: 0.1,
    })),
  };

  const options = {
    responsive: true,
    maintainAspectRatio: false,
    scales: {
      y: {
        min: 0,
        max: 1.0, // Stored data is preprocessed 0.0-1.0
        grid: {
          color: "#1e293b",
        },
        ticks: {
          color: "#64748b",
        },
      },
      x: {
        display: false,
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
    <div className="w-full h-[200px]">
      <Line data={data} options={options} />
    </div>
  );
}
