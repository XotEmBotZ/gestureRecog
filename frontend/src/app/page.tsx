"use client";

import { useState } from "react";
import { RealtimeChart } from "@/components/Chart";
import { StaticSampleChart } from "@/components/StaticSampleChart";
import { useWebSerial } from "@/hooks/useWebSerial";
import { 
  Activity, 
  Settings, 
  Play, 
  Database, 
  Trash2, 
  Plug,
  MonitorPlay,
  CircleDot,
  Cpu,
  RefreshCw,
  ChevronRight,
  Info,
  X
} from "lucide-react";
import { cn } from "@/lib/utils";

export default function Home() {
  const { isConnected, connect, disconnect, latestData, sendData } = useWebSerial();
  const [mode, setMode] = useState<"TRAIN" | "INFERENCE" | "CALIBRATE">("INFERENCE");
  const [kNeighbors, setKNeighbors] = useState(3);
  const [storeLabel, setStoreLabel] = useState("");
  const [showDetail, setShowDetail] = useState(false);
  
  const handleConnect = () => {
    if (isConnected) {
      disconnect();
    } else {
      connect();
    }
  };

  const setESPMode = (newMode: typeof mode) => {
    setMode(newMode);
    if (newMode === "INFERENCE") {
        sendData(`mode infer`);
    } else {
        sendData(`mode ${newMode.toLowerCase()}`);
    }
  };

  const handleItemClick = (id: number) => {
    sendData(`read ${id}`);
    setShowDetail(true);
  };

  const currentMode = latestData?.mode || mode;
  const isDisabled = latestData?.disabled;

  return (
    <div className="min-h-screen flex flex-col bg-background text-text-main selection:bg-accent/30 overflow-x-hidden">
      
      {/* Detail Overlay */}
      {showDetail && latestData?.selectedSampleData && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4 md:p-8">
          <div className="absolute inset-0 bg-background/80 backdrop-blur-sm" onClick={() => setShowDetail(false)} />
          <div className="glass-card w-full max-w-3xl rounded-3xl overflow-hidden relative animate-in zoom-in-95 duration-200">
            <div className="bg-panel-muted/50 p-6 border-b border-panel-border flex items-center justify-between">
              <div className="flex items-center gap-4">
                <div className="p-3 rounded-2xl bg-accent/10 border border-accent/20">
                  <Database className="text-accent w-6 h-6" />
                </div>
                <div>
                  <h3 className="text-xl font-black text-text-bright tracking-tight">
                    {latestData.selectedSampleData.label}
                  </h3>
                  <p className="text-xs font-mono text-text-muted">Storage ID: {latestData.selectedSampleData.id.toString().padStart(2, '0')}</p>
                </div>
              </div>
              <button 
                onClick={() => setShowDetail(false)}
                className="p-2 rounded-xl hover:bg-panel-border transition-colors text-text-muted hover:text-text-bright"
              >
                <X className="w-6 h-6" />
              </button>
            </div>
            
            <div className="p-6 space-y-6">
              <div className="bg-[#05070a]/50 rounded-2xl border border-panel-border p-4">
                <StaticSampleChart channels={latestData.selectedSampleData.channels} />
              </div>
              
              <div className="grid grid-cols-2 sm:grid-cols-5 gap-3">
                {latestData.selectedSampleData.channels.map((ch, i) => (
                  <div key={i} className="bg-panel-muted/30 border border-panel-border/50 rounded-xl p-3 text-center">
                    <span className="text-[10px] font-black text-text-muted uppercase block mb-1">CH {i}</span>
                    <span className="text-xs font-mono text-info">{ch.length} Samples</span>
                  </div>
                ))}
              </div>
            </div>
            
            <div className="p-6 bg-panel-muted/20 border-t border-panel-border flex justify-end">
              <button 
                onClick={() => setShowDetail(false)}
                className="btn-secondary px-8"
              >
                Close View
              </button>
            </div>
          </div>
        </div>
      )}

      {/* --- Top Navbar --- */}
      <nav className="sticky top-0 z-50 glass-card border-x-0 border-t-0 px-4 md:px-8 py-3 flex items-center justify-between">
        <div className="flex items-center gap-3 group cursor-default">
          <div className="p-2 rounded-lg bg-accent/10 border border-accent/20 group-hover:border-accent/40 transition-colors">
            <Cpu className="text-accent w-6 h-6 animate-pulse-subtle" />
          </div>
          <div>
            <h1 className="text-lg md:text-xl font-bold tracking-tight text-text-bright">ESP HANDYMAN</h1>
            <div className="text-[10px] md:text-xs font-medium text-text-muted flex items-center gap-1">
              <span className={cn("w-1.5 h-1.5 rounded-full", isConnected ? "bg-accent" : "bg-danger")} />
              {isConnected ? "OSSI-SYSTEM CONNECTED" : "OFFLINE"}
            </div>
          </div>
        </div>
        
        <div className="flex items-center gap-3">
          <button 
            onClick={handleConnect}
            className={cn(
              "btn-base flex items-center gap-2 text-sm md:text-base px-3 md:px-5",
              isConnected 
                ? "bg-danger/10 text-danger border border-danger/20 hover:bg-danger hover:text-white" 
                : "accent-gradient text-black hover:shadow-glow"
            )}
          >
            <Plug className="w-4 h-4" />
            <span className="hidden sm:inline">{isConnected ? "Disconnect" : "Connect"}</span>
          </button>
        </div>
      </nav>

      {/* --- Main Dashboard --- */}
      <main className="flex-1 p-4 md:p-6 lg:p-8 max-w-[1600px] mx-auto w-full grid grid-cols-1 lg:grid-cols-12 gap-6">
        
        {/* --- Primary Panel (Left Column) --- */}
        <section className="lg:col-span-8 space-y-6">
          
          {/* Chart Section */}
          <div className="glass-card rounded-2xl p-4 md:p-6 flex flex-col gap-4 relative overflow-hidden min-h-[450px]">
            <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-3 z-10">
              <div className="flex items-center gap-3">
                <Activity className="w-5 h-5 text-accent" />
                <h2 className="text-sm font-bold text-text-bright uppercase tracking-widest">Real-time Telemetry</h2>
              </div>
              <div className="flex items-center gap-2">
                {isDisabled ? (
                  <div className="px-3 py-1 bg-danger/10 border border-danger/20 rounded-full flex items-center gap-2">
                    <div className="w-1.5 h-1.5 bg-danger rounded-full animate-ping" />
                    <span className="text-[10px] font-bold text-danger uppercase tracking-tighter">IDLE</span>
                  </div>
                ) : (
                  <div className="px-3 py-1 bg-accent/10 border border-accent/20 rounded-full flex items-center gap-2">
                    <div className="w-1.5 h-1.5 bg-accent rounded-full animate-pulse" />
                    <span className="text-[10px] font-bold text-accent uppercase tracking-tighter">CAPTURING</span>
                  </div>
                )}
                <div className="px-3 py-1 bg-info/10 border border-info/20 rounded-full">
                  <span className="text-[10px] font-bold text-info uppercase tracking-tighter">{currentMode}</span>
                </div>
              </div>
            </div>
            
            <div className="flex-1 min-h-[300px] w-full mt-4">
              <RealtimeChart 
                latestRaw={latestData?.raw || Array(5).fill(0)} 
                latestMapped={latestData?.mapped || Array(5).fill(0)} 
              />
            </div>
          </div>

          {/* Bottom Interactive Layer */}
          <div className="grid grid-cols-1 gap-6">
            
            {/* Inference Result Card */}
            <div className={cn(
              "glass-card rounded-2xl p-6 relative overflow-hidden transition-all duration-300 group",
              isDisabled ? "opacity-60 saturate-50 grayscale-[0.5]" : "opacity-100"
            )}>
              <CircleDot className={cn(
                "absolute -right-6 -top-6 w-36 h-36 opacity-10 transition-transform group-hover:scale-110",
                isDisabled ? "text-danger" : "text-accent"
              )} />
              
              <div className="relative z-10 flex flex-col h-full min-h-[100px]">
                <div className="flex items-center gap-2 mb-4">
                  <MonitorPlay className="w-4 h-4 text-accent" />
                  <h3 className="text-[10px] font-bold uppercase tracking-[0.2em] text-text-muted">Inference Engine</h3>
                </div>
                
                <div className="flex-1 flex flex-col justify-center">
                  {isDisabled ? (
                    <div className="flex flex-col gap-1">
                      <span className="text-xl font-bold text-danger uppercase tracking-tight">System Halted</span>
                      <span className="text-xs text-text-muted italic">Sensor movement below threshold</span>
                    </div>
                  ) : (currentMode === "INFER" || currentMode === "INFERENCE") ? (
                    <div className="flex flex-col gap-1">
                      <div className="text-4xl font-black text-accent tracking-tighter mb-1 animate-in fade-in slide-in-from-bottom-2 duration-500">
                        {latestData?.prediction || "---"}
                      </div>
                      {latestData?.predictionDistance && (
                        <div className="flex items-center gap-2 text-xs font-mono text-text-muted">
                          <Info className="w-3 h-3" />
                          <span>KNN Distance: <span className="text-info font-bold">{latestData.predictionDistance}</span></span>
                        </div>
                      )}
                    </div>
                  ) : (
                    <div className="p-3 rounded-lg bg-panel-muted border border-panel-border text-xs text-text-muted italic flex items-center gap-2">
                      <Settings className="w-4 h-4" />
                      Switch to INFERENCE to see predictions
                    </div>
                  )}
                </div>
              </div>
            </div>
          </div>

          {/* Channels Visualizer Grid */}
          <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-5 gap-4">
            {[0, 1, 2, 3, 4].map((i) => {
              const raw = latestData?.raw?.[i] ?? 0;
              const mapVal = latestData?.mapped?.[i] ?? 0;
              const minVal = latestData?.min?.[i] ?? 0;
              const maxVal = latestData?.max?.[i] ?? 0;
              const percent = Math.min(100, Math.max(0, (mapVal / 4096) * 100));

              return (
                <div key={i} className="glass-card rounded-xl p-4 flex flex-col gap-3 group hover:border-accent/40 transition-colors">
                  <div className="flex justify-between items-center">
                    <span className="text-[10px] font-black text-text-muted uppercase tracking-tighter">CH {i}</span>
                    <span className="text-[10px] font-mono text-info/60">{mapVal}</span>
                  </div>
                  
                  <div className="flex flex-col gap-1">
                    <span className="text-2xl font-black text-text-bright tracking-tighter leading-none">{raw}</span>
                    <div className="h-1.5 w-full bg-panel-muted rounded-full overflow-hidden border border-panel-border">
                      <div 
                        className="h-full accent-gradient transition-all duration-300 ease-out" 
                        style={{ width: `${percent}%` }} 
                      />
                    </div>
                  </div>

                  <div className="flex items-center justify-between text-[8px] font-bold uppercase text-text-muted/60 pt-2 border-t border-panel-border/50">
                    <div className="flex flex-col">
                      <span>Min</span>
                      <span className="text-text-main">{minVal}</span>
                    </div>
                    <div className="flex flex-col items-end">
                      <span>Max</span>
                      <span className="text-text-main">{maxVal}</span>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>

        </section>

        {/* --- Sidebar Control (Right Column) --- */}
        <aside className="lg:col-span-4 space-y-6">
          
          {/* Action Hub */}
          <div className="glass-card rounded-2xl overflow-hidden">
            <div className="bg-panel-muted/50 p-4 border-b border-panel-border">
              <h3 className="text-xs font-black uppercase tracking-[0.2em] text-text-bright flex items-center gap-2">
                <Settings className="w-4 h-4 text-accent" />
                Operational Mode
              </h3>
            </div>
            <div className="p-4 grid grid-cols-1 gap-2">
              <ModeButton 
                active={currentMode === "TRAIN"} 
                onClick={() => setESPMode("TRAIN")}
                icon={<Play className="w-4 h-4" />}
                label="Train Mode"
                desc="Record gesture samples"
              />
              <ModeButton 
                active={currentMode === "INFER" || currentMode === "INFERENCE"} 
                onClick={() => setESPMode("INFERENCE")}
                icon={<MonitorPlay className="w-4 h-4" />}
                label="Inference"
                desc="Run classification engine"
              />
              <ModeButton 
                active={currentMode === "CALIBRATE"} 
                onClick={() => setESPMode("CALIBRATE")}
                icon={<Settings className="w-4 h-4" />}
                label="Calibrate"
                desc="Map absolute min/max"
              />
            </div>
          </div>

          {/* Configuration Panel */}
          <div className="glass-card rounded-2xl overflow-hidden">
            <div className="bg-panel-muted/50 p-4 border-b border-panel-border flex items-center justify-between">
              <h3 className="text-xs font-black uppercase tracking-[0.2em] text-text-bright flex items-center gap-2">
                <Settings className="w-4 h-4 text-accent" />
                System Config
              </h3>
            </div>
            <div className="p-6 space-y-6">
              <div className="space-y-2">
                <div className="flex items-center justify-between">
                  <label className="text-[10px] font-black uppercase tracking-widest text-text-muted">KNN Neighbors</label>
                  <span className="text-xs font-mono text-info">{kNeighbors}</span>
                </div>
                <div className="flex gap-2">
                  <input 
                    type="range" min="1" max="15" step="1"
                    value={kNeighbors} 
                    onChange={e => setKNeighbors(parseInt(e.target.value) || 1)}
                    className="flex-1 accent-accent cursor-pointer"
                  />
                  <button 
                    onClick={() => sendData(`k ${kNeighbors}`)}
                    className="p-2 rounded-lg bg-panel-muted border border-panel-border hover:text-accent transition-colors"
                  >
                    <RefreshCw className="w-4 h-4" />
                  </button>
                </div>
              </div>

              <div className="space-y-3 pt-4 border-t border-panel-border/30">
                <label className="text-[10px] font-black uppercase tracking-widest text-text-muted">Dataset Label</label>
                <div className="flex flex-col gap-2">
                  <input 
                    type="text" 
                    value={storeLabel} 
                    onChange={e => setStoreLabel(e.target.value)}
                    placeholder="Enter gesture name..."
                    className="bg-panel-muted border border-panel-border rounded-xl p-3 text-sm focus:outline-none focus:border-accent/50 placeholder:text-text-muted/50"
                  />
                  <button 
                    onClick={() => {
                      if(storeLabel.trim()) {
                        sendData(`store ${storeLabel}`);
                        setStoreLabel("");
                      }
                    }}
                    disabled={currentMode !== "TRAIN"}
                    className="btn-primary w-full flex items-center justify-center gap-2 mt-2"
                  >
                    <Database className="w-4 h-4" />
                    Store Current Buffer
                  </button>
                </div>
              </div>
            </div>
          </div>

          {/* Dataset Management */}
          <div className="glass-card rounded-2xl overflow-hidden flex flex-col min-h-[400px]">
             <div className="bg-panel-muted/50 p-4 border-b border-panel-border flex items-center justify-between">
              <h3 className="text-xs font-black uppercase tracking-[0.2em] text-text-bright flex items-center gap-2">
                <Database className="w-4 h-4 text-accent" />
                On-Chip Storage ({latestData?.datasetList?.length || 0})
              </h3>
              <button 
                onClick={() => sendData("list")}
                className="p-1.5 rounded-lg hover:bg-panel-border transition-colors text-accent"
              >
                <RefreshCw className="w-4 h-4" />
              </button>
            </div>
            
            <div className="flex-1 overflow-y-auto max-h-[400px]">
              {latestData?.datasetList && latestData.datasetList.length > 0 ? (
                <div className="divide-y divide-panel-border/30">
                  {latestData.datasetList.map((record) => (
                    <button 
                      key={record.id} 
                      onClick={() => handleItemClick(record.id)}
                      className="w-full p-4 flex items-center justify-between group hover:bg-panel-muted/30 transition-colors text-left"
                    >
                      <div className="flex items-center gap-3">
                        <span className="text-[10px] font-mono text-text-muted">{record.id.toString().padStart(2, '0')}</span>
                        <span className="text-sm font-bold text-text-bright tracking-tight">{record.label}</span>
                      </div>
                      <ChevronRight className="w-4 h-4 text-text-muted/0 group-hover:text-text-muted transition-all" />
                    </button>
                  ))}
                </div>
              ) : (
                <div className="p-12 flex flex-col items-center justify-center text-center gap-4">
                  <div className="p-4 rounded-full bg-panel-muted border border-panel-border">
                      <Database className="w-8 h-8 text-text-muted/20" />
                  </div>
                  <div className="space-y-1">
                    <p className="text-xs font-bold text-text-muted uppercase tracking-tighter">Database Empty</p>
                    <p className="text-[10px] text-text-muted/60 max-w-[150px]">Refresh to list records stored in SPIFFS memory.</p>
                  </div>
                </div>
              )}
            </div>

            <div className="p-4 mt-auto border-t border-panel-border/30">
              <button 
                onClick={() => {
                  if(window.confirm("Are you sure you want to format the data partition? This cannot be undone.")) {
                    sendData("clear");
                  }
                }}
                className="w-full btn-danger flex items-center justify-center gap-2"
              >
                <Trash2 className="w-4 h-4" />
                Format Data Partition
              </button>
            </div>
          </div>

        </aside>
      </main>

      {/* --- Footer / Status Bar --- */}
      <footer className="p-4 text-center">
        <p className="text-[10px] font-mono text-text-muted/40 uppercase tracking-[0.3em]">
          &copy; 2026 OSSICOLOSCOPE LABS • BUILD V2.0.4-BETA
        </p>
      </footer>
    </div>
  );
}

function ModeButton({ active, onClick, icon, label, desc }: { active: boolean, onClick: () => void, icon: React.ReactNode, label: string, desc: string }) {
  return (
    <button 
      onClick={onClick}
      className={cn(
        "flex items-center gap-4 p-4 rounded-xl border transition-all duration-300 text-left group",
        active 
          ? "bg-accent/10 border-accent/50 text-text-bright shadow-[0_0_20px_rgba(63,185,80,0.1)]" 
          : "border-panel-border text-text-muted hover:border-panel-border/80 hover:bg-panel-muted/50"
      )}
    >
      <div className={cn(
        "p-2.5 rounded-lg transition-colors",
        active ? "bg-accent text-black" : "bg-panel-muted border border-panel-border text-text-muted group-hover:text-text-main"
      )}>
        {icon}
      </div>
      <div className="flex-1">
        <div className={cn("text-xs font-bold uppercase tracking-wider", active ? "text-accent" : "text-text-bright")}>
          {label}
        </div>
        <div className="text-[10px] font-medium opacity-60 leading-tight mt-0.5">{desc}</div>
      </div>
      <ChevronRight className={cn("w-4 h-4 transition-transform", active ? "text-accent translate-x-1" : "opacity-0")} />
    </button>
  );
}
