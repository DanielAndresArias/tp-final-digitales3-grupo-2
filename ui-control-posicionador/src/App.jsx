import { useState, useRef, useEffect, useCallback } from "react";
import {
  Plug, PlugZap, CornerDownLeft, Home, Crosshair, Send, Trash2, AlertTriangle,
} from "lucide-react";

/*
 * Panel de control del posicionador lineal (TP Digital 3 - UNC).
 * Habla con la placa LPC1769 por Web Serial (Chrome/Edge) a 9600 8N1.
 *
 * Protocolo (lo que entiende el firmware):
 *   <numero>\n  -> ir a esa posicion en mm
 *   p\n         -> reporta "Motor: X mm   Encoder: Y mm"
 *   h\n         -> rehace el homing
 */

const BAUD = 9600;

export default function App() {
  const [connected, setConnected] = useState(false);
  const [supported] = useState(() => typeof navigator !== "undefined" && "serial" in navigator);
  const [log, setLog] = useState([]);
  const [target, setTarget] = useState("");
  const [motorMm, setMotorMm] = useState(null);
  const [encoderMm, setEncoderMm] = useState(null);
  const [status, setStatus] = useState("Sin conectar");

  const portRef = useRef(null);
  const readerRef = useRef(null);
  const logEndRef = useRef(null);

  const addLog = useCallback((dir, text) => {
    setLog((l) => [...l.slice(-200), { dir, text, t: Date.now() }]);
  }, []);

  useEffect(() => {
    logEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [log]);

  const parseLine = useCallback((line) => {
    const m = line.match(/Motor:\s*(-?\d+(?:\.\d+)?)/i);
    const e = line.match(/Encoder:\s*(-?\d+(?:\.\d+)?)/i);
    const ok = line.match(/OK\s*->\s*(-?\d+(?:\.\d+)?)/i);
    if (m) setMotorMm(parseFloat(m[1]));
    if (e) setEncoderMm(parseFloat(e[1]));
    if (ok && !m) setMotorMm(parseFloat(ok[1]));
  }, []);

  const readLoop = useCallback(async (port) => {
    const decoder = new TextDecoderStream();
    port.readable.pipeTo(decoder.writable).catch(() => {});
    const reader = decoder.readable.getReader();
    readerRef.current = reader;
    let buf = "";
    try {
      while (true) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += value;
        let idx;
        while ((idx = buf.search(/[\r\n]/)) >= 0) {
          const line = buf.slice(0, idx).trim();
          buf = buf.slice(idx + 1);
          if (line) {
            addLog("rx", line);
            parseLine(line);
          }
        }
      }
    } catch (err) {
      addLog("err", "Lectura interrumpida: " + err.message);
    }
  }, [addLog, parseLine]);

  const connect = useCallback(async () => {
    if (!supported) return;
    try {
      const port = await navigator.serial.requestPort();
      await port.open({ baudRate: BAUD, dataBits: 8, parity: "none", stopBits: 1 });
      portRef.current = port;
      setConnected(true);
      setStatus(`Conectado · ${BAUD} 8N1`);
      addLog("sys", `Puerto abierto a ${BAUD} baud`);
      readLoop(port);
    } catch (err) {
      setStatus("Error al conectar");
      addLog("err", err.message);
    }
  }, [supported, addLog, readLoop]);

  const disconnect = useCallback(async () => {
    try {
      await readerRef.current?.cancel().catch(() => {});
      await portRef.current?.close().catch(() => {});
    } finally {
      portRef.current = null;
      readerRef.current = null;
      setConnected(false);
      setStatus("Sin conectar");
      addLog("sys", "Puerto cerrado");
    }
  }, [addLog]);

  const send = useCallback(async (text) => {
    const port = portRef.current;
    if (!port?.writable) return;
    const writer = port.writable.getWriter();
    try {
      await writer.write(new TextEncoder().encode(text + "\n"));
      addLog("tx", text);
    } catch (err) {
      addLog("err", "Envio fallido: " + err.message);
    } finally {
      writer.releaseLock();
    }
  }, [addLog]);

  const sendTarget = useCallback(() => {
    const v = target.trim().replace(",", ".");
    if (v === "" || isNaN(parseFloat(v))) return;
    send(v);
  }, [target, send]);

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-200 font-sans antialiased">
      <div className="mx-auto max-w-3xl px-5 py-8">

        {/* Encabezado */}
        <header className="flex items-center justify-between gap-4 border-b border-zinc-800 pb-5">
          <div>
            <h1 className="text-lg font-semibold tracking-tight text-zinc-100">
              Posicionador lineal
            </h1>
            <p className="text-xs text-zinc-500 mt-0.5">Control por UART · LPC1769</p>
          </div>
          <div className="flex items-center gap-3">
            <span className="flex items-center gap-2 text-xs text-zinc-400">
              <span className={`h-2 w-2 rounded-full ${connected ? "bg-emerald-400" : "bg-zinc-600"}`} />
              {status}
            </span>
            {connected ? (
              <button onClick={disconnect}
                className="inline-flex items-center gap-2 rounded-md bg-zinc-800 px-3 py-2 text-sm font-medium text-zinc-200 hover:bg-zinc-700 transition-colors">
                <Plug className="h-4 w-4" /> Desconectar
              </button>
            ) : (
              <button onClick={connect} disabled={!supported}
                className="inline-flex items-center gap-2 rounded-md bg-emerald-500 px-3 py-2 text-sm font-medium text-zinc-950 hover:bg-emerald-400 transition-colors disabled:opacity-40 disabled:cursor-not-allowed">
                <PlugZap className="h-4 w-4" /> Conectar
              </button>
            )}
          </div>
        </header>

        {!supported && (
          <div className="mt-5 flex items-start gap-3 rounded-md border border-amber-500/30 bg-amber-500/10 px-4 py-3 text-sm text-amber-200">
            <AlertTriangle className="h-4 w-4 mt-0.5 shrink-0" />
            <p>Tu navegador no soporta Web Serial. Abrí esta página en <strong>Chrome</strong> o <strong>Edge</strong> (en <code>localhost</code> o HTTPS).</p>
          </div>
        )}

        {/* Lectura de posicion */}
        <section className="mt-6 grid grid-cols-2 gap-3">
          <Readout label="Motor (pasos)" value={motorMm} accent="text-emerald-400" />
          <Readout label="Encoder (real)" value={encoderMm} accent="text-sky-400" />
        </section>

        {/* Ir a posicion */}
        <section className="mt-3 rounded-lg border border-zinc-800 bg-zinc-900/50 p-4">
          <label className="block text-xs font-medium text-zinc-400 mb-2">Ir a posición (mm)</label>
          <div className="flex gap-2">
            <input
              type="text" inputMode="decimal" value={target}
              onChange={(e) => setTarget(e.target.value)}
              onKeyDown={(e) => e.key === "Enter" && sendTarget()}
              placeholder="ej: 40 o 12.5"
              disabled={!connected}
              className="flex-1 rounded-md border border-zinc-700 bg-zinc-950 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-600 outline-none focus:border-emerald-500 disabled:opacity-50 font-mono"
            />
            <button onClick={sendTarget} disabled={!connected}
              className="inline-flex items-center gap-2 rounded-md bg-emerald-500 px-4 py-2 text-sm font-medium text-zinc-950 hover:bg-emerald-400 transition-colors disabled:opacity-40 disabled:cursor-not-allowed">
              <Send className="h-4 w-4" /> Ir
            </button>
          </div>
          <div className="mt-3 flex flex-wrap gap-2">
            {[10, 25, 50, 75].map((p) => (
              <button key={p} onClick={() => { setTarget(String(p)); send(String(p)); }}
                disabled={!connected}
                className="rounded-md border border-zinc-700 px-3 py-1.5 text-xs font-mono text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors disabled:opacity-40 disabled:cursor-not-allowed">
                {p} mm
              </button>
            ))}
            <div className="ml-auto flex gap-2">
              <button onClick={() => send("p")} disabled={!connected}
                className="inline-flex items-center gap-1.5 rounded-md border border-zinc-700 px-3 py-1.5 text-xs font-medium text-zinc-300 hover:border-zinc-500 hover:text-zinc-100 transition-colors disabled:opacity-40 disabled:cursor-not-allowed">
                <Crosshair className="h-3.5 w-3.5" /> Leer posición
              </button>
              <button onClick={() => send("h")} disabled={!connected}
                className="inline-flex items-center gap-1.5 rounded-md border border-amber-600/40 px-3 py-1.5 text-xs font-medium text-amber-300 hover:bg-amber-500/10 transition-colors disabled:opacity-40 disabled:cursor-not-allowed">
                <Home className="h-3.5 w-3.5" /> Home
              </button>
            </div>
          </div>
        </section>

        {/* Consola */}
        <section className="mt-3 rounded-lg border border-zinc-800 bg-zinc-900/50">
          <div className="flex items-center justify-between border-b border-zinc-800 px-4 py-2">
            <span className="text-xs font-medium text-zinc-400">Consola serie</span>
            <button onClick={() => setLog([])}
              className="inline-flex items-center gap-1 text-xs text-zinc-500 hover:text-zinc-300 transition-colors">
              <Trash2 className="h-3.5 w-3.5" /> Limpiar
            </button>
          </div>
          <div className="h-64 overflow-y-auto px-4 py-3 font-mono text-xs leading-relaxed">
            {log.length === 0 && (
              <p className="text-zinc-600">Sin datos. Conectá la placa y mandá un comando.</p>
            )}
            {log.map((e, i) => (
              <div key={i} className="flex gap-2">
                <span className={
                  e.dir === "tx" ? "text-emerald-400"
                  : e.dir === "rx" ? "text-sky-400"
                  : e.dir === "err" ? "text-red-400"
                  : "text-zinc-500"
                }>
                  {e.dir === "tx" ? "→" : e.dir === "rx" ? "←" : "•"}
                </span>
                <span className={e.dir === "err" ? "text-red-300" : "text-zinc-300"}>{e.text}</span>
              </div>
            ))}
            <div ref={logEndRef} />
          </div>
        </section>

        <p className="mt-4 text-center text-xs text-zinc-600">
          → enviado · ← recibido · 9600 8N1
        </p>
      </div>
    </div>
  );
}

function Readout({ label, value, accent }) {
  return (
    <div className="rounded-lg border border-zinc-800 bg-zinc-900/50 p-4">
      <div className="text-xs font-medium text-zinc-500">{label}</div>
      <div className="mt-1 flex items-baseline gap-1.5">
        <span className={`font-mono text-3xl font-semibold tabular-nums ${accent}`}>
          {value === null ? "––.––" : value.toFixed(2)}
        </span>
        <span className="text-sm text-zinc-500">mm</span>
      </div>
    </div>
  );
}
