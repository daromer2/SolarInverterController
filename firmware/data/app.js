/* Solar Inverter Controller SPA. Everything renders from /api/device (the
 * active profile serialized as JSON) + live state over WebSocket — no
 * device-specific code here except the SVG manifests. */
"use strict";

const $ = (s) => document.querySelector(s);
const state = {};          // live datapoints
let device = null;         // /api/device model
let manifest = null;       // svg binding manifest
let ws = null;
const pendingCmds = {};    // key -> row state element
const FIELDS = {};         // key -> {label, unit} from /api/device

/* Attach/refresh a native SVG tooltip (<title>) on an element. */
function setTip(el, text) {
  if (!text) return;
  let t = el.querySelector("title");
  if (!t) {
    t = document.createElementNS("http://www.w3.org/2000/svg", "title");
    el.insertBefore(t, el.firstChild);
  }
  t.textContent = text;
}

/* ---------------- helpers ---------------- */

function toast(msg, cls) {
  const t = $("#toast");
  t.textContent = msg;
  t.className = "show " + (cls || "");
  clearTimeout(t._h);
  t._h = setTimeout(() => (t.className = ""), 3500);
}

function fmtVal(v, dec) {
  if (v === undefined || v === null) return "—";
  if (typeof v === "string") return v;
  return Number(v).toFixed(dec !== undefined ? dec : (Math.abs(v) < 10 ? 2 : Math.abs(v) < 100 ? 1 : 0));
}

/* tiny expression evaluator: keys/numbers with + - * / (left-to-right factors,
 * standard +/- term splitting). Returns undefined if any key is missing or the
 * result isn't finite (e.g. divide by zero). */
function evalExpr(expr) {
  if (!expr) return undefined;
  let ok = true;
  const val = (tok) => {
    tok = tok.trim();
    if (tok === "") return 0;
    if (!isNaN(Number(tok))) return Number(tok);
    const v = state[tok];
    if (v === undefined || typeof v === "string") { ok = false; return 0; }
    return v;
  };
  let total = 0, sign = 1, term = null, op = "*", buf = "";
  const applyFactor = () => {
    const f = val(buf); buf = "";
    if (term === null) term = f;
    else term = op === "/" ? term / f : term * f;
  };
  const flushTerm = () => {
    if (buf !== "" || term !== null) { applyFactor(); total += sign * term; term = null; op = "*"; }
  };
  for (const c of expr) {
    if (c === "+" || c === "-") {
      if (buf === "" && term === null) { if (c === "-") sign = -sign; continue; }
      flushTerm(); sign = c === "-" ? -1 : 1;
    } else if (c === "*") { applyFactor(); op = "*"; }
    else if (c === "/") { applyFactor(); op = "/"; }
    else buf += c;
  }
  flushTerm();
  return ok && isFinite(total) ? total : undefined;
}

/* Resolve a bind value with an optional fallback source. Each source may be a
 * key or an expr and carry its own unit (so a current field can fall back to a
 * voltage field, switching "A" -> "V"). */
function resolveBind(b) {
  const trySrc = (key, expr, unit) => {
    if (key !== undefined && state[key] !== undefined) return { v: state[key], unit };
    if (expr) { const v = evalExpr(expr); if (v !== undefined) return { v, unit }; }
    return null;
  };
  const primary = trySrc(b.key, b.expr, b.unit);
  if (primary) return primary;
  if (b.fallback) {
    const f = b.fallback;
    const fb = trySrc(f.key, f.expr, f.unit !== undefined ? f.unit : b.unit);
    if (fb) return fb;
  }
  return { v: undefined, unit: b.unit };
}

async function api(path, opts) {
  const r = await fetch(path, opts);
  if (r.status === 401) { toast("Login required", "bad"); throw new Error("auth"); }
  return r;
}

function post(path, params) {
  return api(path, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: new URLSearchParams(params).toString(),
  });
}

/* ---------------- navigation ---------------- */

document.querySelectorAll("nav button").forEach((b) =>
  b.addEventListener("click", () => {
    document.querySelectorAll("nav button").forEach((x) => x.classList.remove("active"));
    document.querySelectorAll(".page").forEach((x) => x.classList.remove("visible"));
    b.classList.add("active");
    $("#page-" + b.dataset.page).classList.add("visible");
    if (b.dataset.page === "set") loadSettings();
  })
);

/* ---------------- websocket ---------------- */

function connectWs() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onmessage = (ev) => {
    const m = JSON.parse(ev.data);
    if (m.t === "state") { Object.assign(state, m.d); renderLive(); }
    else if (m.t === "conn") renderConn(m);
    else if (m.t === "cmd") onCmdResult(m);
    else if (m.t === "event") toast(m.message, "bad");
  };
  ws.onclose = () => setTimeout(connectWs, 2000);
}

function renderConn(c) {
  $("#dot-inv").className = "dot" + (c.inv ? " on" : "");
  $("#dot-wifi").className = "dot" + (c.wifi ? " on" : "");
  $("#dot-mqtt").className = "dot" + (c.mqtt ? " on" : "");
  $("#wifi-rssi").textContent = c.rssi ? c.rssi + " dBm" : "";
  const up = c.uptime, h = Math.floor(up / 3600), m2 = Math.floor((up % 3600) / 60);
  $("#sb-sys").textContent = `up ${h}h${m2}m · heap ${(c.heap / 1024).toFixed(0)}k`;
  $("#svgwrap").classList.toggle("stale", !c.inv);
}

/* ---------------- dashboard ---------------- */

async function loadDevice() {
  device = await (await api("/api/device")).json();
  $("#devlabel").textContent = `${device.deviceName} · ${device.label} · v${device.fw}`;
  document.title = device.deviceName + " · Solar";

  // SVG + manifest
  try {
    const svg = await (await fetch(device.svg)).text();
    $("#svgwrap").innerHTML = svg;
    manifest = await (await fetch(device.svg.replace(".svg", ".json"))).json();
  } catch (e) {
    $("#svgwrap").textContent = "dashboard graphics missing (upload web assets)";
  }

  for (const f of device.fields) FIELDS[f.key] = { label: f.label, unit: f.unit || "" };

  renderDpGrid();
  renderControls();
}

function renderDpGrid() {
  const grid = $("#dpgrid");
  grid.innerHTML = "";
  for (const f of device.fields) {
    if (f.key.includes("reserved")) continue;
    const div = document.createElement("div");
    div.className = "dp";
    div.title = `${f.label}${f.unit ? " (" + f.unit + ")" : ""} · MQTT: sensor/${f.key}`;
    div.innerHTML = `<div class="k">${f.label}</div><div class="v" id="dp-${f.key}">—</div>`;
    grid.appendChild(div);
  }
}

function renderLive() {
  // datapoint grid
  for (const f of device ? device.fields : []) {
    const el = document.getElementById("dp-" + f.key);
    if (el && state[f.key] !== undefined)
      el.textContent = fmtVal(state[f.key]) + (f.unit ? " " + f.unit : "");
  }

  // mode badge
  if (device && device.modeKey && state[device.modeKey] !== undefined) {
    const code = String(state[device.modeKey]);
    const mode = (device.modes || []).find((m) => m.code === code);
    const badge = $("#sb-mode");
    badge.hidden = false;
    badge.textContent = mode ? mode.label : code;
  }

  // warnings
  const active = [];
  for (const f of device ? device.fields : [])
    if (f.key.startsWith("warn_") && state[f.key] >= 1 && !f.key.includes("reserved"))
      active.push(f.label);
  $("#warncard").hidden = active.length === 0;
  $("#warnings").innerHTML = active.map((w) => `<span class="warnitem">${w}</span>`).join("");

  // svg binds
  if (manifest) {
    for (const b of manifest.binds || []) {
      const el = document.getElementById(b.el);
      if (!el) continue;
      const r = resolveBind(b);
      el.textContent = (r.v === undefined ? "—" : fmtVal(r.v, b.dec)) + (r.unit ? " " + r.unit : "");
      // hover tooltip: explicit bind title, else the field's label
      const tip = b.title || (b.key && FIELDS[b.key] ? FIELDS[b.key].label : null);
      if (tip) setTip(el, tip);
    }
    for (const f of manifest.flows || []) {
      const el = document.getElementById(f.el);
      if (!el) continue;
      if (f.mode) {  // mode-driven flow (e.g. grid active in Line mode)
        const cur = device.modeKey ? String(state[device.modeKey]) : "";
        el.classList.toggle("active", f.mode.includes(cur));
        el.classList.remove("rev");
        continue;
      }
      let v = resolveBind(f).v;
      if (f.invert && v !== undefined) v = -v;   // flip sign convention per device
      const min = f.min !== undefined ? f.min : 10;
      el.classList.toggle("active", v !== undefined && Math.abs(v) > min);
      el.classList.toggle("rev", v !== undefined && v < -min);
      if (v !== undefined && Math.abs(v) > min)
        el.style.animationDuration = Math.max(0.25, Math.min(2.5, 1200 / Math.abs(v))) + "s";
    }

    // sign-driven status labels (e.g. battery Charging/Discharging)
    for (const s of manifest.states || []) {
      const el = document.getElementById(s.el);
      if (!el) continue;
      const v = resolveBind(s).v;
      const db = s.deadband || 0;
      let st;
      if (v === undefined) st = s.zero || { text: "—", color: "#8494a5" };
      else if (v > db) st = s.pos;
      else if (v < -db) st = s.neg;
      else st = s.zero || { text: "Idle", color: "#8494a5" };
      if (st) { el.textContent = st.text; el.style.fill = st.color; }
    }
  }

  // control current values
  updateControlCurrents();
  $("#inv-age").textContent = "";
}

/* ---------------- controls ---------------- */

/* control key -> state key when they differ */
const CUR_ALIASES = {
  solar_charge: "en_solar_charge",
  ac_charge: "en_ac_charge",
  feed_to_grid: "en_feed_to_grid",
  batt_to_loads_solar_normal: "en_batt_to_loads_solar_normal",
  batt_to_loads_solar_loss: "en_batt_to_loads_solar_loss",
  batt_to_grid_solar_normal: "en_batt_to_grid_solar_normal",
  batt_to_grid_solar_loss: "en_batt_to_grid_solar_loss",
  grid_range: "input_voltage_range",
  mute_buzzer: "flag_mute_buzzer",
  mute_buzzer_standby: "flag_mute_buzzer_standby",
  mute_buzzer_battery: "flag_mute_buzzer_battery",
  generator_as_ac: "flag_generator_as_ac",
  wide_ac_range: "flag_wide_ac_range",
  ng_relay: "flag_ng_relay",
};

function curValue(key) {
  for (const k of [key, CUR_ALIASES[key], "bats_" + key, "flag_" + key]) {
    if (k && state[k] !== undefined) return state[k];
  }
  return undefined;
}

function renderControls() {
  const root = $("#controls");
  root.innerHTML = "";
  const groups = {};
  for (const s of device.sets) (groups[s.group] = groups[s.group] || []).push(s);

  for (const [group, sets] of Object.entries(groups)) {
    const g = document.createElement("div");
    g.className = "ctlgroup";
    g.innerHTML = `<h3>${group}</h3>`;
    for (const s of sets) {
      const row = document.createElement("div");
      row.className = "ctlrow";
      let input;
      if (s.type === "enum") {
        input = `<select id="in-${s.key}">${s.options.map((o, i) => `<option value="${o}">${o}</option>`).join("")}</select>
                 <button class="act" data-key="${s.key}">Set</button>`;
      } else if (s.type === "bool") {
        input = `<label class="switch"><input type="checkbox" id="in-${s.key}" data-key="${s.key}"><span class="slider"></span></label>`;
      } else if (s.type === "number") {
        input = `<input type="number" id="in-${s.key}" min="${s.min}" max="${s.max}" step="${s.step}">
                 <button class="act" data-key="${s.key}">Set</button>`;
      } else {
        input = `<input type="text" id="in-${s.key}" placeholder="value">
                 <button class="act" data-key="${s.key}">Set</button>`;
      }
      row.innerHTML = `<label>${s.label}${s.unit ? ` <span style="color:var(--muted)">(${s.unit})</span>` : ""}</label>
        <span class="cur" id="cur-${s.key}">—</span>${input}<span class="state" id="st-${s.key}"></span>`;
      g.appendChild(row);
    }
    root.appendChild(g);
  }

  root.querySelectorAll("button[data-key]").forEach((b) =>
    b.addEventListener("click", () => {
      const key = b.dataset.key;
      const val = document.getElementById("in-" + key).value;
      sendCmd(key, val);
    })
  );
  root.querySelectorAll("input[type=checkbox][data-key]").forEach((c) =>
    c.addEventListener("change", () => sendCmd(c.dataset.key, c.checked ? "1" : "0", c))
  );
}

function updateControlCurrents() {
  if (!device) return;
  for (const s of device.sets) {
    const el = document.getElementById("cur-" + s.key);
    if (!el) continue;
    let v = curValue(s.key);
    if (v === undefined) continue;
    if (s.type === "enum" && typeof v === "number" && s.options[v] !== undefined) v = s.options[v];
    if (s.type === "enum" && typeof v === "string" && /^\d+$/.test(v) && s.options[+v]) v = s.options[+v];
    if (s.type === "bool") {
      const box = document.getElementById("in-" + s.key);
      if (box && !box._touched) box.checked = v >= 1;
      v = v >= 1 ? "on" : "off";
    }
    el.textContent = typeof v === "number" ? fmtVal(v) : v;
  }
}

async function sendCmd(key, value, checkbox) {
  const def = device.sets.find((s) => s.key === key);
  if (!confirm(`Send: ${def ? def.label : key} = ${value}?`)) {
    if (checkbox) checkbox.checked = !checkbox.checked;
    return;
  }
  if (checkbox) checkbox._touched = true;
  const st = document.getElementById("st-" + key);
  if (st) { st.textContent = "sending…"; st.className = "state pend"; }
  try {
    const r = await post("/api/cmd", { key, value });
    const j = await r.json();
    if (!j.accepted) {
      if (st) { st.textContent = j.error || "rejected"; st.className = "state bad"; }
      toast(`Rejected: ${j.error || "invalid"}`, "bad");
    }
  } catch (e) {
    if (st) { st.textContent = "error"; st.className = "state bad"; }
  }
}

function onCmdResult(m) {
  if (m.key) {
    const st = document.getElementById("st-" + m.key);
    if (st) {
      st.textContent = m.status === "ok" ? "✓ accepted" : m.status;
      st.className = "state " + (m.status === "ok" ? "ok" : "bad");
    }
    toast(`${m.key} = ${m.value}: ${m.status.toUpperCase()}`, m.status === "ok" ? "ok" : "bad");
  } else {
    conLog(`< ${m.status}${m.response ? " " + m.response : ""}`, m.status === "ok" ? "rx" : "err");
  }
}

/* ---------------- console ---------------- */

function conLog(text, cls) {
  const log = $("#conlog");
  const line = document.createElement("div");
  line.className = cls;
  line.textContent = text;
  log.appendChild(line);
  log.scrollTop = log.scrollHeight;
}

$("#consend").addEventListener("click", sendRaw);
$("#coninput").addEventListener("keydown", (e) => e.key === "Enter" && sendRaw());
async function sendRaw() {
  const cmd = $("#coninput").value.trim();
  if (!cmd) return;
  conLog(`> ${cmd}`, "tx");
  $("#coninput").value = "";
  await post("/api/raw", { cmd });
}

$("#logrefresh").addEventListener("click", async () => {
  $("#devlog").textContent = await (await api("/api/log")).text();
});

/* ---------------- settings ---------------- */

const SETTING_KEYS = [
  "deviceName", "profileId", "uartRx", "uartTx", "baudOverride",
  "wifiSsid", "mqttEnabled", "mqttServer", "mqttPort", "mqttUser",
  "baseTopic", "publishIntervalS", "perFieldTopics", "legacyCompat",
  "haDiscovery", "haPrefix", "feedGuardEnabled", "feedGuardTimeoutS",
  "feedGuardFailsafeW", "ledEnabled", "ledRedPin", "ledGreenPin",
  "ledActiveLow", "authEnabled", "webUser",
];
const SECRET_KEYS = ["wifiPass", "mqttPass", "webPass", "otaPassword"];

async function loadSettings() {
  const s = await (await api("/api/settings")).json();
  const sel = $("#s-profileId");
  if (device && sel.options.length === 0)
    for (const p of device.profiles) sel.add(new Option(p.label, p.id));
  for (const k of SETTING_KEYS) {
    const el = document.getElementById("s-" + k);
    if (!el) continue;
    if (el.type === "checkbox") el.checked = !!s[k];
    else el.value = s[k] !== undefined ? s[k] : "";
  }
}

$("#btn-save").addEventListener("click", async () => {
  const out = {};
  for (const k of SETTING_KEYS) {
    const el = document.getElementById("s-" + k);
    if (!el) continue;
    out[k] = el.type === "checkbox" ? el.checked : el.type === "number" ? Number(el.value) : el.value;
  }
  for (const k of SECRET_KEYS) {
    const el = document.getElementById("s-" + k);
    if (el && el.value) out[k] = el.value;  // only send when changed
  }
  const r = await api("/api/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(out),
  });
  toast(r.ok ? "Saved — reboot to apply" : "Save failed", r.ok ? "ok" : "bad");
});

$("#btn-reboot").addEventListener("click", async () => {
  if (!confirm("Reboot the device?")) return;
  await post("/api/reboot", {});
  toast("Rebooting…");
});

$("#btn-factory").addEventListener("click", async () => {
  if (!confirm("Factory reset erases ALL settings. Continue?")) return;
  await post("/api/factoryreset", {});
  toast("Factory reset, rebooting…");
});

$("#btn-scan").addEventListener("click", scanWifi);
async function scanWifi() {
  const sel = $("#s-wifiScan");
  sel.innerHTML = "<option>scanning…</option>";
  const poll = async () => {
    const r = await (await api("/api/scan")).json();
    if (r.scanning) return setTimeout(poll, 1500);
    sel.innerHTML = "<option value=''>— pick —</option>";
    (r.networks || [])
      .sort((a, b) => b.rssi - a.rssi)
      .forEach((n) => sel.add(new Option(`${n.ssid} (${n.rssi} dBm)${n.secure ? " 🔒" : ""}`, n.ssid)));
  };
  poll();
}
$("#s-wifiScan").addEventListener("change", (e) => {
  if (e.target.value) $("#s-wifiSsid").value = e.target.value;
});

/* ---------------- OTA ---------------- */

function otaUpload(fileInput, type) {
  const f = fileInput.files[0];
  if (!f) return toast("Pick a .bin file first", "bad");
  if (!confirm(`Upload ${f.name} as ${type === "fs" ? "web assets" : "firmware"}? Device reboots after.`)) return;
  const xhr = new XMLHttpRequest();
  const form = new FormData();
  form.append("file", f);
  const prog = $("#ota-prog");
  prog.hidden = false;
  xhr.upload.onprogress = (e) => (prog.value = (e.loaded / e.total) * 100);
  xhr.onload = () => {
    prog.hidden = true;
    toast(xhr.status === 200 ? "Update OK — rebooting" : "Update failed", xhr.status === 200 ? "ok" : "bad");
  };
  xhr.onerror = () => { prog.hidden = true; toast("Upload error", "bad"); };
  xhr.open("POST", "/api/ota?type=" + type);
  xhr.send(form);
}
$("#btn-ota-fw").addEventListener("click", () => otaUpload($("#ota-fw"), "fw"));
$("#btn-ota-fs").addEventListener("click", () => otaUpload($("#ota-fs"), "fs"));

$("#btn-import").addEventListener("click", async () => {
  const f = $("#imp-file").files[0];
  if (!f) return toast("Pick a settings JSON first", "bad");
  const text = await f.text();
  const r = await api("/api/settings/import", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: text,
  });
  toast(r.ok ? "Imported — reboot to apply" : "Import failed", r.ok ? "ok" : "bad");
});

/* ---------------- boot ---------------- */

(async () => {
  await loadDevice();
  const st = await (await api("/api/state")).json();
  Object.assign(state, st);
  renderLive();
  connectWs();
})();
