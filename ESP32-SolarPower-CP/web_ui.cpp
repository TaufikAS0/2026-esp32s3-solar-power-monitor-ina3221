#include "web_ui.h"

#include <ArduinoJson.h>
#include <math.h>

#include "battery_profile.h"
#include "config.h"

namespace {

String htmlEscape(String value) {
  value.replace("&", "&amp;");
  value.replace("<", "&lt;");
  value.replace(">", "&gt;");
  value.replace("\"", "&quot;");
  return value;
}

void appendBatteryPresets(JsonArray array) {
  size_t count = 0;
  const BatteryProfilePreset* presets = batteryProfilePresets(count);
  for (size_t index = 0; index < count; ++index) {
    JsonObject item = array.createNestedObject();
    item["id"] = presets[index].id;
    item["label"] = presets[index].label;
    item["full_voltage_v"] = static_cast<float>(presets[index].fullMv) / 1000.0f;
    item["empty_voltage_v"] = static_cast<float>(presets[index].emptyMv) / 1000.0f;
  }
}

void appendBackendSenderStatus(JsonObject object, const BackendSenderRuntime& runtime) {
  object["backend_sender_enabled"] = runtime.enabled;
  object["backend_sender_state"] = runtime.state;
  object["backend_queue_depth"] = runtime.queueDepth;
  object["backend_queue_capacity"] = runtime.queueCapacity;
  object["backend_retry_batch_depth"] = runtime.retryBatchDepth;
  object["backend_dropped_samples"] = runtime.droppedSamples;
  object["backend_last_http_status"] = runtime.lastHttpStatus;
  object["backend_last_error"] = runtime.lastError;
  object["backend_last_send_at_ms"] = runtime.lastSendAtMs;
  object["backend_last_telemetry_attempt_ms"] = runtime.lastTelemetryAttemptMs;
  object["backend_last_heartbeat_attempt_ms"] = runtime.lastHeartbeatAttemptMs;
  object["backend_telemetry_due_at_ms"] = runtime.telemetryDueAtMs;
  object["backend_next_retry_in_ms"] = runtime.nextRetryInMs;
}

void appendWifiRuntimeStatus(JsonObject object, const WifiRuntime& runtime) {
  object["wifi_sta_connected_once"] = runtime.staConnectedOnce;
  object["wifi_disconnect_count"] = runtime.staDisconnectCount;
  object["wifi_reconnect_attempts"] = runtime.staReconnectAttempts;
  object["wifi_last_connect_ms"] = runtime.lastStaConnectMs;
  object["wifi_last_disconnect_ms"] = runtime.lastStaDisconnectMs;
  object["wifi_last_reconnect_attempt_ms"] = runtime.lastStaReconnectAttemptMs;
}

void appendFlowThresholdConfig(JsonObject object, const DeviceConfig& config) {
  object["solar_active_threshold_mw"] = config.solarActiveThresholdMw;
  object["load_active_threshold_mw"] = config.loadActiveThresholdMw;
  object["battery_flow_threshold_mw"] = config.batteryFlowThresholdMw;
}

String buildInaMapText(const DeviceConfig& config) {
  String map = F("0x40=INA3221, CH1=Solar ");
  map += String(config.solarShuntMilliOhms, 2);
  map += F(" mOhm, CH2=Battery ");
  map += String(config.batteryShuntMilliOhms, 2);
  map += F(" mOhm, CH3=Load ");
  map += String(config.loadShuntMilliOhms, 2);
  map += F(" mOhm");
  return map;
}

void appendInaConfig(JsonObject object, const DeviceConfig& config) {
  object["solar_shunt_milliohms"] = config.solarShuntMilliOhms;
  object["battery_shunt_milliohms"] = config.batteryShuntMilliOhms;
  object["load_shunt_milliohms"] = config.loadShuntMilliOhms;
}

String jsonNumber(float value, uint8_t decimals = 3U) {
  if (isnan(value) || isinf(value)) {
    return F("0");
  }

  return String(value, static_cast<unsigned int>(decimals));
}

void appendRuntimeConfig(JsonObject object,
                         const DeviceConfig& config,
                         const InaSensors& sensors,
                         const RawHistoryBuffer& rawHistoryBuffer,
                         const HistoryBuffer& historyBuffer,
                         const HistoryBuffer& minuteHistoryBuffer) {
  const InaTimingProfile timing = describeIna3221Timing(config.inaAveragingSamples,
                                                        config.inaBusConvTimeUs,
                                                        config.inaShuntConvTimeUs);
  String configRegisterHex = "0x";
  configRegisterHex += String(timing.configRegister, HEX);
  configRegisterHex.toUpperCase();

  object["sensor_poll_interval_ms"] = config.sensorPollIntervalMs;
  object["history_interval_ms"] = config.historyIntervalMs;
  object["chart_point_limit"] = config.chartPointLimit;
  object["history_capacity"] = historyBuffer.capacity();
  object["history_span_ms"] =
      static_cast<uint32_t>(historyBuffer.capacity()) * config.historyIntervalMs;
  object["history_seconds_capacity"] = historyBuffer.capacity();
  object["history_seconds_span_ms"] =
      static_cast<uint32_t>(historyBuffer.capacity()) * config.historyIntervalMs;
  object["history_minutes_interval_ms"] = Config::kMinuteHistoryIntervalMs;
  object["history_minutes_capacity"] = minuteHistoryBuffer.capacity();
  object["history_minutes_span_ms"] =
      static_cast<uint32_t>(minuteHistoryBuffer.capacity()) * Config::kMinuteHistoryIntervalMs;
  object["raw_history_capacity"] = rawHistoryBuffer.capacity();
  object["raw_history_oldest_seq"] = rawHistoryBuffer.oldestSequence();
  object["raw_history_latest_seq"] = rawHistoryBuffer.latestSequence();
  object["raw_history_fetch_limit"] = Config::kRawHistoryFetchLimit;
  object["i2c_clock_hz"] = Config::kI2cClockHz;
  object["ina_averaging_samples"] = timing.averagingSamples;
  object["ina_bus_conv_us"] = timing.busConvTimeUs;
  object["ina_shunt_conv_us"] = timing.shuntConvTimeUs;
  object["ina_config_register"] = timing.configRegister;
  object["ina_config_register_hex"] = configRegisterHex;
  object["ina_frame_time_us"] = timing.frameTimeUs;
  object["ina_estimated_channel_hz"] = timing.estimatedChannelRateHz;
  object["sensor_measured_hz"] = sensors.measuredReadRateHz();
  object["sensor_total_reads"] = sensors.totalReadCount();
  object["sensor_last_read_ms"] = sensors.lastReadMs();
}

void appendLedPwmConfig(JsonObject object,
                        const DeviceConfig& config,
                        const LedPwmRuntime& runtime) {
  object["led_pwm_pin"] = runtime.pin;
  object["led_pwm_channel"] = runtime.channel;
  object["led_pwm_resolution_bits"] = runtime.resolutionBits;
  object["led_pwm_enabled"] = config.ledPwmEnabled;
  object["led_pwm_inverted"] = config.ledPwmInverted;
  object["led_pwm_frequency_hz"] = config.ledPwmFrequencyHz;
  object["led_pwm_duty_percent"] = config.ledPwmDutyPercent;
  object["led_pwm_flash_enabled"] = config.ledPwmFlashEnabled;
  object["led_pwm_flash_on_ms"] = config.ledPwmFlashOnMs;
  object["led_pwm_flash_period_ms"] = config.ledPwmFlashPeriodMs;
  object["led_pwm_attached"] = runtime.attached;
  object["led_pwm_brightness_active"] = runtime.brightnessActive;
  object["led_pwm_flash_output_on"] = runtime.flashOutputOn;
  object["led_pwm_flash_cycle_ms"] = runtime.flashCycleMs;
  object["led_pwm_duty_raw"] = runtime.dutyRaw;
  object["led_pwm_duty_raw_max"] = runtime.dutyRawMax;
  object["led_pwm_signal_duty_percent"] = runtime.signalDutyPercent;
}

bool argIsTruthy(const String& value) {
  return value == "1" || value == "true" || value == "on" || value == "yes";
}

enum class HistoryViewMode : uint8_t {
  Seconds,
  Minutes
};

HistoryViewMode parseHistoryViewMode(const String& value) {
  if (value == "minutes" || value == "minute" || value == "min") {
    return HistoryViewMode::Minutes;
  }

  return HistoryViewMode::Seconds;
}

const char* historyViewModeText(HistoryViewMode viewMode) {
  return viewMode == HistoryViewMode::Minutes ? "minutes" : "seconds";
}

uint32_t historyIntervalMsForView(const DeviceConfig& config, HistoryViewMode viewMode) {
  return viewMode == HistoryViewMode::Minutes ? Config::kMinuteHistoryIntervalMs
                                              : config.historyIntervalMs;
}

const HistoryBuffer& historyBufferForView(HistoryViewMode viewMode,
                                          const HistoryBuffer& secondsBuffer,
                                          const HistoryBuffer& minuteBuffer) {
  return viewMode == HistoryViewMode::Minutes ? minuteBuffer : secondsBuffer;
}

const char* historySampleNote(HistoryViewMode viewMode) {
  return viewMode == HistoryViewMode::Minutes
             ? "Long archive stores representative 10 s bucket snapshots in FIFO order."
             : "No chart averaging. Second view stores direct history snapshots at the configured interval.";
}

const char kDashboardHtml[] PROGMEM = R"dash(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Solar Power Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    :root{--bg:#080c14;--bg-card:#0f1520;--bg-card2:#141e2e;--border:#1e2d42;--text:#dde4ef;--muted:#4a6080;--label:#3a5070;--solar:#f59e0b;--solar-glow:0 0 24px rgba(245,158,11,.35);--bat:#10b981;--bat-glow:0 0 24px rgba(16,185,129,.35);--blue:#38bdf8;--blue-dim:rgba(56,189,248,.12);--s-charging:#10b981;--s-solar:#38bdf8;--s-combo:#a855f7;--s-idle:#334155;--s-error:#ef4444}
    *{box-sizing:border-box;margin:0;padding:0}
    body{background:var(--bg);color:var(--text);font-family:'Courier New','Consolas',monospace;min-height:100vh;padding:20px}
    .card{background:var(--bg-card);border:1px solid var(--border);border-radius:12px;padding:18px 20px;transition:border-color .3s,box-shadow .3s}
    .card-solar-on{border-color:var(--solar);box-shadow:var(--solar-glow)}
    .card-load-on{border-color:var(--blue);box-shadow:0 0 24px rgba(56,189,248,.32)}
    .card-solar-warn,.card-bat-warn,.card-load-warn{border-color:var(--s-error);box-shadow:0 0 18px rgba(239,68,68,.16)}
    .card-bat-charge{border-color:var(--bat);box-shadow:var(--bat-glow)}
    .card-bat-discharge{border-color:var(--s-error);box-shadow:0 0 24px rgba(239,68,68,.28)}
    .card-head{display:flex;justify-content:space-between;align-items:center;gap:10px}
    .card-linkbtn{background:transparent;border:1px solid var(--border);color:var(--muted);border-radius:999px;padding:4px 10px;font-family:'Courier New',monospace;font-size:.6rem;letter-spacing:.08em;cursor:pointer}
    .card-linkbtn:hover{border-color:var(--blue);color:var(--blue)}
    .lbl{font-size:.6rem;letter-spacing:.15em;text-transform:uppercase;color:var(--label);margin-bottom:8px}
    .big-val{font-size:2.2rem;font-weight:700;letter-spacing:-.02em;line-height:1}
    .sub{font-size:.72rem;color:var(--muted);margin-top:4px}
    @keyframes flash{0%{color:#fff}100%{color:var(--text)}} .flashed{animation:flash .4s ease-out forwards}
    .topbar{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;padding-bottom:16px;border-bottom:1px solid var(--border)}
    .topbar-title{font-size:1.25rem;font-weight:700;color:#fff}
    .topbar-sub{font-size:.7rem;color:var(--muted);margin-top:3px}
    .topbar-right{display:flex;align-items:center;gap:10px}
    @keyframes pulse{0%,100%{opacity:1;transform:scale(1)}50%{opacity:.35;transform:scale(1.35)}}
    .live-dot{font-size:.85rem;color:var(--bat);animation:pulse 1.5s ease-in-out infinite}
    .live-dot.error{color:var(--s-error)}
    .state-badge{font-size:.68rem;font-weight:700;letter-spacing:.1em;padding:4px 12px;border-radius:20px;border:1px solid currentColor;background:transparent}
    .badge-charging{color:var(--s-charging)} .badge-solar{color:var(--s-solar)} .badge-battery{color:var(--s-error)} .badge-combo{color:var(--s-combo)} .badge-idle{color:var(--s-idle)} .badge-error{color:var(--s-error)}
    .cards-row,.cfg-grid{display:grid;gap:14px;margin-bottom:14px;grid-template-columns:repeat(4,1fr)}
    @media (max-width:900px){.cards-row,.cfg-grid{grid-template-columns:repeat(2,1fr)}}
    @media (max-width:500px){.cards-row,.cfg-grid{grid-template-columns:1fr}.topbar{flex-direction:column;align-items:flex-start;gap:10px}}
    .arc-svg{width:100%;max-width:120px;display:block;margin:8px auto 10px}
    .flow-card,.chart-card{margin-bottom:14px}
    .chart-toolbar{display:flex;justify-content:space-between;align-items:flex-start;gap:14px;flex-wrap:wrap}
    .chart-view-switch{display:flex;gap:8px;flex-wrap:wrap}
    .chart-mode-btn{background:var(--bg-card2);border:1px solid var(--border);color:var(--muted);border-radius:999px;padding:7px 12px;font-family:'Courier New',monospace;font-size:.7rem;cursor:pointer;transition:border-color .2s,color .2s,background .2s}
    .chart-mode-btn.active{border-color:var(--blue);color:var(--blue);background:rgba(56,189,248,.12)}
    .chart-meta{display:flex;flex-direction:column;gap:4px}
    .inp{width:100%;background:var(--bg);border:1px solid var(--border);border-radius:6px;padding:8px 10px;color:var(--text);font-family:'Courier New',monospace;font-size:.8rem;outline:none}
    .inp:focus{border-color:var(--blue)}
    .btn-primary,.btn-secondary,.btn-danger{border-radius:6px;padding:8px 14px;font-family:'Courier New',monospace;font-size:.75rem;cursor:pointer}
    .btn-primary{background:var(--blue-dim);border:1px solid var(--blue);color:var(--blue);letter-spacing:.05em}
    .btn-primary:hover{background:rgba(56,189,248,.2)}
    .btn-secondary{background:var(--bg-card2);border:1px solid var(--border);color:var(--muted)}
    .btn-secondary:hover{border-color:var(--text);color:var(--text)}
    .btn-danger{background:rgba(239,68,68,.1);border:1px solid var(--s-error);color:var(--s-error)}
    .btn-secondary[disabled]{opacity:.45;cursor:not-allowed}
    @keyframes flow-h{to{stroke-dashoffset:-20}}
    @keyframes flow-down{to{stroke-dashoffset:-18}}
    @keyframes flow-up{to{stroke-dashoffset:-18}}
    @keyframes pulse-blue{0%,100%{opacity:.8;filter:drop-shadow(0 0 3px rgba(56,189,248,.3))}50%{opacity:1;filter:drop-shadow(0 0 8px rgba(56,189,248,.65))}}
    @keyframes pulse-green{0%,100%{opacity:.8;filter:drop-shadow(0 0 3px rgba(16,185,129,.35))}50%{opacity:1;filter:drop-shadow(0 0 9px rgba(16,185,129,.7))}}
    @keyframes pulse-red{0%,100%{opacity:.75;filter:drop-shadow(0 0 3px rgba(239,68,68,.4))}50%{opacity:1;filter:drop-shadow(0 0 10px rgba(239,68,68,.9))}}
    .flow-on{stroke-dasharray:6 4;animation:flow-h .5s linear infinite;opacity:1}
    .flow-load{stroke-dasharray:6 4;animation:flow-h .42s linear infinite,pulse-blue 1s ease-in-out infinite;opacity:1;filter:drop-shadow(0 0 5px rgba(56,189,248,.45))}
    .flow-down{stroke-dasharray:6 4;animation:flow-down .45s linear infinite,pulse-green .95s ease-in-out infinite;opacity:1;filter:drop-shadow(0 0 5px rgba(16,185,129,.5))}
    .flow-up{stroke-dasharray:8 4;animation:flow-up .3s linear infinite,pulse-red .8s ease-in-out infinite;opacity:1;filter:drop-shadow(0 0 7px rgba(239,68,68,.75))}
    .flow-off{stroke-dasharray:3 8;animation:none;opacity:.08}
    .flow-err{stroke-dasharray:4 6;animation:pulse-red .8s ease-in-out infinite;opacity:.95;filter:drop-shadow(0 0 7px rgba(239,68,68,.7))}
    .flow-layout{display:grid;grid-template-columns:minmax(0,1fr) 320px;gap:18px;align-items:stretch;margin-top:10px}
    .flow-stage{min-width:0;display:flex;align-items:center;justify-content:center;padding:2px 0 8px}
    .flow-eff{background:var(--bg-card2);border:1px solid var(--border);border-radius:10px;padding:14px 16px;display:flex;flex-direction:column;justify-content:flex-start;gap:12px;width:100%;max-width:320px;justify-self:end}
    .flow-eff-top{display:flex;flex-direction:column;gap:4px}
    .flow-eff-title{font-size:.68rem;letter-spacing:.12em;text-transform:uppercase;color:var(--label)}
    .flow-eff-mode{font-size:.72rem;color:var(--muted);min-height:1em}
    .flow-eff-val{font-size:2rem;font-weight:700;line-height:1;color:var(--muted)}
    .flow-eff-loss{font-size:.78rem;color:var(--muted)}
    .flow-mini{height:16px;background:var(--bg);border:1px solid var(--border);border-radius:999px;overflow:hidden;display:flex}
    .flow-mini-seg{height:100%;display:flex;align-items:center;justify-content:center;font-size:.52rem;font-weight:700;letter-spacing:.08em;white-space:nowrap;transition:width .5s ease,opacity .3s ease}
    .flow-mini-legend{display:flex;flex-wrap:wrap;gap:10px;font-size:.64rem;color:var(--muted)}
    .flow-mini-legend strong{font-weight:700}
    .flow-controls{margin-top:14px;padding-top:14px;border-top:1px solid var(--border);display:grid;grid-template-columns:minmax(220px,.9fr) minmax(0,1.6fr);gap:16px;align-items:start}
    .flow-controls-copy{display:flex;flex-direction:column;gap:6px}
    .flow-setup{display:flex;flex-direction:column;gap:8px}
    .flow-grid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px}
    .analysis-grid-bg{position:absolute;inset:0;pointer-events:none;background-image:linear-gradient(#1e2d42 1px,transparent 1px),linear-gradient(90deg,#1e2d42 1px,transparent 1px);background-size:32px 32px;opacity:.15}
    .analysis-top{display:grid;grid-template-columns:180px 1fr;gap:20px;margin-bottom:16px;position:relative;align-items:center}
    .analysis-bottom{display:grid;grid-template-columns:1fr 1fr;gap:10px;position:relative}
    .analysis-tiles{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px;position:relative}
    .atile{background:var(--bg-card2);border:1px solid var(--border);border-radius:8px;padding:12px 14px;position:relative;overflow:hidden}
    .atile:after{content:'';position:absolute;top:0;left:0;right:0;height:2px;border-radius:8px 8px 0 0}
    .atile-solar:after{background:#f59e0b}.atile-load:after{background:#38bdf8}.atile-bat:after{background:#10b981}.atile-loss:after{background:#ef4444}
    .dist-seg{height:100%;display:flex;align-items:center;justify-content:center;font-size:.6rem;font-weight:700;letter-spacing:.08em;overflow:hidden;white-space:nowrap;border-right:1px solid var(--bg-card);transition:width .8s cubic-bezier(.4,0,.2,1)}
    .legend-item{display:flex;align-items:center;gap:5px;font-size:.65rem;color:var(--muted)} .ldot{width:6px;height:6px;border-radius:50%;flex-shrink:0}
    .srow{display:flex;justify-content:space-between;align-items:center;padding:5px 0;border-bottom:1px solid var(--border)}
    .skey{display:flex;align-items:center;gap:6px;font-size:.68rem;color:var(--muted)} .sdot{width:5px;height:5px;border-radius:50%;flex-shrink:0} .sval{font-size:.78rem;font-weight:700}
    .brow{display:flex;justify-content:space-between;align-items:center} .bkey{font-size:.68rem;color:var(--muted)} .bval{font-size:.8rem;font-weight:700}
    .bat-setup{display:none;margin-top:12px;padding-top:12px;border-top:1px solid var(--border)}
    .bat-setup.open{display:block}
    .bat-grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}
    .bat-grid .inp,.bat-setup select{width:100%}
    .bat-setup select{background:var(--bg);border:1px solid var(--border);border-radius:6px;padding:8px 10px;color:var(--text);font-family:'Courier New',monospace;font-size:.8rem;outline:none}
    .bat-setup select:focus{border-color:var(--blue)}
    @keyframes eff-glow{0%,100%{filter:drop-shadow(0 0 3px rgba(16,185,129,.3))}50%{filter:drop-shadow(0 0 10px rgba(16,185,129,.7))}} #eff-arc{animation:eff-glow 2.5s ease-in-out infinite}
    @media (max-width:900px){.analysis-tiles{grid-template-columns:repeat(2,1fr)!important}}
    @media (max-width:1100px){.flow-layout{grid-template-columns:1fr}}
    @media (max-width:1100px){.flow-controls{grid-template-columns:1fr}}
    @media (max-width:900px){.flow-grid{grid-template-columns:1fr}}
    @media (max-width:700px){.analysis-top,.analysis-bottom{grid-template-columns:1fr!important}}
    @media (max-width:500px){.analysis-tiles{grid-template-columns:1fr!important}.bat-grid{grid-template-columns:1fr}}
  </style>
</head>
<body>
  <header class="topbar">
    <div class="topbar-left">
      <div class="topbar-title">Solar Power Monitor</div>
      <div class="topbar-sub"><span id="fw">Firmware --</span> | Build <span id="build-stamp">--</span> | IP <span id="ip">--</span> | Mode <span id="hdr-mode">--</span> | Device <span id="hdr-device">--</span> | <span id="hdr-interval">--</span> | updated <span id="last-upd">--</span></div>
    </div>
    <div class="topbar-right">
      <span id="live-dot" class="live-dot">o</span>
      <span id="state-badge" class="state-badge badge-idle">IDLE</span>
    </div>
  </header>

  <div class="cards-row">
    <div class="card" id="c-solar">
      <div class="card-head">
        <div class="lbl" style="margin-bottom:0">Solar Panel</div>
        <button class="card-linkbtn" id="toggle-solar-setup" type="button">Sensor Setup</button>
      </div>
      <svg class="arc-svg" viewBox="0 0 120 70">
        <path d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#1e2d42" stroke-width="8" stroke-linecap="round"/>
        <path id="arc-solar" d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#4a6080" stroke-width="8" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .5s ease,stroke .3s ease"/>
        <text x="60" y="58" text-anchor="middle" fill="#dde4ef" font-size="11" font-family="Courier New" id="arc-solar-txt">0.0V</text>
      </svg>
      <div class="big-val" id="v-solar">0.00 V</div>
      <div class="sub" id="i-solar">Current: -- mA</div>
      <div class="sub" id="p-solar">Power: -- mW</div>
      <div class="sub" id="solar-bus">Bus: -- V</div>
      <div class="sub" id="solar-shunt-line">Shunt: -- mOhm</div>
      <div class="bat-setup" id="solar-setup">
        <div class="sub">Kalibrasi shunt INA3221 untuk channel solar panel.</div>
        <div class="bat-grid" style="grid-template-columns:1fr">
          <input class="inp" id="solar-shunt-mo" type="number" step="0.1" min="0.1" max="1000" placeholder="Solar shunt (mOhm)">
        </div>
        <button class="btn-primary" id="save-solar-shunt" type="button" style="margin-top:10px;width:100%">Save Solar Sensor</button>
      </div>
    </div>

    <div class="card" id="c-bat">
      <div class="card-head">
        <div class="lbl" style="margin-bottom:0">Battery</div>
        <button class="card-linkbtn" id="toggle-bat-setup" type="button">Battery Setup</button>
      </div>
      <svg class="arc-svg" viewBox="0 0 120 70">
        <path d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#1e2d42" stroke-width="8" stroke-linecap="round"/>
        <path id="arc-bat" d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#4a6080" stroke-width="8" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .5s ease,stroke .3s ease"/>
        <text x="60" y="58" text-anchor="middle" fill="#dde4ef" font-size="11" font-family="Courier New" id="arc-bat-txt">N/A</text>
      </svg>
      <div class="big-val" id="v-bat">0.00 V</div>
      <div class="sub" id="bat-pct">Charge: N/A</div>
      <div class="sub" id="i-bat">Current: -- mA</div>
      <div class="sub" id="p-bat">Power: -- mW</div>
      <div class="sub" id="bat-bus">Bus: -- V | Shunt: -- mV</div>
      <div class="sub" id="bat-shunt-line">INA shunt: -- mOhm</div>
      <div class="sub" id="bat-profile-line">Profile: --</div>
      <div class="bat-setup" id="bat-setup">
        <div class="sub">Choose a preset, then optionally override full and empty voltage.</div>
        <select id="bat-profile-sel" style="margin-top:8px"></select>
        <div class="bat-grid">
          <input class="inp" id="bat-full" type="number" step="0.01" min="0.5" max="60" placeholder="Full voltage (V)">
          <input class="inp" id="bat-empty" type="number" step="0.01" min="0.5" max="60" placeholder="Empty voltage (V)">
        </div>
        <div class="sub" style="margin-top:10px">Kalibrasi shunt INA3221 untuk channel battery.</div>
        <div class="bat-grid" style="grid-template-columns:1fr">
          <input class="inp" id="bat-shunt-mo" type="number" step="0.1" min="0.1" max="1000" placeholder="Battery shunt (mOhm)">
        </div>
        <button class="btn-primary" id="save-bat-profile" type="button" style="margin-top:10px;width:100%">Save Battery Profile</button>
        <button class="btn-secondary" id="save-bat-shunt" type="button" style="margin-top:8px;width:100%">Save Battery Sensor</button>
      </div>
    </div>

    <div class="card" id="c-load">
      <div class="card-head">
        <div class="lbl" style="margin-bottom:0">Load Output</div>
        <button class="card-linkbtn" id="toggle-load-setup" type="button">Sensor Setup</button>
      </div>
      <svg class="arc-svg" viewBox="0 0 120 70">
        <path d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#1e2d42" stroke-width="8" stroke-linecap="round"/>
        <path id="arc-load" d="M10,65 A55,55 0 0,1 110,65" fill="none" stroke="#4a6080" stroke-width="8" stroke-linecap="round" stroke-dasharray="172.8" stroke-dashoffset="172.8" style="transition:stroke-dashoffset .5s ease,stroke .3s ease"/>
        <text x="60" y="58" text-anchor="middle" fill="#dde4ef" font-size="11" font-family="Courier New" id="arc-load-txt">0.0V</text>
      </svg>
      <div class="big-val" id="v-load">0.00 V</div>
      <div class="sub" id="i-load">Current: -- mA</div>
      <div class="sub" id="p-load">Power: -- mW</div>
      <div class="sub" id="load-bus">Bus: -- V | Shunt: -- mV</div>
      <div class="sub" id="load-shunt-line">Shunt: -- mOhm</div>
      <div class="bat-setup" id="load-setup">
        <div class="sub">Kalibrasi shunt INA3221 untuk channel load.</div>
        <div class="bat-grid" style="grid-template-columns:1fr">
          <input class="inp" id="load-shunt-mo" type="number" step="0.1" min="0.1" max="1000" placeholder="Load shunt (mOhm)">
        </div>
        <button class="btn-primary" id="save-load-shunt" type="button" style="margin-top:10px;width:100%">Save Load Sensor</button>
      </div>
    </div>

    <div class="card" id="c-flow">
      <div class="lbl">Battery Flow</div>
      <div class="big-val" id="flow-val" style="margin-top:28px;font-size:1.6rem">--</div>
      <div class="sub" style="margin-top:12px" id="flow-arrow">--</div>
      <div class="sub" id="flow-detail">--</div>
      <div class="sub" id="flow-warn">--</div>
    </div>
  </div>

  <div class="card flow-card">
    <div class="lbl">Power Flow</div>
    <div class="flow-layout">
      <div class="flow-stage">
        <svg id="flow-svg" viewBox="0 0 470 250" xmlns="http://www.w3.org/2000/svg" style="width:100%;height:250px;display:block;overflow:visible">
          <defs>
            <marker id="arr-solar" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#f59e0b"/></marker>
            <marker id="arr-bat-in" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#10b981"/></marker>
            <marker id="arr-bat-out" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#ef4444"/></marker>
            <marker id="arr-load" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#38bdf8"/></marker>
            <marker id="arr-error" markerWidth="6" markerHeight="6" refX="3" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="#ef4444"/></marker>
          </defs>
          <rect id="node-solar-box" x="8" y="48" width="102" height="46" rx="8" fill="#1a1f0a" stroke="#4a6080" stroke-width="1.8"/>
          <text id="node-solar-text" x="59" y="76" text-anchor="middle" fill="#4a6080" font-size="11" font-family="Courier New">SOLAR</text>
          <line id="line-solar" x1="110" y1="71" x2="185" y2="71" stroke="#f59e0b" stroke-width="2.5" stroke-linecap="round" marker-end="url(#arr-solar)" class="flow-off"/>
          <rect x="187" y="48" width="90" height="46" rx="8" fill="#0d1a2a" stroke="#38bdf8" stroke-width="1.8"/>
          <text x="232" y="76" text-anchor="middle" fill="#38bdf8" font-size="11" font-family="Courier New">PMIC</text>
          <line id="line-load" x1="277" y1="71" x2="350" y2="71" stroke="#38bdf8" stroke-width="2.5" stroke-linecap="round" marker-end="url(#arr-load)" class="flow-off"/>
          <rect id="node-load-box" x="352" y="48" width="110" height="46" rx="8" fill="#0a1a1a" stroke="#4a6080" stroke-width="1.8"/>
          <text id="node-load-text" x="407" y="76" text-anchor="middle" fill="#4a6080" font-size="11" font-family="Courier New">LOAD</text>
          <line id="line-bat-charge" x1="219" y1="110" x2="219" y2="150" stroke="#10b981" stroke-width="2.8" stroke-linecap="round" marker-end="url(#arr-bat-in)" class="flow-off"/>
          <line id="line-bat-discharge" x1="245" y1="150" x2="245" y2="110" stroke="#ef4444" stroke-width="3" stroke-linecap="round" marker-end="url(#arr-bat-out)" class="flow-off"/>
          <rect id="node-bat-box" x="162" y="160" width="140" height="44" rx="8" fill="#101622" stroke="#4a6080" stroke-width="1.8"/>
          <text id="node-bat-text" x="232" y="188" text-anchor="middle" fill="#4a6080" font-size="11" font-family="Courier New">BATTERY</text>
          <text id="lbl-solar-w" x="148" y="64" text-anchor="middle" fill="#f59e0b" font-size="9" opacity=".85">--mW</text>
          <text id="lbl-load-w" x="314" y="64" text-anchor="middle" fill="#38bdf8" font-size="9" opacity=".85">--mW</text>
          <text id="lbl-bat-w" x="326" y="171" text-anchor="start" fill="#4a6080" font-size="9" opacity=".85">--mW</text>
        </svg>
      </div>
      <div class="flow-eff">
        <div class="flow-eff-top">
          <div class="flow-eff-title">Path Efficiency</div>
          <div id="flow-eff-mode" class="flow-eff-mode">Analysis Unavailable</div>
        </div>
        <div id="flow-eff-val" class="flow-eff-val">N/A</div>
        <div id="flow-eff-loss" class="flow-eff-loss">Loss N/A</div>
        <div class="flow-mini">
          <div id="flow-mini-load" class="flow-mini-seg" style="background:rgba(56,189,248,.25);color:#38bdf8;width:0%">LOAD</div>
          <div id="flow-mini-bat" class="flow-mini-seg" style="background:rgba(16,185,129,.25);color:#10b981;width:0%">BAT</div>
          <div id="flow-mini-loss" class="flow-mini-seg" style="background:rgba(239,68,68,.2);color:#ef4444;width:0%">LOSS</div>
        </div>
        <div class="flow-mini-legend">
          <span>Load <strong id="flow-mini-load-txt">--</strong></span>
          <span>Battery <strong id="flow-mini-bat-txt">--</strong></span>
          <span>Loss <strong id="flow-mini-loss-txt">--</strong></span>
        </div>
      </div>
    </div>
    <div class="flow-controls">
      <div class="flow-controls-copy">
        <div class="flow-eff-title">Flow Thresholds</div>
        <div class="sub">Minimum power before solar, battery, and load flow are considered active.</div>
        <div class="sub" id="flow-edit-note">Changes save only after pressing the button.</div>
        <div class="sub" id="flow-threshold-note">Active thresholds: --</div>
      </div>
      <div class="flow-setup">
        <div class="flow-grid">
          <input class="inp" id="flow-solar-threshold" type="number" min="0" max="100000" step="1" placeholder="Solar mW">
          <input class="inp" id="flow-battery-threshold" type="number" min="0" max="100000" step="1" placeholder="Battery mW">
          <input class="inp" id="flow-load-threshold" type="number" min="0" max="100000" step="1" placeholder="Load mW">
        </div>
        <button class="btn-primary" id="save-flow" type="button" style="width:100%">Save Flow Thresholds</button>
      </div>
    </div>
  </div>

  <div class="card chart-card">
    <div class="chart-toolbar">
      <div class="chart-meta">
        <div class="lbl" style="margin-bottom:0">History Scope</div>
        <div class="sub" id="history-mode-note">Grid view: waiting for runtime config...</div>
        <div class="sub" id="history-source-note">History will be stored in this browser to keep ESP load light.</div>
      </div>
      <div class="chart-view-switch">
        <button class="chart-mode-btn active" id="history-live-logs" type="button">Live Logs</button>
        <button class="chart-mode-btn active" type="button" data-history-scale="1s">1s</button>
        <button class="chart-mode-btn" type="button" data-history-scale="3s">3s</button>
        <button class="chart-mode-btn" type="button" data-history-scale="10s">10s</button>
        <button class="chart-mode-btn" type="button" data-history-scale="1min">1min</button>
        <button class="chart-mode-btn" type="button" data-history-scale="10min">10min</button>
        <button class="chart-mode-btn" id="history-lock-y" type="button">Lock Y-Axis</button>
        <button class="chart-mode-btn" id="history-clear-browser" type="button">Clear Browser History</button>
      </div>
    </div>
  </div>

  <div class="card chart-card">
    <div class="lbl">Power History</div>
    <div style="height:180px;margin-top:10px"><canvas id="powerChart" style="cursor:crosshair"></canvas></div>
  </div>

  <div class="card chart-card">
    <div class="lbl">Voltage History</div>
    <div style="height:180px;margin-top:10px"><canvas id="voltageChart" style="cursor:crosshair"></canvas></div>
  </div>

  <div class="card chart-card">
    <div class="lbl">Current History</div>
    <div style="height:180px;margin-top:10px"><canvas id="currentChart" style="cursor:crosshair"></canvas></div>
  </div>

  <div class="card analysis-card" style="margin-bottom:14px;position:relative;overflow:hidden;">
    <div class="analysis-grid-bg"></div>
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;position:relative;">
      <div>
        <div class="lbl" style="margin-bottom:2px">Power Analysis</div>
        <div id="analysis-mode" class="sub" style="margin-top:0">Analysis unavailable</div>
      </div>
      <button class="btn-secondary" id="reset-session" style="padding:3px 10px;font-size:.62rem;">Reset Session</button>
    </div>
    <div class="analysis-top">
      <div style="display:flex;flex-direction:column;align-items:center;">
        <div style="position:relative;width:160px;height:90px;">
          <svg viewBox="0 0 160 90" style="width:160px;height:90px;">
            <path d="M16,80 A64,64 0 0,1 144,80" fill="none" stroke="#1e2d42" stroke-width="10" stroke-linecap="round"/>
            <path id="eff-arc" d="M16,80 A64,64 0 0,1 144,80" fill="none" stroke="#4a6080" stroke-width="10" stroke-linecap="round" stroke-dasharray="201.1" stroke-dashoffset="201.1" style="transition:stroke-dashoffset 1s cubic-bezier(.4,0,.2,1),stroke .3s ease"/>
          </svg>
          <div style="position:absolute;bottom:0;left:50%;transform:translateX(-50%);text-align:center;">
            <div id="eff-pct" class="big-val" style="font-size:1.8rem;line-height:1;color:var(--muted)">N/A</div>
          </div>
        </div>
        <span class="lbl" style="margin-top:4px;margin-bottom:0;">Path Efficiency</span>
      </div>
      <div style="display:flex;flex-direction:column;gap:10px;">
        <div class="lbl" style="margin-bottom:0;">Power Distribution</div>
        <div style="height:26px;background:var(--bg);border:1px solid var(--border);border-radius:4px;overflow:hidden;display:flex;">
          <div id="bar-load" class="dist-seg" style="background:rgba(56,189,248,.25);color:#38bdf8;width:0%">LOAD</div>
          <div id="bar-bat" class="dist-seg" style="background:rgba(16,185,129,.25);color:#10b981;width:0%">BAT</div>
          <div id="bar-loss" class="dist-seg" style="background:rgba(239,68,68,.20);color:#ef4444;width:0%">LOSS</div>
        </div>
        <div style="display:flex;flex-wrap:wrap;gap:12px;">
          <div class="legend-item"><div class="ldot" style="background:#38bdf8"></div><span>Load <strong id="leg-load-mw" style="color:#38bdf8">N/A</strong> (<span id="leg-load-pct">--</span>)</span></div>
          <div class="legend-item"><div id="ldot-bat" class="ldot" style="background:#10b981"></div><span>Battery <strong id="leg-bat-mw" style="color:#10b981">N/A</strong> (<span id="leg-bat-pct">--</span>)</span></div>
          <div class="legend-item"><div class="ldot" style="background:#ef4444"></div><span>Loss <strong id="leg-loss-mw" style="color:#ef4444">N/A</strong> (<span id="leg-loss-pct">--</span>)</span></div>
        </div>
      </div>
    </div>
    <div class="analysis-tiles">
      <div class="atile atile-solar">
        <div class="lbl">Solar Input</div>
        <div id="an-p-solar" class="big-val" style="font-size:1.4rem;color:#f59e0b;">N/A</div>
        <div id="an-iv-solar" class="sub">N/A</div>
      </div>
      <div class="atile atile-load">
        <div class="lbl">Load Output</div>
        <div id="an-p-load" class="big-val" style="font-size:1.4rem;color:#38bdf8;">N/A</div>
        <div id="an-iv-load" class="sub">N/A</div>
      </div>
      <div class="atile atile-bat">
        <div class="lbl" id="an-bat-lbl">Battery</div>
        <div id="an-p-bat" class="big-val" style="font-size:1.4rem;color:#4a6080;">N/A</div>
        <div id="an-iv-bat" class="sub">N/A</div>
      </div>
      <div class="atile atile-loss">
        <div class="lbl">Est. Loss</div>
        <div id="an-p-loss" class="big-val" style="font-size:1.4rem;color:#ef4444;">N/A</div>
        <div class="sub">PMIC + wiring</div>
      </div>
    </div>
    <div class="analysis-bottom">
      <div style="background:var(--bg-card2);border:1px solid var(--border);border-radius:8px;padding:14px 16px;">
        <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;">
          <div class="lbl" style="margin-bottom:0;">Session Energy</div>
          <div id="session-dur" class="sub" style="color:#10b981;">00:00:00</div>
        </div>
        <div class="srow"><div class="skey"><div class="sdot" style="background:#f59e0b"></div>Harvested</div><div id="s-solar" class="sval" style="color:#f59e0b">0.000 Wh</div></div>
        <div class="srow"><div class="skey"><div class="sdot" style="background:#38bdf8"></div>To Load</div><div id="s-load" class="sval" style="color:#38bdf8">0.000 Wh</div></div>
        <div class="srow"><div class="skey"><div class="sdot" style="background:#10b981"></div>To Battery</div><div id="s-bat" class="sval" style="color:#10b981">0.000 Wh</div></div>
        <div class="srow"><div class="skey"><div class="sdot" style="background:#ef4444"></div>From Battery</div><div id="s-bat-out" class="sval" style="color:#ef4444">0.000 Wh</div></div>
        <div class="srow" style="border-bottom:none"><div class="skey"><div class="sdot" style="background:#ef4444"></div>System Loss</div><div id="s-loss" class="sval" style="color:#ef4444">0.000 Wh</div></div>
      </div>
      <div style="background:var(--bg-card2);border:1px solid var(--border);border-radius:8px;padding:14px 16px;display:flex;flex-direction:column;gap:8px;">
        <div class="lbl">Battery Estimate</div>
        <div class="brow"><div class="bkey">Charge Rate (C-rate)</div><div id="b-crate" class="bval" style="color:#4a6080">N/A</div></div>
        <div style="height:3px;background:var(--border);border-radius:2px;overflow:hidden;"><div id="crate-fill" style="height:100%;background:#10b981;width:0%;transition:width .6s ease;border-radius:2px;"></div></div>
        <div class="brow"><div class="bkey">Est. full charge</div><div id="b-full" class="bval" style="color:#10b981">N/A</div></div>
        <div class="brow"><div class="bkey">Est. runtime (batt only)</div><div id="b-run" class="bval" style="color:#f59e0b">N/A</div></div>
        <div class="brow"><div class="bkey">Battery capacity</div><div class="bval" style="color:var(--muted)" id="b-cap">2000 mAh</div></div>
        <div class="brow"><div class="bkey">Path efficiency</div><div id="b-avg-eff" class="bval" style="color:#4a6080">N/A</div></div>
      </div>
    </div>
  </div>

  <div class="cfg-grid">
    <div class="card">
      <div class="lbl">Wi-Fi Config</div>
      <div class="sub" style="margin-bottom:10px">Ganti SSID dan password dari dashboard. Password kosong akan mempertahankan password yang sudah tersimpan.</div>
      <input class="inp" id="wifi-ssid" placeholder="SSID Wi-Fi" type="text" style="margin-bottom:8px">
      <input class="inp" id="wifi-pass" placeholder="Password Wi-Fi (opsional)" type="password" style="margin-bottom:8px">
      <button class="btn-primary" id="save-wifi" style="width:100%">Save Wi-Fi & Reboot</button>
      <div class="sub" id="wifi-note" style="margin-top:8px">Current SSID: --</div>
    </div>

    <div class="card">
      <div class="lbl">Device Config</div>
      <input class="inp" id="cfg-device" placeholder="device_id" type="text">
      <input class="inp" id="cfg-line" placeholder="line_id" type="text" style="margin-top:8px">
      <button class="btn-primary" id="save-device" style="margin-top:10px;width:100%">Save Device Config</button>
    </div>

    <div class="card">
      <div class="lbl">Runtime Sampling</div>
      <div class="sub" style="margin-bottom:10px">Pisahkan sensor poll, history buffer, dan report/send interval. Mode default sekarang diarahkan ke sampling cepat INA3221.</div>
      <div class="sub" style="margin-bottom:6px">Sensor poll interval</div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="cfg-sensor-poll" type="number" value="1" min="1" max="60000" style="flex:1">
        <span class="sub">ms</span>
      </div>
      <div class="sub" style="margin-bottom:6px">Report / send interval</div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="cfg-interval" type="number" value="1000" min="200" max="60000" style="flex:1">
        <span class="sub">ms</span>
      </div>
      <div class="sub" style="margin-bottom:6px">History sample interval</div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="cfg-history-interval" type="number" value="100" min="10" max="60000" style="flex:1">
        <span class="sub">ms</span>
      </div>
      <div class="sub" style="margin-bottom:6px">Chart points shown</div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:10px">
        <input class="inp" id="cfg-chart-points" type="number" value="180" min="30" max="2000" style="flex:1">
        <span class="sub">pts</span>
      </div>
      <div class="sub" style="margin-bottom:6px">INA averaging</div>
      <select class="inp" id="cfg-ina-avg" style="margin-bottom:8px">
        <option value="1">1 sample</option>
        <option value="4">4 samples</option>
        <option value="16">16 samples</option>
        <option value="64">64 samples</option>
        <option value="128">128 samples</option>
        <option value="256">256 samples</option>
        <option value="512">512 samples</option>
        <option value="1024">1024 samples</option>
      </select>
      <div class="sub" style="margin-bottom:6px">INA bus conversion</div>
      <select class="inp" id="cfg-ina-bus-us" style="margin-bottom:8px">
        <option value="140">140 us</option>
        <option value="204">204 us</option>
        <option value="332">332 us</option>
        <option value="588">588 us</option>
        <option value="1100">1.1 ms</option>
        <option value="2116">2.116 ms</option>
        <option value="4156">4.156 ms</option>
        <option value="8244">8.244 ms</option>
      </select>
      <div class="sub" style="margin-bottom:6px">INA shunt conversion</div>
      <select class="inp" id="cfg-ina-shunt-us" style="margin-bottom:10px">
        <option value="140">140 us</option>
        <option value="204">204 us</option>
        <option value="332">332 us</option>
        <option value="588">588 us</option>
        <option value="1100">1.1 ms</option>
        <option value="2116">2.116 ms</option>
        <option value="4156">4.156 ms</option>
        <option value="8244">8.244 ms</option>
      </select>
      <button class="btn-primary" id="apply-interval" style="width:100%">Apply Sampling Runtime</button>
      <div class="sub" id="interval-note" style="margin-top:8px">--</div>
      <div class="sub" id="runtime-ina-note" style="margin-top:6px">--</div>
    </div>

    <div class="card">
      <div class="lbl">LED PWM Control</div>
      <div class="sub" style="margin-bottom:10px">LT3478 dim/control output from ESP32-S3 LEDC PWM.</div>
      <div class="sub" id="led-pwm-meta" style="margin-bottom:8px">GPIO18 | 12-bit | 100 Hz</div>
      <label style="display:flex;align-items:center;gap:10px;cursor:pointer;margin-bottom:10px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:#0a1220">
        <input type="checkbox" id="led-pwm-enable" style="accent-color:var(--blue);width:18px;height:18px;flex:0 0 18px">
        <span class="sub">Enable LED PWM output</span>
      </label>
      <label style="display:flex;align-items:center;gap:10px;cursor:pointer;margin-bottom:10px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:#0a1220">
        <input type="checkbox" id="led-pwm-flash-enable" style="accent-color:var(--blue);width:18px;height:18px;flex:0 0 18px">
        <span class="sub">Enable flashing pulse mode</span>
      </label>
      <div class="sub" id="led-pwm-duty-note" style="margin-bottom:6px">Brightness: 0%</div>
      <input id="led-pwm-duty" type="range" min="0" max="100" step="1" value="0" style="width:100%;margin-bottom:8px">
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="led-pwm-duty-num" type="number" min="0" max="100" step="1" value="0" style="flex:1">
        <span class="sub">%</span>
      </div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="led-pwm-freq" type="number" min="100" max="20000" step="100" value="100" style="flex:1">
        <span class="sub">Hz</span>
      </div>
      <div class="sub" id="led-pwm-flash-note" style="margin-bottom:6px">Flashing off | preset 100 ms ON every 500 ms</div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="led-pwm-flash-on" type="number" min="1" max="60000" step="10" value="100" style="flex:1">
        <span class="sub">ms ON</span>
      </div>
      <div style="display:flex;gap:6px;align-items:center;margin-bottom:8px">
        <input class="inp" id="led-pwm-flash-period" type="number" min="1" max="60000" step="10" value="500" style="flex:1">
        <span class="sub">ms period</span>
      </div>
      <label style="display:flex;align-items:center;gap:10px;cursor:pointer;margin-bottom:10px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:#0a1220">
        <input type="checkbox" id="led-pwm-invert" style="accent-color:var(--blue);width:18px;height:18px;flex:0 0 18px">
        <span class="sub">Invert PWM signal</span>
      </label>
      <button class="btn-primary" id="apply-led-pwm" style="width:100%">Save LED PWM Now</button>
      <div class="sub" id="led-pwm-edit-note" style="margin-top:8px;color:var(--muted)">Live control active. Auto-save after you stop sliding.</div>
      <div class="sub" id="led-pwm-status" style="margin-top:6px">Output idle</div>
    </div>

    <div class="card">
      <div class="lbl">Backend Sender</div>
      <label style="display:flex;align-items:center;gap:10px;cursor:pointer;margin-bottom:10px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:#0a1220">
        <input type="checkbox" id="srv-enable" style="accent-color:var(--blue);width:18px;height:18px;flex:0 0 18px">
        <span class="sub">Enable local backend sender</span>
      </label>
      <input class="inp" id="srv-base" placeholder="http://192.168.1.100/api/v1" type="text" style="margin-bottom:8px">
      <input class="inp" id="srv-key" placeholder="API key (leave blank to keep current)" type="password" style="margin-bottom:8px">
      <label style="display:flex;align-items:center;gap:10px;cursor:pointer;margin-bottom:10px;padding:8px 10px;border:1px solid var(--border);border-radius:12px;background:#0a1220">
        <input type="checkbox" id="srv-key-clear" style="accent-color:var(--blue);width:18px;height:18px;flex:0 0 18px">
        <span class="sub">Clear stored API key</span>
      </label>
      <button class="btn-primary" id="save-server" style="width:100%">Save Backend Sender</button>
      <div class="sub" id="srv-edit-note" style="margin-top:8px;color:var(--muted)">Changes save only after pressing the button.</div>
      <div class="sub" id="srv-key-note" style="margin-top:8px">API key empty</div>
      <div class="sub" id="srv-status" style="margin-top:8px">Sender disabled</div>
      <div class="sub" id="srv-queue" style="margin-top:4px">Queue: 0 / 32</div>
    </div>

    <div class="card">
      <div class="lbl">OTA Control</div>
      <label style="display:flex;align-items:center;gap:8px;cursor:pointer;margin-bottom:10px">
        <input type="checkbox" id="ota-en" style="accent-color:var(--blue)">
        <span class="sub">Enable Arduino OTA</span>
      </label>
      <button class="btn-primary" id="apply-ota" style="width:100%">Apply OTA Setting</button>
      <div class="sub" id="ota-info" style="margin-top:8px">--</div>
    </div>

    <div class="card">
      <div class="lbl">I2C Scan</div>
      <div class="sub" style="margin-bottom:10px">Scan active devices on I2C bus.</div>
      <div class="sub" id="i2c-map-note" style="margin-bottom:8px;color:var(--muted)">Map: INA3221 @ 0x40 | CH1 = Solar | CH2 = Battery | CH3 = Load</div>
      <button class="btn-secondary" id="scan-i2c" style="width:100%">Scan I2C Bus</button>
      <div id="i2c-result" class="sub" style="margin-top:10px">--</div>
    </div>
  </div>

  <div class="card">
    <div class="lbl">Maintenance</div>
    <div class="sub" style="margin-bottom:10px">Use reboot after changing network or device identity.</div>
    <button class="btn-danger" id="reboot-btn">Reboot Device</button>
  </div>

  <script>
    let maxPts=300,chartReady=false,lastUpdate=Date.now(),pollMs=1000,pollTimer=null,rawPollMs=250,rawPollTimer=null,rawFetchBusy=false,archiveFetchBusy=false,archiveLastFetchMs=0,rawCursorSeq=0,batteryPresets=[],serverFormDirty=false,serverFormSaving=false,flowFormDirty=false,flowFormSaving=false,ledFormDirty=false,ledFormSaving=false,ledLiveTimer=null,ledSaveTimer=null,historyScale='1s',historyCapacityMax=600,runtimeReportMs=1000,runtimeSensorPollMs=1,runtimeRawEspCapacity=2048,runtimeRawFetchLimit=256,runtimeArchiveEspCapacity=600,runtimeArchiveIntervalMs=10000,runtimeMeasuredHz=0,runtimeInaAvg=1,viewFollowLive=true,viewStartMs=0,viewEndMs=0,yAxisLocked=false;
    const charts={},trendHistory=[],archiveHistory=[],rawHistory=[],SOLAR='#f59e0b',BAT='#10b981',BAT_DIS='#ef4444',COMBO='#a855f7',MUTED='#4a6080',WARN='#ef4444',LOAD='#38bdf8',TREND_HISTORY_LIMIT=3600,RAW_HISTORY_KEEP_MS=600000,HISTORY_SCALE_KEY='solarMonitorHistoryScaleV1',HISTORY_SCALES={s1:{id:'1s',label:'1s',gridMs:1000,windowMs:10000,source:'raw'},s3:{id:'3s',label:'3s',gridMs:3000,windowMs:30000,source:'raw'},s10:{id:'10s',label:'10s',gridMs:10000,windowMs:100000,source:'raw'},m1:{id:'1min',label:'1min',gridMs:60000,windowMs:600000,source:'raw'},m10:{id:'10min',label:'10min',gridMs:600000,windowMs:6000000,source:'archive'}},yAxisRanges={power:null,voltage:null,current:null},lastRendered={points:[],viewStartMs:0,viewEndMs:0,sourceMode:'raw-live',scaleId:'1s'},zoomSelection={active:false,chartId:'',startPx:0,endPx:0};
    function pad2(v){v=Math.max(0,Math.floor(Number(v)||0));return String(v).padStart(2,'0')}
    function pad3(v){v=Math.max(0,Math.floor(Number(v)||0));return String(v).padStart(3,'0')}
    function formatClockMs(ms){const d=new Date(Number(ms)||Date.now());return pad2(d.getHours())+':'+pad2(d.getMinutes())+':'+pad2(d.getSeconds())+'.'+pad3(d.getMilliseconds())}
    function formatHistoryTick(value){const seconds=Math.abs(Number(value)||0);if(seconds<0.001)return'now';if(seconds>=60){const minutes=seconds/60;return'-'+(Math.abs(minutes-Math.round(minutes))<0.01?Math.round(minutes):minutes.toFixed(1))+'m'}return'-'+(Math.abs(seconds-Math.round(seconds))<0.01?Math.round(seconds):seconds.toFixed(1))+'s'}
    function createChartOptions(){return{animation:false,responsive:true,maintainAspectRatio:false,interaction:{mode:'nearest',intersect:false},plugins:{legend:{labels:{color:'#4a6080',font:{family:'Courier New',size:10}}},tooltip:{callbacks:{title:items=>{const raw=items&&items.length?items[0].raw:null;return raw&&Number.isFinite(Number(raw.t))?formatClockMs(raw.t):'Time n/a'},footer:items=>{const raw=items&&items.length?items[0].raw:null;if(!raw)return'';const seq=Number.isFinite(Number(raw.seq))?('Seq '+Math.round(Number(raw.seq))):'';const rel=Number.isFinite(Number(raw.x))?(Math.abs(Number(raw.x))<0.0005?'now':((Number(raw.x)<0?'-':'+')+Math.abs(Number(raw.x)).toFixed(3)+' s')):'';return seq&&rel?(seq+' | '+rel):(seq||rel)}}}},scales:{x:{type:'linear',display:true,min:-10,max:0,grid:{display:true,color:'#1e2d42'},ticks:{color:'#4a6080',font:{family:'Courier New',size:10},stepSize:1,maxTicksLimit:11,maxRotation:0,minRotation:0,callback:value=>formatHistoryTick(value)}},y:{grid:{color:'#1e2d42'},ticks:{color:'#4a6080',font:{family:'Courier New',size:10}}}}}}
    function mkDs(label,color,bg){return{label:label,data:[],parsing:false,borderColor:color,backgroundColor:bg,borderWidth:1.5,fill:true,tension:0,pointRadius:0,pointHoverRadius:3,pointHitRadius:8}}
    function mkCfg(a,b,c,ca,cb,cc,bga,bgb,bgc){return{type:'line',data:{datasets:[mkDs(a,ca,bga),mkDs(b,cb,bgb),mkDs(c,cc,bgc)]},options:createChartOptions()}}
    function fmt(v,d){return Number.isFinite(v)?Number(v).toFixed(d==null?2:d):'--'}
    function fmtSigned(v,d){if(!Number.isFinite(Number(v)))return'--';v=Number(v);if(Math.abs(v)<.05)v=0;return(v>0?'+':'')+v.toFixed(d==null?1:d)}
    function formatHistorySpan(ms){ms=Math.max(0,Number(ms)||0);if(ms>=60000)return(ms/60000).toFixed(ms>=600000?1:2)+' min';if(ms>=1000)return(ms/1000).toFixed(ms>=10000?0:1)+' s';return Math.round(ms)+' ms'}
    function activeHistoryScale(){if(historyScale==='3s')return HISTORY_SCALES.s3;if(historyScale==='10s')return HISTORY_SCALES.s10;if(historyScale==='1min')return HISTORY_SCALES.m1;if(historyScale==='10min')return HISTORY_SCALES.m10;return HISTORY_SCALES.s1}
    function rawBrowserHistorySpanMs(){return rawHistory.length>1?Math.max(0,(Number(rawHistory[rawHistory.length-1].t)||0)-(Number(rawHistory[0].t)||0)):0}
    function sourceUsesRaw(mode){return mode==='raw-live'||mode==='raw-cache'}
    function visibleWindowPointEstimate(scale,mode){if(sourceUsesRaw(mode)||(!mode&&scale.source==='raw')){const hz=runtimeMeasuredHz>0?runtimeMeasuredHz:(1000/Math.max(1,runtimeSensorPollMs));return Math.max(1,Math.round(scale.windowMs*hz/1000))}const archiveIntervalMs=(mode==='archive-cache'||(!mode&&scale.source==='archive'))?runtimeArchiveIntervalMs:runtimeReportMs;return Math.max(1,Math.round(scale.windowMs/Math.max(1,archiveIntervalMs)))}
    function saveHistoryScale(){try{localStorage.setItem(HISTORY_SCALE_KEY,historyScale)}catch(e){}}
    function loadHistoryScale(){try{const stored=localStorage.getItem(HISTORY_SCALE_KEY);if(stored==='1s'||stored==='3s'||stored==='10s'||stored==='1min'||stored==='10min')historyScale=stored}catch(e){}}
    function lowerBoundByTime(points,targetMs){let lo=0,hi=points.length;while(lo<hi){const mid=(lo+hi)>>1;if((Number(points[mid].t)||0)<targetMs)lo=mid+1;else hi=mid}return lo}
    function pruneTimedHistory(list,minTimestampMs){const cut=lowerBoundByTime(list,minTimestampMs);if(cut>0)list.splice(0,cut)}
    function clearBrowserHistory(){trendHistory.length=0;archiveHistory.length=0;rawHistory.length=0;rawCursorSeq=0;archiveLastFetchMs=0;viewFollowLive=true;viewStartMs=0;viewEndMs=0;renderHistoryFromBrowser('Browser RAM cleared. Waiting for new live samples...')}
    function updateLiveLogsButton(){const button=document.getElementById('history-live-logs');if(!button)return;button.classList.toggle('active',viewFollowLive);button.textContent=viewFollowLive?'Live Logs ON':'Live Logs'}
    function activateLiveLogs(note){viewFollowLive=true;viewStartMs=0;viewEndMs=0;renderHistoryFromBrowser(note||'Live logs mode enabled.')}
    function resetHistoryZoom(note){activateLiveLogs(note||'Zoom reset to live logs.')}
    function chartByCanvasId(id){if(id==='powerChart')return charts.power;if(id==='voltageChart')return charts.voltage;if(id==='currentChart')return charts.current;return null}
    function canvasEventPoint(ev,chart){const canvas=chart&&chart.canvas?chart.canvas:ev.currentTarget,rect=canvas.getBoundingClientRect(),scaleX=canvas.width/Math.max(1,rect.width),scaleY=canvas.height/Math.max(1,rect.height);return{x:(ev.clientX-rect.left)*scaleX,y:(ev.clientY-rect.top)*scaleY}}
    function canvasPixelX(ev){const chart=chartByCanvasId(ev.currentTarget.id),point=canvasEventPoint(ev,chart);if(!chart||!chart.chartArea)return Math.max(0,Math.min(ev.currentTarget.width,point.x));return Math.max(chart.chartArea.left,Math.min(chart.chartArea.right,point.x))}
    function chartEventInPlot(ev,chart){if(!chart||!chart.chartArea)return false;const point=canvasEventPoint(ev,chart);return point.x>=chart.chartArea.left&&point.x<=chart.chartArea.right&&point.y>=chart.chartArea.top&&point.y<=chart.chartArea.bottom}
    function registerChartPlugins(){if(window.__solarChartPluginsRegistered||typeof Chart==='undefined')return;window.__solarChartPluginsRegistered=true;Chart.register({id:'solarDragZoomOverlay',afterDraw(chart){if(!zoomSelection.active||zoomSelection.chartId!==chart.canvas.id)return;const left=Math.max(chart.chartArea.left,Math.min(zoomSelection.startPx,zoomSelection.endPx)),right=Math.min(chart.chartArea.right,Math.max(zoomSelection.startPx,zoomSelection.endPx));if(right-left<2)return;const ctx=chart.ctx;ctx.save();ctx.fillStyle='rgba(56,189,248,0.16)';ctx.strokeStyle='rgba(56,189,248,0.95)';ctx.lineWidth=1;ctx.fillRect(left,chart.chartArea.top,right-left,chart.chartArea.bottom-chart.chartArea.top);ctx.strokeRect(left,chart.chartArea.top,right-left,chart.chartArea.bottom-chart.chartArea.top);ctx.restore()}})}
    function beginChartSelection(ev){const chart=chartByCanvasId(ev.currentTarget.id);if(!chartEventInPlot(ev,chart))return;zoomSelection.active=true;zoomSelection.chartId=ev.currentTarget.id;zoomSelection.startPx=canvasPixelX(ev);zoomSelection.endPx=zoomSelection.startPx;if(chart)chart.draw()}
    function moveChartSelection(ev){if(!zoomSelection.active||zoomSelection.chartId!==ev.currentTarget.id)return;zoomSelection.endPx=canvasPixelX(ev);const chart=chartByCanvasId(zoomSelection.chartId);if(chart)chart.draw()}
    function endChartSelection(){if(!zoomSelection.active)return;const chartId=zoomSelection.chartId,chart=chartByCanvasId(chartId),left=Math.min(zoomSelection.startPx,zoomSelection.endPx),right=Math.max(zoomSelection.startPx,zoomSelection.endPx);zoomSelection.active=false;zoomSelection.chartId='';if(chart)chart.draw();if(!chart||right-left<10||!lastRendered.viewEndMs)return;const xScale=chart.scales.x,relStart=xScale.getValueForPixel(left),relEnd=xScale.getValueForPixel(right),startMs=lastRendered.viewEndMs+Math.min(relStart,relEnd)*1000,endMs=lastRendered.viewEndMs+Math.max(relStart,relEnd)*1000;if(!Number.isFinite(startMs)||!Number.isFinite(endMs)||endMs-startMs<50)return;viewFollowLive=false;viewStartMs=startMs;viewEndMs=endMs;renderHistoryFromBrowser('Zoom '+formatHistorySpan(endMs-startMs)+' selected.')}
    function installChartInteractions(){['powerChart','voltageChart','currentChart'].forEach(id=>{const canvas=document.getElementById(id);if(!canvas||canvas.dataset.zoomReady==='1')return;canvas.dataset.zoomReady='1';canvas.addEventListener('mousedown',beginChartSelection);canvas.addEventListener('mousemove',moveChartSelection);canvas.addEventListener('dblclick',()=>activateLiveLogs('Live logs restored from chart double-click.'))});if(!window.__solarChartSelectionHooked){window.__solarChartSelectionHooked=true;window.addEventListener('mouseup',endChartSelection)}}
    function sourceModeLabel(mode){if(mode==='raw-live')return'live burst';if(mode==='raw-cache')return'browser raw cache';if(mode==='archive-cache')return'device archive 10s buckets';return'browser archive'}
    function rawSourceCovers(startMs,endMs){return rawHistory.length&&Number(rawHistory[0].t)<=startMs&&Number(rawHistory[rawHistory.length-1].t)>=endMs}
    function normalizeArchivePoint(item){return{t:Number(item&&item.timestamp_ms)||Date.now(),ps:Number(item&&item.p_solar_mw)||0,pb:Number(item&&item.p_bat_signed_mw!=null?item.p_bat_signed_mw:item&&item.p_bat_mw)||0,pl:Number(item&&item.p_load_mw)||0,vs:Number(item&&item.v_solar_v)||0,vb:Number(item&&item.v_bat_v)||0,vl:Number(item&&item.v_load_v)||0,is:Number(item&&item.i_solar_ma)||0,ib:Number(item&&item.i_bat_ma)||0,il:Number(item&&item.i_load_ma)||0}}
    function chooseHistorySource(scale,startMs,endMs){if(scale.source==='archive'){if(startMs>0&&endMs>startMs&&rawSourceCovers(startMs,endMs))return{points:rawHistory,mode:viewFollowLive?'raw-live':'raw-cache'};if(archiveHistory.length)return{points:archiveHistory,mode:'archive-cache'};if(trendHistory.length)return{points:trendHistory,mode:'trend-archive'};if(rawHistory.length)return{points:rawHistory,mode:'raw-cache'};return{points:[],mode:'archive-cache'}}if(startMs===0&&endMs===0)return rawHistory.length?{points:rawHistory,mode:'raw-live'}:(trendHistory.length?{points:trendHistory,mode:'trend-archive'}:(archiveHistory.length?{points:archiveHistory,mode:'archive-cache'}:{points:[],mode:'raw-live'}));if(rawSourceCovers(startMs,endMs))return{points:rawHistory,mode:viewFollowLive?'raw-live':'raw-cache'};if(trendHistory.length)return{points:trendHistory,mode:'trend-archive'};if(archiveHistory.length)return{points:archiveHistory,mode:'archive-cache'};return rawHistory.length?{points:rawHistory,mode:'raw-cache'}:{points:[],mode:'raw-live'}}
    function effectiveViewRange(scale,source){if(!source.length){const now=Date.now();return{startMs:now-scale.windowMs,endMs:now}}const sourceStart=Number(source[0].t)||Date.now(),sourceEnd=Number(source[source.length-1].t)||sourceStart;if(viewFollowLive){let endMs=sourceEnd,startMs=endMs-scale.windowMs;if(startMs<sourceStart)startMs=sourceStart;return{startMs:startMs,endMs:endMs}}let startMs=viewStartMs,endMs=viewEndMs;if(!(endMs>startMs)){endMs=Math.max(sourceEnd,sourceStart+scale.windowMs);startMs=endMs-scale.windowMs}const spanMs=Math.max(1,endMs-startMs);if(startMs<sourceStart){startMs=sourceStart;endMs=startMs+spanMs}if(endMs>sourceEnd){endMs=sourceEnd;startMs=endMs-spanMs}if(startMs<sourceStart)startMs=sourceStart;if(endMs<=startMs)endMs=Math.max(sourceEnd,startMs+1);return{startMs:startMs,endMs:endMs}}
    function calcYRange(points,keys){let min=Infinity,max=-Infinity;points.forEach(point=>keys.forEach(key=>{const value=Number(point[key]);if(Number.isFinite(value)){if(value<min)min=value;if(value>max)max=value}}));if(!Number.isFinite(min)||!Number.isFinite(max))return null;if(Math.abs(max-min)<0.0001){const pad=Math.max(1,Math.abs(max)*0.1);min-=pad;max+=pad}else{const pad=(max-min)*0.08;min-=pad;max+=pad}if(min>0&&max>0)min=Math.max(0,min);return{min:min,max:max}}
    function captureYAxisLocks(){if(!lastRendered.points.length)return;yAxisRanges.power=calcYRange(lastRendered.points,['ps','pb','pl']);yAxisRanges.voltage=calcYRange(lastRendered.points,['vs','vb','vl']);yAxisRanges.current=calcYRange(lastRendered.points,['is','ib','il'])}
    function updateYAxisLockButton(){const button=document.getElementById('history-lock-y');if(!button)return;button.classList.toggle('active',yAxisLocked);button.textContent=yAxisLocked?'Unlock Y-Axis':'Lock Y-Axis'}
    function toggleYAxisLock(){if(!yAxisLocked){captureYAxisLocks();yAxisLocked=!!(yAxisRanges.power||yAxisRanges.voltage||yAxisRanges.current)}else{yAxisLocked=false;yAxisRanges.power=null;yAxisRanges.voltage=null;yAxisRanges.current=null}updateYAxisLockButton();renderHistoryFromBrowser(yAxisLocked?'Y-axis locked from current view.':'Y-axis unlocked.')}
    function appendTrendHistorySample(d){const sample={t:Number(d.timestamp_ms)||Date.now(),ps:Number(d.solar&&d.solar.power_mw)||0,pb:Number(d.battery_power_signed_mw)||0,pl:Number(d.load&&d.load.power_mw)||0,vs:Number(d.solar&&d.solar.voltage)||0,vb:Number(d.battery&&d.battery.voltage)||0,vl:Number(d.load&&d.load.voltage)||0,is:Number(d.solar&&d.solar.current_ma)||0,ib:Number(d.battery&&d.battery.current_ma)||0,il:Number(d.load&&d.load.current_ma)||0};const last=trendHistory.length?trendHistory[trendHistory.length-1]:null;if(last&&Math.abs(sample.t-last.t)<Math.max(250,runtimeReportMs/2))return;trendHistory.push(sample);while(trendHistory.length>TREND_HISTORY_LIMIT)trendHistory.shift()}
    function appendRawHistoryBatch(points){let appended=0;const currentScale=activeHistoryScale();points.forEach(item=>{if(!Array.isArray(item)||item.length<8)return;const seq=Number(item[0])||0;if(seq<=rawCursorSeq)return;const t=Number(item[1])||Date.now(),vs=Number(item[2])||0,vb=Number(item[3])||0,vl=Number(item[4])||0,is=Number(item[5])||0,ib=Number(item[6])||0,il=Number(item[7])||0;rawHistory.push({seq:seq,t:t,vs:vs,vb:vb,vl:vl,is:is,ib:ib,il:il,ps:vs*is,pb:vb*ib,pl:vl*il});rawCursorSeq=seq;appended++});if(rawHistory.length){const latestTimestampMs=Number(rawHistory[rawHistory.length-1].t)||Date.now(),minimumWindowMs=Math.max(RAW_HISTORY_KEEP_MS,currentScale.source==='raw'?currentScale.windowMs:0);pruneTimedHistory(rawHistory,latestTimestampMs-minimumWindowMs)}return appended}
    function downsampleBrowserSamples(points,limit){if(points.length<=limit||limit<3)return points.slice();const reduced=[points[0]];const stride=(points.length-2)/(limit-2);for(let index=1;index<limit-1;index++){reduced.push(points[Math.min(points.length-2,Math.round(index*stride))])}reduced.push(points[points.length-1]);return reduced}
    function rawDisplayPointLimit(scale,range){const spanMs=range&&range.endMs>range.startMs?(range.endMs-range.startMs):scale.windowMs;if(scale.id==='1min')return Math.max(4800,maxPts*16);if(spanMs>=120000)return Math.max(2400,maxPts*10);return Math.max(1200,maxPts*6)}
    function pushUniqueRawDisplayPoint(target,point){if(!point)return;const last=target.length?target[target.length-1]:null;if(last&&Number(last.seq)===Number(point.seq)&&Number(last.t)===Number(point.t))return;target.push(point)}
    function compactStableRawSamples(points,range,limit){if(points.length<=limit||limit<4)return points.slice();const spanMs=Math.max(1,range&&range.endMs>range.startMs?(range.endMs-range.startMs):((Number(points[points.length-1].t)||Date.now())-(Number(points[0].t)||Date.now()))),bucketBudget=Math.max(1,Math.floor(limit/3)),bucketMs=Math.max(1,Math.ceil(spanMs/bucketBudget)),reduced=[];let bucketStart=0;while(bucketStart<points.length){const bucketId=Math.floor((Number(points[bucketStart].t)||0)/bucketMs);let bucketEnd=bucketStart+1;while(bucketEnd<points.length&&Math.floor((Number(points[bucketEnd].t)||0)/bucketMs)===bucketId)bucketEnd++;const bucketCount=bucketEnd-bucketStart;pushUniqueRawDisplayPoint(reduced,points[bucketStart]);if(bucketCount>2)pushUniqueRawDisplayPoint(reduced,points[bucketStart+Math.floor(bucketCount/2)]);if(bucketCount>1)pushUniqueRawDisplayPoint(reduced,points[bucketEnd-1]);bucketStart=bucketEnd}return reduced.length>limit?downsampleBrowserSamples(reduced,limit):reduced}
    function displaySamplesForScale(points,scale,sourceMode,range){if(sourceUsesRaw(sourceMode)){const rawLimit=rawDisplayPointLimit(scale,range);return points.length>rawLimit?compactStableRawSamples(points,range,rawLimit):points.slice()}if(scale.id==='1min'||scale.id==='10min')return points.slice();return points.length>maxPts?downsampleBrowserSamples(points,maxPts):points.slice()}
    function renderHistoryFromBrowser(noteOverride){const scale=activeHistoryScale(),preferredSource=chooseHistorySource(scale,viewFollowLive?0:viewStartMs,viewFollowLive?0:viewEndMs),baseSource=preferredSource.points;if(!baseSource.length){syncCharts([],Date.now(),scale);renderHistoryModeSummary(noteOverride||(scale.source==='raw'?'Waiting for raw live stream...':'Waiting for browser archive history...'));return}const viewRange=effectiveViewRange(scale,baseSource),resolvedSource=chooseHistorySource(scale,viewRange.startMs,viewRange.endMs),source=resolvedSource.points;if(!source.length){syncCharts([],Date.now(),scale);renderHistoryModeSummary(noteOverride||'Waiting for browser history...');return}const clampedRange=effectiveViewRange(scale,source),startIndex=lowerBoundByTime(source,clampedRange.startMs),endIndex=lowerBoundByTime(source,clampedRange.endMs+1),visible=(startIndex<endIndex?source.slice(startIndex,endIndex):source.slice(Math.max(0,source.length-1))),displayPoints=displaySamplesForScale(visible,scale,resolvedSource.mode,clampedRange),viewEndMsLocal=clampedRange.endMs||Number(source[source.length-1].t)||Date.now(),visibleCount=visible.length,rawCompactNote=sourceUsesRaw(resolvedSource.mode)&&visibleCount!==displayPoints.length?(' -> '+displayPoints.length+' chart pts | stable raw pick | no avg'):visibleCount!==displayPoints.length?(' -> '+displayPoints.length+' chart pts'):'';lastRendered.points=displayPoints.slice();lastRendered.viewStartMs=clampedRange.startMs;lastRendered.viewEndMs=viewEndMsLocal;lastRendered.sourceMode=resolvedSource.mode;lastRendered.scaleId=scale.id;syncCharts(displayPoints,viewEndMsLocal,scale);const note=noteOverride||('Mode '+(viewFollowLive?'Live logs':'Zoom inspect')+' | Source '+sourceModeLabel(resolvedSource.mode)+' | '+visibleCount+' pts'+rawCompactNote+(sourceUsesRaw(resolvedSource.mode)?(' | avg x'+runtimeInaAvg+' | '+(runtimeMeasuredHz>0?runtimeMeasuredHz.toFixed(1):'0.0')+' Hz'):' | browser archive view'));renderHistoryModeSummary(note)}
    function renderHistoryModeSummary(noteOverride){const scale=activeHistoryScale(),rawSpanMs=rawBrowserHistorySpanMs(),modeLabel=viewFollowLive?'Live logs':'Zoom inspect',rangeLabel=!viewFollowLive&&viewEndMs>viewStartMs?(' | Selection '+formatHistorySpan(viewEndMs-viewStartMs)):' | Live tail moving';document.querySelectorAll('[data-history-scale]').forEach(btn=>btn.classList.toggle('active',btn.dataset.historyScale===historyScale));updateLiveLogsButton();document.getElementById('history-mode-note').textContent='Mode '+modeLabel+' | Grid '+scale.label+' | Window '+formatHistorySpan(scale.windowMs)+' | ~'+visibleWindowPointEstimate(scale,lastRendered.sourceMode)+' pts'+rangeLabel;document.getElementById('history-source-note').textContent=noteOverride||('Source '+sourceModeLabel(lastRendered.sourceMode)+' | Raw RAM '+formatHistorySpan(rawSpanMs)+' | Archive '+formatHistorySpan(runtimeArchiveEspCapacity*runtimeArchiveIntervalMs)+' | Status '+runtimeReportMs+' ms | Fetch '+rawPollMs+' ms | avg x'+runtimeInaAvg+' | drag on chart to zoom | double-click or Live Logs to follow newest data')}
    function setHistoryScale(view){historyScale=(view==='3s'||view==='10s'||view==='1min'||view==='10min')?view:'1s';if(!viewFollowLive&&viewEndMs>viewStartMs){const scale=activeHistoryScale(),centerMs=(viewStartMs+viewEndMs)/2;viewStartMs=centerMs-(scale.windowMs/2);viewEndMs=centerMs+(scale.windowMs/2)}saveHistoryScale();if(activeHistoryScale().source==='archive')fetchArchiveHistory(true);renderHistoryFromBrowser()}
    function flash(id,txt){const el=document.getElementById(id);if(!el)return;el.textContent=txt;el.classList.remove('flashed');void el.offsetWidth;el.classList.add('flashed');setTimeout(()=>el.classList.remove('flashed'),400)}
    function updateArc(arcId,arcTxtId,value,min,max,stroke,labelText){const total=172.8,pct=Math.min(1,Math.max(0,(value-min)/(max-min))),offset=total*(1-pct),arc=document.getElementById(arcId);arc.style.strokeDashoffset=offset;if(stroke)arc.setAttribute('stroke',stroke);document.getElementById(arcTxtId).textContent=labelText!=null?labelText:(Number(value)||0).toFixed(1)+'V'}
    function setPoll(ms){pollMs=Math.max(200,Number(ms)||1000)}
    function setRawPollMs(ms){rawPollMs=Math.max(150,Math.min(1000,Math.round(Number(ms)||250)))}
    function schedulePoll(){if(pollTimer)clearTimeout(pollTimer);const delay=document.hidden?Math.max(3000,pollMs):pollMs;pollTimer=setTimeout(fetchData,delay)}
    function scheduleRawPoll(){if(rawPollTimer)clearTimeout(rawPollTimer);const delay=document.hidden?Math.max(1000,rawPollMs):rawPollMs;rawPollTimer=setTimeout(fetchRawHistory,delay)}
    function setNode(boxId,textId,stroke,fill){const box=document.getElementById(boxId);box.setAttribute('stroke',stroke);if(fill!==undefined)box.setAttribute('fill',fill);document.getElementById(textId).setAttribute('fill',stroke)}
    function setSvgClass(id,name){document.getElementById(id).setAttribute('class',name)}
    function setSolarVisual(active,ok){const stroke=!ok?WARN:(active?SOLAR:MUTED);const fill=!ok?'#2a0f12':(active?'#1a1f0a':'#101622');document.getElementById('c-solar').className='card'+(!ok?' card-solar-warn':active?' card-solar-on':'');document.getElementById('arc-solar').setAttribute('stroke',stroke);setNode('node-solar-box','node-solar-text',stroke,fill)}
    function setBatteryVisual(dir,ok){let stroke=MUTED,fill='#101622';const card=document.getElementById('c-bat');card.className='card';if(!ok){card.className+=' card-bat-warn';stroke=WARN;fill='#2a0f12'}else if(dir==='charging'){card.className+=' card-bat-charge';stroke=BAT;fill='#0a1a12'}else if(dir==='discharging'){card.className+=' card-bat-discharge';stroke=BAT_DIS;fill='#220f14'}document.getElementById('arc-bat').setAttribute('stroke',stroke);setNode('node-bat-box','node-bat-text',stroke,fill);document.getElementById('lbl-bat-w').setAttribute('fill',stroke)}
    function setLoadCardVisual(active,ok){const card=document.getElementById('c-load');let stroke=MUTED;if(!ok){card.className='card card-load-warn';stroke=WARN}else if(active){card.className='card card-load-on';stroke=LOAD}else{card.className='card';stroke=MUTED}document.getElementById('arc-load').setAttribute('stroke',stroke)}
    function setLoadVisual(active,error){const stroke=error?WARN:(active?LOAD:MUTED);const fill=error?'#2a0f12':(active?'#0d2236':'#0a1a1a');setNode('node-load-box','node-load-text',stroke,fill);document.getElementById('lbl-load-w').setAttribute('fill',stroke)}
    function resetLine(id,stroke,end){const el=document.getElementById(id);setSvgClass(id,'flow-off');el.setAttribute('stroke',stroke);if(end){el.setAttribute('marker-end',end)}else{el.removeAttribute('marker-end')}}
    function updateFlow(mode,solarActive,batDir,loadActive,loadOk,pSolar,pLoad,pBatSigned){resetLine('line-solar',SOLAR,'url(#arr-solar)');resetLine('line-load',LOAD,'url(#arr-load)');resetLine('line-bat-charge',BAT,'url(#arr-bat-in)');resetLine('line-bat-discharge',BAT_DIS,'url(#arr-bat-out)');if(mode==='error'){['line-solar','line-bat-charge','line-bat-discharge'].forEach(id=>{const el=document.getElementById(id);setSvgClass(id,'flow-err');el.setAttribute('stroke',WARN);el.setAttribute('marker-end','url(#arr-error)')});if(loadOk&&loadActive){setSvgClass('line-load','flow-load');setLoadVisual(true,false)}else{setLoadVisual(false,!loadOk)}}else{if(solarActive)setSvgClass('line-solar','flow-on');if(loadOk&&loadActive)setSvgClass('line-load','flow-load');if(batDir==='charging')setSvgClass('line-bat-charge','flow-down');if(batDir==='discharging')setSvgClass('line-bat-discharge','flow-up');setLoadVisual(loadOk&&loadActive,!loadOk)}document.getElementById('lbl-solar-w').textContent=(Number(pSolar)||0).toFixed(0)+'mW';document.getElementById('lbl-load-w').textContent=loadOk?(Number(pLoad)||0).toFixed(0)+'mW':'--mW';document.getElementById('lbl-bat-w').textContent=fmtSigned(pBatSigned,0)+'mW'}
    function flowCopy(mode,dir,solarActive){if(mode==='error')return['Error','No valid sensor data','var(--s-error)'];if(dir==='charging')return['Charging','PMIC to BATTERY','var(--bat)'];if(dir==='discharging'&&solarActive)return['Combo','SOLAR and BATTERY to LOAD','var(--s-combo)'];if(dir==='discharging')return['Discharging','BATTERY to PMIC','var(--s-error)'];if(solarActive)return['Solar Direct','SOLAR to LOAD','var(--solar)'];return['Idle','No active flow','var(--muted)']}
    function setBadge(state,label){const badge=document.getElementById('state-badge'),dot=document.getElementById('live-dot');const map={SOLAR_CHARGING:'badge-charging',SOLAR_DIRECT:'badge-solar',SOLAR_AND_BATTERY:'badge-combo',BATTERY_ONLY:'badge-battery',IDLE:'badge-idle',SENSOR_ERROR:'badge-error',OFFLINE:'badge-error'};badge.className='state-badge '+(map[state]||'badge-idle');badge.textContent=label||state||'IDLE';dot.className='live-dot'+((state==='SENSOR_ERROR'||state==='OFFLINE')?' error':'')}
    function formatHours(h){if(!Number.isFinite(Number(h))||Number(h)<=0)return'N/A';h=Number(h);const hh=Math.floor(h),mm=Math.round((h-hh)*60);if(hh===0)return mm+'m';return hh+'h'+(mm>0?' '+mm+'m':'')}
    function setText(id,text,color){const el=document.getElementById(id);if(!el)return;el.textContent=text;if(color!==undefined)el.style.color=color}
    function analysisModeText(mode){return mode==='solar_input'?'Solar Input Analysis':mode==='battery_input'?'Battery Supply Analysis':mode==='mixed_input'?'Mixed Supply Analysis':'Analysis Unavailable'}
    function renderBatteryPresets(list){batteryPresets=Array.isArray(list)?list.slice():[];const sel=document.getElementById('bat-profile-sel');if(!sel)return;sel.innerHTML='';batteryPresets.forEach(p=>{const opt=document.createElement('option');opt.value=p.id;opt.textContent=p.label+' ('+Number(p.full_voltage_v).toFixed(2)+' / '+Number(p.empty_voltage_v).toFixed(2)+'V)';sel.appendChild(opt)})}
    function applyBatteryPreset(id){const preset=batteryPresets.find(p=>p.id===id);if(!preset)return;document.getElementById('bat-full').value=Number(preset.full_voltage_v).toFixed(2);document.getElementById('bat-empty').value=Number(preset.empty_voltage_v).toFixed(2)}
    function batteryProfileText(label,fullV,emptyV){return'Profile: '+(label||'Custom')+' ('+fmt(fullV,2)+' / '+fmt(emptyV,2)+'V)'}
    function formatShunt(v){const value=Number(v);return Number.isFinite(value)?value.toFixed(2)+' mOhm':'-- mOhm'}
    function renderWifiConfig(data,syncInputs){const ssid=data.wifi_ssid||'';if(syncInputs&&document.activeElement!==document.getElementById('wifi-ssid'))document.getElementById('wifi-ssid').value=ssid;if(syncInputs)document.getElementById('wifi-pass').value='';document.getElementById('wifi-note').textContent='Current SSID: '+(ssid||'--')+' | Mode: '+(data.wifi_mode||'--')}
    function renderInaConfig(data,syncInputs){const solar=Number(data.solar_shunt_milliohms),battery=Number(data.battery_shunt_milliohms),load=Number(data.load_shunt_milliohms);document.getElementById('solar-shunt-line').textContent='Shunt config: '+formatShunt(solar);document.getElementById('bat-shunt-line').textContent='INA shunt: '+formatShunt(battery);document.getElementById('load-shunt-line').textContent='Shunt config: '+formatShunt(load);document.getElementById('i2c-map-note').textContent='Map: '+(data.i2c_map||('0x40=INA3221, CH1=Solar '+formatShunt(solar)+', CH2=Battery '+formatShunt(battery)+', CH3=Load '+formatShunt(load)));if(syncInputs){if(document.activeElement!==document.getElementById('solar-shunt-mo'))document.getElementById('solar-shunt-mo').value=Number.isFinite(solar)?solar.toFixed(2):'';if(document.activeElement!==document.getElementById('bat-shunt-mo'))document.getElementById('bat-shunt-mo').value=Number.isFinite(battery)?battery.toFixed(2):'';if(document.activeElement!==document.getElementById('load-shunt-mo'))document.getElementById('load-shunt-mo').value=Number.isFinite(load)?load.toFixed(2):'';}}
    function clampChartPoints(v){v=Number(v);if(!Number.isFinite(v))v=300;return Math.max(30,Math.min(historyCapacityMax,Math.round(v)))}
    function renderRuntimeConfig(data){const reportMs=Math.max(200,Number(data.sample_interval_ms)||1000),sensorPollMs=Math.max(1,Number(data.sensor_poll_interval_ms)||1),historyMs=Math.max(10,Number(data.history_interval_ms)||100),secondsCap=Math.max(30,Number(data.history_seconds_capacity||data.history_capacity)||600),archiveCap=Math.max(60,Number(data.history_minutes_capacity)||600),archiveIntervalMs=Math.max(1000,Number(data.history_minutes_interval_ms)||10000),rawChartPts=Number(data.chart_point_limit)||300,inaAvg=Math.max(1,Number(data.ina_averaging_samples)||1),inaBusUs=Math.max(140,Number(data.ina_bus_conv_us)||140),inaShuntUs=Math.max(140,Number(data.ina_shunt_conv_us)||140),theoryHz=Number(data.ina_estimated_channel_hz)||0,measuredHz=Number(data.sensor_measured_hz)||0,rawEspCapacity=Math.max(1,Number(data.raw_history_capacity)||2048),rawFetchLimit=Math.max(32,Number(data.raw_history_fetch_limit)||256),regHex=data.ina_config_register_hex||'0x7007';historyCapacityMax=Math.max(secondsCap,rawEspCapacity,archiveCap,2000);const chartPts=clampChartPoints(rawChartPts),largestWindowMs=Math.max(HISTORY_SCALES.s1.windowMs,HISTORY_SCALES.s3.windowMs,HISTORY_SCALES.s10.windowMs,HISTORY_SCALES.m1.windowMs,HISTORY_SCALES.m10.windowMs);if(document.activeElement!==document.getElementById('cfg-sensor-poll'))document.getElementById('cfg-sensor-poll').value=sensorPollMs;if(document.activeElement!==document.getElementById('cfg-interval'))document.getElementById('cfg-interval').value=reportMs;if(document.activeElement!==document.getElementById('cfg-history-interval'))document.getElementById('cfg-history-interval').value=historyMs;if(document.activeElement!==document.getElementById('cfg-chart-points'))document.getElementById('cfg-chart-points').value=chartPts;document.getElementById('cfg-ina-avg').value=String(inaAvg);document.getElementById('cfg-ina-bus-us').value=String(inaBusUs);document.getElementById('cfg-ina-shunt-us').value=String(inaShuntUs);runtimeReportMs=reportMs;runtimeSensorPollMs=sensorPollMs;runtimeRawEspCapacity=rawEspCapacity;runtimeRawFetchLimit=rawFetchLimit;runtimeArchiveEspCapacity=archiveCap;runtimeArchiveIntervalMs=archiveIntervalMs;runtimeMeasuredHz=measuredHz;runtimeInaAvg=inaAvg;maxPts=chartPts;setRawPollMs(Math.max(150,Math.min(1000,Math.round(reportMs/4))));document.getElementById('interval-note').textContent='Poll '+sensorPollMs+' ms | Status '+reportMs+' ms | Raw '+rawPollMs+' ms | ESP '+rawEspCapacity+' pts | Browser RAM '+formatHistorySpan(RAW_HISTORY_KEEP_MS)+' | Archive '+formatHistorySpan(archiveCap*archiveIntervalMs)+' | Trend chart '+chartPts+' pts';document.getElementById('runtime-ina-note').textContent='INA '+regHex+' | avg x'+inaAvg+' | bus '+inaBusUs+' us | shunt '+inaShuntUs+' us | theory '+(theoryHz?theoryHz.toFixed(1):'0.0')+' Hz | measured '+(measuredHz?measuredHz.toFixed(1):'0.0')+' Hz | drag to zoom | lock Y available';renderHistoryFromBrowser()}
    function markFlowFormDirty(){flowFormDirty=true;document.getElementById('save-flow').textContent='Save Flow Thresholds *';document.getElementById('flow-edit-note').textContent='Unsaved flow threshold changes pending.'}
    function clearFlowFormDirty(msg){flowFormDirty=false;flowFormSaving=false;document.getElementById('save-flow').textContent='Save Flow Thresholds';document.getElementById('flow-edit-note').textContent=msg||'Changes save only after pressing the button.'}
    function renderFlowThresholdConfig(data){const solar=Math.max(0,Number(data.solar_active_threshold_mw)||0),load=Math.max(0,Number(data.load_active_threshold_mw)||0),battery=Math.max(0,Number(data.battery_flow_threshold_mw)||0);if(!flowFormDirty&&!flowFormSaving){document.getElementById('flow-solar-threshold').value=solar;document.getElementById('flow-load-threshold').value=load;document.getElementById('flow-battery-threshold').value=battery}document.getElementById('flow-threshold-note').textContent='Active thresholds: solar >= '+solar+' mW | battery >= '+battery+' mW | load >= '+load+' mW'}
    function markServerFormDirty(){serverFormDirty=true;document.getElementById('save-server').textContent='Save Backend Sender *';document.getElementById('srv-edit-note').textContent='Unsaved sender changes pending.'}
    function clearServerFormDirty(msg){serverFormDirty=false;serverFormSaving=false;document.getElementById('save-server').textContent='Save Backend Sender';document.getElementById('srv-edit-note').textContent=msg||'Changes save only after pressing the button.'}
    function clampLedDuty(v){v=Number(v);if(!Number.isFinite(v))v=0;return Math.max(0,Math.min(100,Math.round(v)))}
    function clampLedFlashOn(v){v=Number(v);if(!Number.isFinite(v))v=100;return Math.max(1,Math.min(60000,Math.round(v)))}
    function clampLedFlashPeriod(v){v=Number(v);if(!Number.isFinite(v))v=500;return Math.max(1,Math.min(60000,Math.round(v)))}
    function syncLedDutyInputs(source){const duty=source==='slider'?clampLedDuty(document.getElementById('led-pwm-duty').value):clampLedDuty(document.getElementById('led-pwm-duty-num').value);document.getElementById('led-pwm-duty').value=duty;document.getElementById('led-pwm-duty-num').value=duty;document.getElementById('led-pwm-duty-note').textContent='Brightness: '+duty+'%'}
    function syncLedFlashInputs(){let onMs=clampLedFlashOn(document.getElementById('led-pwm-flash-on').value),periodMs=clampLedFlashPeriod(document.getElementById('led-pwm-flash-period').value);if(periodMs<onMs)periodMs=onMs;document.getElementById('led-pwm-flash-on').value=onMs;document.getElementById('led-pwm-flash-period').value=periodMs;return{onMs:onMs,periodMs:periodMs}}
    function markLedPwmFormDirty(msg){ledFormDirty=true;document.getElementById('apply-led-pwm').textContent='Save LED PWM Now';document.getElementById('led-pwm-edit-note').textContent=msg||'Live apply active. Applying and auto-saving...'}
    function clearLedPwmFormDirty(msg){ledFormDirty=false;ledFormSaving=false;document.getElementById('apply-led-pwm').textContent='Save LED PWM Now';document.getElementById('led-pwm-edit-note').textContent=msg||'Live control active. Auto-save after you stop sliding.'}
    function renderLedPwmConfig(data){const pin=Number(data.led_pwm_pin)||18,res=Number(data.led_pwm_resolution_bits)||12,freq=Math.max(100,Number(data.led_pwm_frequency_hz)||100),duty=clampLedDuty(data.led_pwm_duty_percent),enabled=!!data.led_pwm_enabled,inverted=!!data.led_pwm_inverted,flashEnabled=!!data.led_pwm_flash_enabled,flashOnMs=clampLedFlashOn(data.led_pwm_flash_on_ms),flashPeriodMs=Math.max(flashOnMs,clampLedFlashPeriod(data.led_pwm_flash_period_ms)),attached=('led_pwm_attached' in data)?!!data.led_pwm_attached:true,signalPct=Number(data.led_pwm_signal_duty_percent),brightnessActive=('led_pwm_brightness_active' in data)?!!data.led_pwm_brightness_active:enabled,flashOutputOn=('led_pwm_flash_output_on' in data)?!!data.led_pwm_flash_output_on:false,flashCycleMs=Math.max(0,Math.round(Number(data.led_pwm_flash_cycle_ms)||0));if(!ledFormDirty&&!ledFormSaving){document.getElementById('led-pwm-enable').checked=enabled;document.getElementById('led-pwm-flash-enable').checked=flashEnabled;document.getElementById('led-pwm-invert').checked=inverted;document.getElementById('led-pwm-freq').value=freq;document.getElementById('led-pwm-duty').value=duty;document.getElementById('led-pwm-duty-num').value=duty;document.getElementById('led-pwm-flash-on').value=flashOnMs;document.getElementById('led-pwm-flash-period').value=flashPeriodMs}document.getElementById('led-pwm-meta').textContent='GPIO'+pin+' | '+res+'-bit | '+freq+' Hz';document.getElementById('led-pwm-duty-note').textContent='Brightness: '+duty+'%';document.getElementById('led-pwm-flash-note').textContent=flashEnabled?('Flashing: '+flashOnMs+' ms ON every '+flashPeriodMs+' ms'):('Flashing off | preset '+flashOnMs+' ms ON every '+flashPeriodMs+' ms');document.getElementById('led-pwm-status').textContent=(enabled?(flashEnabled?'PWM flashing':'PWM active'):'PWM idle')+' | signal '+(Number.isFinite(signalPct)?signalPct.toFixed(1):'--')+'%'+(inverted?' | inverted':'')+(flashEnabled?(' | flash '+(flashOutputOn?'ON':'OFF')+' '+flashCycleMs+'/'+flashPeriodMs+' ms'):'')+(brightnessActive&&!flashEnabled?' | output on':'')+(attached?'':' | attach failed')}
    function renderSenderStatus(data){const enabled=('backend_sender_enabled' in data)?!!data.backend_sender_enabled:!!data.server_enabled,state=data.backend_sender_state|| (enabled?'idle':'disabled'),depth=Number(data.backend_queue_depth)||0,cap=Number(data.backend_queue_capacity)||32,dropped=Number(data.backend_dropped_samples)||0,retry=Number(data.backend_next_retry_in_ms)||0,http=data.backend_last_http_status?(' | HTTP '+data.backend_last_http_status):'',parts=[state.replace(/_/g,' ')];if(data.backend_last_error)parts.push(data.backend_last_error);if(retry>0)parts.push('retry in '+retry+' ms');if(!serverFormDirty&&!serverFormSaving){document.getElementById('srv-enable').checked=enabled;document.getElementById('srv-base').value=data.api_base||document.getElementById('srv-base').value||'';}document.getElementById('srv-key-note').textContent=data.api_key_configured?'API key stored on device':'API key empty';document.getElementById('srv-status').textContent=parts.join(' | ');document.getElementById('srv-queue').textContent='Queue: '+depth+' / '+cap+' | Dropped: '+dropped+http}
    function pointSeries(points,latestTimestampMs,key){return points.map(p=>({x:(Number(p.t)-latestTimestampMs)/1000,y:Number(p[key])||0,t:Number(p.t)||0,seq:Number(p.seq)||0}))}
    function applyYAxisRange(ch,axisKey){if(!ch)return;const range=yAxisLocked?yAxisRanges[axisKey]:null;if(range){ch.options.scales.y.min=range.min;ch.options.scales.y.max=range.max}else{delete ch.options.scales.y.min;delete ch.options.scales.y.max}}
    function syncOne(ch,points,latestTimestampMs,keyA,keyB,keyC,scale,axisKey){if(!ch)return;const pointRadius=scale.source==='raw'?(points.length>50000?0:points.length>15000?0.1:points.length>5000?0.2:points.length>1200?0.5:points.length>600?0.75:1):(points.length>600?0.5:1.1);ch.data.datasets[0].data=pointSeries(points,latestTimestampMs,keyA);ch.data.datasets[1].data=pointSeries(points,latestTimestampMs,keyB);ch.data.datasets[2].data=pointSeries(points,latestTimestampMs,keyC);ch.data.datasets.forEach(ds=>{ds.pointRadius=pointRadius;ds.pointHoverRadius=pointRadius>0?Math.max(3,pointRadius*4):3;ds.pointHitRadius=points.length>15000?4:8});ch.options.scales.x.min=(lastRendered.viewStartMs-latestTimestampMs)/1000;ch.options.scales.x.max=0;ch.options.scales.x.ticks.stepSize=scale.gridMs/1000;applyYAxisRange(ch,axisKey);ch.update('none')}
    function syncCharts(points,latestTimestampMs,scale){if(!chartReady)return;syncOne(charts.power,points,latestTimestampMs,'ps','pb','pl',scale,'power');syncOne(charts.voltage,points,latestTimestampMs,'vs','vb','vl',scale,'voltage');syncOne(charts.current,points,latestTimestampMs,'is','ib','il',scale,'current')}
    function waitForCharts(){const t=setInterval(()=>{if(typeof Chart!=='undefined'){clearInterval(t);registerChartPlugins();charts.power=new Chart(document.getElementById('powerChart').getContext('2d'),mkCfg('Solar Power (mW)','Battery Power (mW)','Load Power (mW)',SOLAR,BAT,LOAD,'rgba(245,158,11,0.08)','rgba(16,185,129,0.08)','rgba(56,189,248,0.06)'));charts.voltage=new Chart(document.getElementById('voltageChart').getContext('2d'),mkCfg('Solar Voltage (V)','Battery Voltage (V)','Load Voltage (V)',SOLAR,BAT,LOAD,'rgba(245,158,11,0.08)','rgba(16,185,129,0.08)','rgba(56,189,248,0.06)'));charts.current=new Chart(document.getElementById('currentChart').getContext('2d'),mkCfg('Solar Current (mA)','Battery Current (mA)','Load Current (mA)',SOLAR,BAT,LOAD,'rgba(245,158,11,0.08)','rgba(16,185,129,0.08)','rgba(56,189,248,0.06)'));chartReady=true;installChartInteractions();updateYAxisLockButton();updateLiveLogsButton();renderHistoryFromBrowser()}},150)}
    function updateFlowEfficiency(d){const a=d.analysis||{},mode=a.analysis_mode||'unavailable',balanceValid=!!a.balance_valid,pctLoad=Math.max(0,Number(a.pct_load)||0),pctBat=Math.max(0,Number(a.pct_bat)||0),pctLoss=Math.max(0,Number(a.pct_loss)||0),showBatterySink=mode==='solar_input'&&pctBat>.05,panel=document.querySelector('.flow-eff');const modeColor=mode==='solar_input'?SOLAR:mode==='battery_input'?BAT_DIS:mode==='mixed_input'?COMBO:MUTED;let effColor=MUTED;setText('flow-eff-mode',analysisModeText(mode),modeColor);if(balanceValid){const eff=Math.max(0,Math.min(100,Number(a.efficiency_pct)||0));effColor=eff>80?BAT:eff>60?SOLAR:WARN;setText('flow-eff-val',eff.toFixed(1)+'%',effColor);setText('flow-eff-loss','Loss '+fmt(a.p_loss_mw,0)+' mW',WARN);document.getElementById('flow-mini-load').style.width=pctLoad.toFixed(1)+'%';document.getElementById('flow-mini-load').style.opacity='1';document.getElementById('flow-mini-loss').style.width=pctLoss.toFixed(1)+'%';document.getElementById('flow-mini-loss').style.opacity='1';if(showBatterySink){document.getElementById('flow-mini-bat').style.width=pctBat.toFixed(1)+'%';document.getElementById('flow-mini-bat').style.opacity='1';setText('flow-mini-bat-txt',pctBat.toFixed(1)+'%',BAT)}else{document.getElementById('flow-mini-bat').style.width='0%';document.getElementById('flow-mini-bat').style.opacity='.14';setText('flow-mini-bat-txt','--',MUTED)}setText('flow-mini-load-txt',pctLoad.toFixed(1)+'%',LOAD);setText('flow-mini-loss-txt',pctLoss.toFixed(1)+'%',WARN);if(panel){panel.style.borderColor=effColor+'55';panel.style.boxShadow='inset 0 0 0 1px rgba(0,0,0,0), 0 0 0 rgba(0,0,0,0)'}}else{setText('flow-eff-val','N/A',MUTED);setText('flow-eff-loss','Loss N/A',MUTED);['flow-mini-load','flow-mini-bat','flow-mini-loss'].forEach(id=>{document.getElementById(id).style.width='0%';document.getElementById(id).style.opacity='.28'});setText('flow-mini-load-txt','--',MUTED);setText('flow-mini-bat-txt','--',MUTED);setText('flow-mini-loss-txt','--',MUTED);if(panel)panel.style.borderColor='var(--border)'}}
    function updateAnalysis(d){const a=d.analysis||{},s=d.solar||{},b=d.battery||{},l=d.load||{},mode=a.analysis_mode||'unavailable',batSigned=Number(d.battery_power_signed_mw)||0,balanceValid=!!a.balance_valid,batteryValid=!!(a.battery_estimate_valid&&b.ok),effArc=document.getElementById('eff-arc');const pctLoad=Math.max(0,Number(a.pct_load)||0),pctBat=Math.max(0,Number(a.pct_bat)||0),pctLoss=Math.max(0,Number(a.pct_loss)||0),showBatterySink=mode==='solar_input'&&pctBat>.05;const modeColor=mode==='solar_input'?SOLAR:mode==='battery_input'?BAT_DIS:mode==='mixed_input'?COMBO:MUTED;let effColor=MUTED;setText('analysis-mode',analysisModeText(mode),modeColor);if(balanceValid){const eff=Math.max(0,Math.min(100,Number(a.efficiency_pct)||0));effColor=eff>80?BAT:eff>60?SOLAR:WARN;effArc.style.strokeDashoffset=(201.1*(1-eff/100)).toFixed(1);effArc.setAttribute('stroke',effColor);setText('eff-pct',eff.toFixed(1)+'%',effColor);document.getElementById('bar-load').style.width=pctLoad.toFixed(1)+'%';document.getElementById('bar-loss').style.width=pctLoss.toFixed(1)+'%';document.getElementById('bar-load').style.opacity='1';document.getElementById('bar-loss').style.opacity='1';if(showBatterySink){document.getElementById('bar-bat').style.width=pctBat.toFixed(1)+'%';document.getElementById('bar-bat').style.opacity='1';setText('leg-bat-pct',pctBat.toFixed(1)+'%')}else{document.getElementById('bar-bat').style.width='0%';document.getElementById('bar-bat').style.opacity='.14';setText('leg-bat-pct','--')}setText('leg-load-pct',pctLoad.toFixed(1)+'%');setText('leg-loss-pct',pctLoss.toFixed(1)+'%');setText('leg-loss-mw',fmt(a.p_loss_mw,0)+' mW',WARN)}else{effArc.style.strokeDashoffset='201.1';effArc.setAttribute('stroke',MUTED);setText('eff-pct','N/A',MUTED);['bar-load','bar-bat','bar-loss'].forEach(id=>{document.getElementById(id).style.width='0%';document.getElementById(id).style.opacity='.28'});setText('leg-load-pct','--');setText('leg-bat-pct','--');setText('leg-loss-pct','--');setText('leg-loss-mw','N/A',MUTED)}setText('leg-load-mw',l.ok?fmt(l.power_mw,0)+' mW':'N/A',l.ok?LOAD:MUTED);setText('leg-bat-mw',b.ok?Math.abs(batSigned).toFixed(0)+' mW':'N/A',b.ok?(batSigned<0?BAT:batSigned>0?BAT_DIS:MUTED):MUTED);document.getElementById('ldot-bat').style.background=b.ok?(batSigned>0?BAT_DIS:BAT):MUTED;setText('an-p-solar',s.ok?fmt(s.power_mw,0)+' mW':'N/A',s.ok?SOLAR:MUTED);setText('an-iv-solar',s.ok?(fmt(s.current_ma,1)+' mA @ '+fmt(s.voltage,2)+' V'):'N/A');setText('an-p-load',l.ok?fmt(l.power_mw,0)+' mW':'N/A',l.ok?LOAD:MUTED);setText('an-iv-load',l.ok?(fmt(l.current_ma,1)+' mA @ '+fmt(l.voltage,2)+' V'):'N/A');let batLabel='Battery N/A',batColor=MUTED,batValue='N/A',batInfo='N/A';if(b.ok){if((d.battery_direction||'unknown')==='charging'){batLabel='Battery Charging';batColor=BAT}else if((d.battery_direction||'unknown')==='discharging'){batLabel='Battery Discharging';batColor=BAT_DIS}else{batLabel='Battery Idle';batColor=MUTED}batValue=fmtSigned(batSigned,0)+' mW';batInfo=fmtSigned(b.current_ma,1)+' mA @ '+fmt(b.voltage,2)+' V'}setText('an-bat-lbl',batLabel);setText('an-p-bat',batValue,batColor);setText('an-iv-bat',batInfo);setText('an-p-loss',balanceValid?fmt(a.p_loss_mw,0)+' mW':'N/A',balanceValid?WARN:MUTED);const dur=Number(a.session_duration_ms)||0,hh=Math.floor(dur/3600000),mm=Math.floor((dur%3600000)/60000),ss=Math.floor((dur%60000)/1000);setText('session-dur',String(hh).padStart(2,'0')+':'+String(mm).padStart(2,'0')+':'+String(ss).padStart(2,'0'),'#10b981');setText('s-solar',(Number(a.session_solar_wh)||0).toFixed(3)+' Wh','#f59e0b');setText('s-load',(Number(a.session_load_wh)||0).toFixed(3)+' Wh','#38bdf8');setText('s-bat',(Number(a.session_bat_in_wh!=null?a.session_bat_in_wh:a.session_bat_wh)||0).toFixed(3)+' Wh','#10b981');setText('s-bat-out',(Number(a.session_bat_out_wh)||0).toFixed(3)+' Wh',BAT_DIS);setText('s-loss',(Number(a.session_loss_wh)||0).toFixed(3)+' Wh','#ef4444');if(batteryValid){setText('b-crate',(Number(a.c_rate)||0).toFixed(2)+'C',BAT);document.getElementById('crate-fill').style.width=Math.min(100,(Number(a.c_rate)||0)*100).toFixed(0)+'%';document.getElementById('crate-fill').style.background=BAT;setText('b-full',Number(a.est_full_h)>0?formatHours(a.est_full_h):'N/A',BAT);setText('b-run',Number(a.est_runtime_h)>0?formatHours(a.est_runtime_h):'N/A',SOLAR)}else{setText('b-crate','N/A',MUTED);document.getElementById('crate-fill').style.width='0%';setText('b-full','N/A',MUTED);setText('b-run','N/A',MUTED)}setText('b-cap',(d.battery_capacity_mah||2000)+' mAh','var(--muted)');setText('b-avg-eff',balanceValid?(Number(a.efficiency_pct)||0).toFixed(1)+'%':'N/A',balanceValid?effColor:MUTED)}
    async function fetchRawHistory(){if(rawFetchBusy)return;rawFetchBusy=true;let statusNote='';try{let loops=0;while(loops<6){const ctrl=new AbortController(),tid=setTimeout(()=>ctrl.abort(),2000),url='/api/history/live?after_seq='+rawCursorSeq+'&limit='+runtimeRawFetchLimit,r=await fetch(url,{signal:ctrl.signal});clearTimeout(tid);const data=await r.json();if(data&&data.overflowed)statusNote='Raw buffer gap detected on ESP; browser FIFO history preserved and newest samples appended.';const appended=appendRawHistoryBatch(Array.isArray(data&&data.points)?data.points:[]);if(appended===0&&!data.truncated)break;if(!data.truncated)break;loops++}if(activeHistoryScale().source==='raw'||statusNote)renderHistoryFromBrowser(statusNote||undefined)}catch(e){}finally{rawFetchBusy=false;scheduleRawPoll()}}
    async function fetchArchiveHistory(force){if(archiveFetchBusy)return;const minAge=Math.max(5000,Math.min(30000,runtimeArchiveIntervalMs));if(!force&&(Date.now()-archiveLastFetchMs)<minAge)return;archiveFetchBusy=true;archiveLastFetchMs=Date.now();try{const ctrl=new AbortController(),tid=setTimeout(()=>ctrl.abort(),3000),url='/api/history?view=minutes&limit='+Math.max(60,runtimeArchiveEspCapacity),r=await fetch(url,{signal:ctrl.signal});clearTimeout(tid);const data=await r.json(),points=Array.isArray(data&&data.points)?data.points:[];archiveHistory.length=0;points.forEach(item=>archiveHistory.push(normalizeArchivePoint(item)));if(activeHistoryScale().source==='archive')renderHistoryFromBrowser()}catch(e){}finally{archiveFetchBusy=false}}
    async function fetchConfig(){
      try{
        const r=await fetch('/api/config');
        const c=await r.json();
        document.getElementById('cfg-device').value=c.device_id||'';
        document.getElementById('cfg-line').value=c.line_id||'';
        document.getElementById('ota-en').checked=!!c.ota_enabled;
        document.getElementById('ota-info').textContent='Host: '+(c.ota_hostname||'-')+' | Port: '+(c.ota_port||3232)+' | '+(c.ota_message||'');
        document.getElementById('fw').textContent='Firmware '+(c.firmware_version||'--')+(c.release_label?(' '+c.release_label):'');
        document.getElementById('build-stamp').textContent=c.build_stamp||'--';
        document.getElementById('hdr-mode').textContent=c.wifi_mode||'--';
        document.getElementById('hdr-device').textContent=c.device_id||'--';
        document.getElementById('hdr-interval').textContent=(c.sample_interval_ms||1000)+' ms report';
        if(document.getElementById('b-cap'))document.getElementById('b-cap').textContent=(c.battery_capacity_mah||2000)+' mAh';
        renderRuntimeConfig(c);
        renderWifiConfig(c,true);
        renderBatteryPresets(c.battery_presets||[]);
        if(document.getElementById('bat-profile-sel'))document.getElementById('bat-profile-sel').value=c.battery_profile_id||'';
        if(document.getElementById('bat-full'))document.getElementById('bat-full').value=Number(c.battery_full_voltage_v||4.2).toFixed(2);
        if(document.getElementById('bat-empty'))document.getElementById('bat-empty').value=Number(c.battery_empty_voltage_v||3.0).toFixed(2);
        if(!serverFormDirty&&!serverFormSaving){
          document.getElementById('srv-key').value='';
          document.getElementById('srv-key-clear').checked=false;
        }
        setText('bat-profile-line',batteryProfileText(c.battery_profile_label||'Custom',Number(c.battery_full_voltage_v||4.2),Number(c.battery_empty_voltage_v||3.0)));
        renderInaConfig(c,true);
        renderFlowThresholdConfig(c);
        renderLedPwmConfig(c);
        renderSenderStatus(c);
        setPoll(c.sample_interval_ms||1000);
        fetchArchiveHistory(true);
      }catch(e){}
    }
    async function fetchData(){
      try{
        const ctrl=new AbortController(),tid=setTimeout(()=>ctrl.abort(),3000);
        const r=await fetch('/api/status',{signal:ctrl.signal});
        clearTimeout(tid);
        const d=await r.json(),loadActive=!!d.load_active,loadOk=!!(d.load&&d.load.ok),batPctValid=!!d.battery_percent_valid,batPct=Number(d.battery_percent)||0,batArcText=batPctValid?(Math.round(batPct)+'%'):'N/A',batPctColor=batPctValid?(batPct>80?BAT:batPct>60?SOLAR:WARN):MUTED,batArcColor=!d.battery.ok?WARN:(d.battery_direction==='charging'?BAT:d.battery_direction==='discharging'?BAT_DIS:batPctColor);
        setPoll(d.sample_interval_ms||pollMs);
        flash('v-solar',fmt(d.solar.voltage,2)+' V');
        flash('i-solar','Current: '+fmt(d.solar.current_ma,1)+' mA');
        flash('p-solar','Power: '+fmt(d.solar.power_mw,0)+' mW');
        flash('solar-bus','Bus: '+fmt(d.solar.bus_voltage,2)+' V');
        flash('v-bat',fmt(d.battery.voltage,2)+' V');
        flash('i-bat','Current: '+fmtSigned(d.battery.current_ma,1)+' mA');
        flash('p-bat','Power: '+fmtSigned(d.battery_power_signed_mw,0)+' mW');
        flash('bat-bus','Bus: '+fmt(d.battery.bus_voltage,2)+' V | Shunt: '+fmt(d.battery.shunt_mv,2)+' mV');
        flash('v-load',loadOk?fmt(d.load.voltage,2)+' V':'N/A');
        flash('i-load',loadOk?('Current: '+fmt(d.load.current_ma,1)+' mA'):'Current: N/A');
        flash('p-load',loadOk?('Power: '+fmt(d.load.power_mw,0)+' mW'):'Power: N/A');
        flash('load-bus',loadOk?('Bus: '+fmt(d.load.bus_voltage,2)+' V | Shunt: '+fmt(d.load.shunt_mv,2)+' mV'):'Bus: N/A');
        setSolarVisual(!!d.solar_active,!!d.solar.ok);
        setBatteryVisual(d.battery_direction||'unknown',!!d.battery.ok);
        setLoadCardVisual(loadActive,loadOk);
        updateArc('arc-solar','arc-solar-txt',Number(d.solar.voltage)||0,0,10,!d.solar.ok?WARN:(d.solar_active?SOLAR:MUTED));
        updateArc('arc-bat','arc-bat-txt',batPctValid?batPct:0,0,100,batArcColor,batArcText);
        updateArc('arc-load','arc-load-txt',Number(d.load.voltage)||0,0,12,!loadOk?WARN:(loadActive?LOAD:MUTED));
        setText('bat-pct','Charge: '+(batPctValid?(Math.round(batPct)+'%'):'N/A'),batPctColor);
        setText('bat-profile-line',batteryProfileText(d.battery_profile_label||'Custom',Number(d.battery_full_voltage_v||4.2),Number(d.battery_empty_voltage_v||3.0)));
        updateFlow(d.visual_mode||'idle',!!d.solar_active,d.battery_direction||'unknown',loadActive,loadOk,d.solar.power_mw,d.load.power_mw,d.battery_power_signed_mw);
        if(d.analysis)updateFlowEfficiency(d);
        if(d.analysis&&d.load)updateAnalysis(d);
        const flow=flowCopy(d.visual_mode||'idle',d.battery_direction||'unknown',!!d.solar_active);
        document.getElementById('flow-val').textContent=flow[0];
        document.getElementById('flow-val').style.color=flow[2];
        document.getElementById('flow-arrow').textContent=flow[1];
        document.getElementById('flow-detail').textContent=d.sensor_health||'OK';
        document.getElementById('flow-warn').textContent=d.visual_warning||'';
        setBadge(d.state,d.state_label||d.state);
        document.getElementById('ip').textContent=d.ip||'--';
        document.getElementById('hdr-mode').textContent=d.wifi_mode||'--';
        document.getElementById('hdr-device').textContent=d.device_id||'--';
        document.getElementById('hdr-interval').textContent=(d.sample_interval_ms||pollMs)+' ms report';
        document.getElementById('fw').textContent='Firmware '+(d.firmware_version||'--')+(d.release_label?(' '+d.release_label):'');
        document.getElementById('build-stamp').textContent=d.build_stamp||'--';
        document.getElementById('ota-info').textContent='Host: '+(d.ota_hostname||'-')+' | Port: '+(d.ota_port||3232)+' | '+(d.ota_message||'-');
        if(document.getElementById('b-cap'))document.getElementById('b-cap').textContent=(d.battery_capacity_mah||2000)+' mAh';
        appendTrendHistorySample(d);
        renderRuntimeConfig(d);
        if(!archiveHistory.length||activeHistoryScale().source==='archive')fetchArchiveHistory(!archiveHistory.length);
        renderWifiConfig(d,false);
        renderInaConfig(d,false);
        renderFlowThresholdConfig(d);
        renderLedPwmConfig(d);
        renderSenderStatus(d);
        document.title='Solar Monitor '+(d.firmware_version||'--')+' '+(d.device_id||'');
        lastUpdate=Date.now();
      }catch(e){
        document.getElementById('live-dot').className='live-dot error';
        setBadge('OFFLINE','OFFLINE');
        document.getElementById('srv-status').textContent='status fetch failed';
      }
      schedulePoll();
    }
    async function saveWifiConfig(){const button=document.getElementById('save-wifi');button.textContent='Saving Wi-Fi...';const body=new URLSearchParams();body.set('wifi_ssid',document.getElementById('wifi-ssid').value);const pass=document.getElementById('wifi-pass').value;if(pass)body.set('wifi_pass',pass);try{const r=await fetch('/api/config/wifi',{method:'POST',body});const d=await r.json();if(!r.ok){alert(d.error||'Wi-Fi update failed');return}document.getElementById('wifi-pass').value='';document.getElementById('wifi-note').textContent=(d.message||'Wi-Fi saved, reboot scheduled')+' | Tunggu device reconnect';alert(d.message||'Wi-Fi saved, reboot scheduled');setTimeout(()=>{fetchConfig();fetchData()},4500)}catch(e){alert('Wi-Fi update failed')}finally{button.textContent='Save Wi-Fi & Reboot'}}
    async function saveDeviceConfig(){const body=new URLSearchParams();body.set('device_id',document.getElementById('cfg-device').value);body.set('line_id',document.getElementById('cfg-line').value);const r=await fetch('/api/config/device',{method:'POST',body});const d=await r.json();alert(d.message||'Saved');fetchConfig();fetchData()}
    async function saveRuntimeConfig(){const body=new URLSearchParams();body.set('sample_interval_ms',document.getElementById('cfg-interval').value);body.set('sensor_poll_interval_ms',document.getElementById('cfg-sensor-poll').value);body.set('history_interval_ms',document.getElementById('cfg-history-interval').value);body.set('chart_point_limit',document.getElementById('cfg-chart-points').value);body.set('ina_averaging_samples',document.getElementById('cfg-ina-avg').value);body.set('ina_bus_conv_us',document.getElementById('cfg-ina-bus-us').value);body.set('ina_shunt_conv_us',document.getElementById('cfg-ina-shunt-us').value);const r=await fetch('/api/config/runtime',{method:'POST',body});const d=await r.json();if(!r.ok){alert(d.error||'Runtime update failed');return}renderRuntimeConfig(d);alert(d.message||'Runtime updated');fetchConfig();fetchData()}
    async function saveInaConfig(channel){const body=new URLSearchParams();if(channel==='solar')body.set('solar_shunt_milliohms',document.getElementById('solar-shunt-mo').value);if(channel==='battery')body.set('battery_shunt_milliohms',document.getElementById('bat-shunt-mo').value);if(channel==='load')body.set('load_shunt_milliohms',document.getElementById('load-shunt-mo').value);const r=await fetch('/api/config/ina',{method:'POST',body});const d=await r.json();if(!r.ok){alert(d.error||'INA config update failed');return}renderInaConfig(d,true);alert(d.message||'INA config updated');fetchData()}
    async function saveFlowConfig(){flowFormSaving=true;document.getElementById('save-flow').textContent='Saving Flow Thresholds...';const body=new URLSearchParams();body.set('solar_active_threshold_mw',document.getElementById('flow-solar-threshold').value);body.set('load_active_threshold_mw',document.getElementById('flow-load-threshold').value);body.set('battery_flow_threshold_mw',document.getElementById('flow-battery-threshold').value);const r=await fetch('/api/config/flow',{method:'POST',body});const d=await r.json();if(!r.ok){flowFormSaving=false;document.getElementById('save-flow').textContent='Save Flow Thresholds *';alert(d.error||'Flow threshold update failed');return}clearFlowFormDirty(d.message||'Flow thresholds updated');alert(d.message||'Flow thresholds updated');fetchConfig();fetchData()}
    function buildLedPwmBody(){const body=new URLSearchParams(),flash=syncLedFlashInputs();if(document.getElementById('led-pwm-enable').checked)body.set('led_pwm_enabled','1');if(document.getElementById('led-pwm-flash-enable').checked)body.set('led_pwm_flash_enabled','1');if(document.getElementById('led-pwm-invert').checked)body.set('led_pwm_inverted','1');body.set('led_pwm_frequency_hz',document.getElementById('led-pwm-freq').value);body.set('led_pwm_duty_percent',document.getElementById('led-pwm-duty-num').value);body.set('led_pwm_flash_on_ms',flash.onMs);body.set('led_pwm_flash_period_ms',flash.periodMs);return body}
    async function applyLedPwmRequest(url,doneMsg,persist){ledFormSaving=true;document.getElementById('led-pwm-edit-note').textContent=persist?'Saving LED PWM...':'Applying LED PWM...';const r=await fetch(url,{method:'POST',body:buildLedPwmBody()});const d=await r.json();if(!r.ok){ledFormSaving=false;document.getElementById('led-pwm-edit-note').textContent=d.error||'LED PWM update failed';return false}renderLedPwmConfig(d);if(persist){clearLedPwmFormDirty(doneMsg||d.message||'LED PWM auto-saved')}else{ledFormSaving=false;document.getElementById('led-pwm-edit-note').textContent='Live apply active. Auto-saving...'}return true}
    async function applyLedPwmLive(){await applyLedPwmRequest('/api/control/led-pwm','LED PWM applied live',false)}
    async function saveLedPwm(manual){const ok=await applyLedPwmRequest('/api/config/led-pwm',manual?'LED PWM saved':'LED PWM auto-saved',true);if(ok){fetchConfig();fetchData()}}
    function scheduleLedPwmApply(){if(ledLiveTimer)clearTimeout(ledLiveTimer);ledLiveTimer=setTimeout(()=>applyLedPwmLive(),120);if(ledSaveTimer)clearTimeout(ledSaveTimer);ledSaveTimer=setTimeout(()=>saveLedPwm(false),700)}
    async function saveServerConfig(){serverFormSaving=true;document.getElementById('save-server').textContent='Saving Backend Sender...';const body=new URLSearchParams();if(document.getElementById('srv-enable').checked)body.set('server_enabled','1');body.set('api_base',document.getElementById('srv-base').value);const apiKey=document.getElementById('srv-key').value;if(apiKey)body.set('api_key',apiKey);if(document.getElementById('srv-key-clear').checked)body.set('clear_api_key','1');const r=await fetch('/api/config/server',{method:'POST',body});const d=await r.json();if(!r.ok){serverFormSaving=false;document.getElementById('save-server').textContent='Save Backend Sender *';alert(d.error||'Backend sender update failed');return}document.getElementById('srv-key').value='';document.getElementById('srv-key-clear').checked=false;clearServerFormDirty(d.message||'Backend sender updated');alert(d.message||'Backend sender updated');fetchConfig();fetchData()}
    async function saveBatteryProfile(){const body=new URLSearchParams();body.set('battery_profile_id',document.getElementById('bat-profile-sel').value);body.set('battery_full_voltage_v',document.getElementById('bat-full').value);body.set('battery_empty_voltage_v',document.getElementById('bat-empty').value);const r=await fetch('/api/config/battery',{method:'POST',body});const d=await r.json();if(!r.ok){alert(d.error||'Battery profile update failed');return}alert(d.message||'Battery profile saved');fetchConfig();fetchData()}
    async function applyOTA(){const body=new URLSearchParams();if(document.getElementById('ota-en').checked)body.set('ota_enabled','1');const r=await fetch('/api/config/ota',{method:'POST',body});const d=await r.json();alert(d.message||'OTA updated');fetchConfig();fetchData()}
    async function resetSession(){await fetch('/api/analysis/reset',{method:'POST'});fetchData()}
    async function scanI2C(){const r=await fetch('/api/i2c-scan');const d=await r.json();document.getElementById('i2c-result').textContent=(d.devices||[]).length?(d.devices.map(x=>x.address_hex+' '+x.label).join(' | ')):'No I2C devices detected'}
    window.addEventListener('load',()=>{
      loadHistoryScale();
      waitForCharts();
      fetchConfig();
      renderHistoryFromBrowser('Waiting for browser RAM live logs...');
      fetchRawHistory();
      fetchData();
      document.addEventListener('visibilitychange',()=>{schedulePoll();scheduleRawPoll()});
      setInterval(()=>{document.getElementById('last-upd').textContent=((Date.now()-lastUpdate)/1000).toFixed(1)+'s ago'},500);
      document.getElementById('save-wifi').onclick=saveWifiConfig;
      document.getElementById('save-device').onclick=saveDeviceConfig;
      document.getElementById('apply-interval').onclick=saveRuntimeConfig;
      document.getElementById('save-solar-shunt').onclick=()=>saveInaConfig('solar');
      document.getElementById('save-bat-shunt').onclick=()=>saveInaConfig('battery');
      document.getElementById('save-load-shunt').onclick=()=>saveInaConfig('load');
      document.getElementById('save-flow').onclick=saveFlowConfig;
      document.getElementById('apply-led-pwm').onclick=()=>saveLedPwm(true);
      document.getElementById('save-server').onclick=saveServerConfig;
      document.getElementById('save-bat-profile').onclick=saveBatteryProfile;
      document.getElementById('toggle-solar-setup').onclick=()=>document.getElementById('solar-setup').classList.toggle('open');
      document.getElementById('toggle-bat-setup').onclick=()=>document.getElementById('bat-setup').classList.toggle('open');
      document.getElementById('toggle-load-setup').onclick=()=>document.getElementById('load-setup').classList.toggle('open');
      document.getElementById('bat-profile-sel').onchange=e=>applyBatteryPreset(e.target.value);
      document.getElementById('apply-ota').onclick=applyOTA;
      document.getElementById('scan-i2c').onclick=scanI2C;
      document.getElementById('reset-session').onclick=resetSession;
      document.querySelectorAll('[data-history-scale]').forEach(btn=>btn.onclick=()=>setHistoryScale(btn.dataset.historyScale));
      document.getElementById('history-live-logs').onclick=()=>activateLiveLogs();
      document.getElementById('history-lock-y').onclick=toggleYAxisLock;
      document.getElementById('history-clear-browser').onclick=clearBrowserHistory;
      document.getElementById('flow-solar-threshold').oninput=markFlowFormDirty;
      document.getElementById('flow-load-threshold').oninput=markFlowFormDirty;
      document.getElementById('flow-battery-threshold').oninput=markFlowFormDirty;
      document.getElementById('led-pwm-enable').onchange=()=>{markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-flash-enable').onchange=()=>{markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-invert').onchange=()=>{markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-freq').oninput=()=>{markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-duty').oninput=()=>{syncLedDutyInputs('slider');markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-duty-num').oninput=()=>{syncLedDutyInputs('number');markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-flash-on').oninput=()=>{syncLedFlashInputs();markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('led-pwm-flash-period').oninput=()=>{syncLedFlashInputs();markLedPwmFormDirty();scheduleLedPwmApply()};
      document.getElementById('srv-enable').onchange=markServerFormDirty;
      document.getElementById('srv-base').oninput=markServerFormDirty;
      document.getElementById('srv-key').oninput=markServerFormDirty;
      document.getElementById('srv-key-clear').onchange=markServerFormDirty;
      clearFlowFormDirty();
      clearLedPwmFormDirty();
      clearServerFormDirty();
      syncLedDutyInputs('number');
      document.getElementById('reboot-btn').onclick=()=>{if(confirm('Reboot device?'))fetch('/api/reboot',{method:'POST'})}
    })
  </script>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.4.0/chart.umd.min.js" crossorigin="anonymous" async></script>
</body>
</html>
)dash";

}  // namespace

WebUi::WebUi(WifiService& wifiService,
             OtaService& otaService,
             BackendSender& backendSender,
             InaSensors& sensors,
             LedPwmController& ledPwmController,
             AnalysisSnapshot& analysis,
             I2cScanner& i2cScanner,
             RawHistoryBuffer& rawHistoryBuffer,
             HistoryBuffer& historyBuffer,
             HistoryBuffer& minuteHistoryBuffer,
             PowerSystemState& currentState)
    : wifiService_(wifiService),
      otaService_(otaService),
      backendSender_(backendSender),
      sensors_(sensors),
      ledPwmController_(ledPwmController),
      analysis_(analysis),
      i2cScanner_(i2cScanner),
      rawHistoryBuffer_(rawHistoryBuffer),
      historyBuffer_(historyBuffer),
      minuteHistoryBuffer_(minuteHistoryBuffer),
      currentState_(currentState) {}

void WebUi::begin() {
  registerRoutes_();
  server_.begin();
}

void WebUi::update() {
  server_.handleClient();
}

void WebUi::registerRoutes_() {
  server_.on("/", HTTP_GET, [this]() { handleRoot_(); });
  server_.on("/api/health", HTTP_GET, [this]() { handleHealth_(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleStatus_(); });
  server_.on("/api/data", HTTP_GET, [this]() { handleStatus_(); });
  server_.on("/api/history", HTTP_GET, [this]() { handleHistory_(); });
  server_.on("/api/history/live", HTTP_GET, [this]() { handleLiveHistory_(); });
  server_.on("/api/i2c/scan", HTTP_GET, [this]() { handleI2cScan_(); });
  server_.on("/api/i2c-scan", HTTP_GET, [this]() { handleI2cScan_(); });
  server_.on("/api/config", HTTP_GET, [this]() { handleConfig_(); });
  server_.on("/api/config/wifi", HTTP_POST, [this]() { handleSaveWifi_(); });
  server_.on("/api/config/ina", HTTP_POST, [this]() { handleSaveIna_(); });
  server_.on("/api/config/device", HTTP_POST, [this]() { handleSaveDevice_(); });
  server_.on("/api/config/flow", HTTP_POST, [this]() { handleSaveFlow_(); });
  server_.on("/api/config/server", HTTP_POST, [this]() { handleSaveServer_(); });
  server_.on("/api/config/runtime", HTTP_POST, [this]() { handleSaveRuntime_(); });
  server_.on("/api/config/led-pwm", HTTP_POST, [this]() { handleSaveLedPwm_(); });
  server_.on("/api/control/led-pwm", HTTP_POST, [this]() { handleControlLedPwm_(); });
  server_.on("/api/config/battery", HTTP_POST, [this]() { handleSaveBattery_(); });
  server_.on("/api/config/ota", HTTP_POST, [this]() { handleSaveOta_(); });
  server_.on("/api/ota", HTTP_POST, [this]() { handleSaveOta_(); });
  server_.on("/api/analysis/reset", HTTP_POST, [this]() { handleAnalysisReset_(); });
  server_.on("/api/reboot", HTTP_POST, [this]() { handleReboot_(); });
  server_.onNotFound([this]() { handleNotFound_(); });
}

void WebUi::handleRoot_() {
  if (wifiService_.isApMode()) {
    const String html = buildSetupHtml_();
    server_.send(200, "text/html", html);
    return;
  }

  server_.send_P(200, "text/html", kDashboardHtml);
}

void WebUi::handleHealth_() {
  DynamicJsonDocument doc(2048);
  const DeviceConfig& config = wifiService_.config();
  const WifiRuntime& wifiRuntime = wifiService_.runtime();
  doc["ok"] = true;
  doc["timestamp_ms"] = millis();
  doc["uptime_ms"] = millis();
  doc["firmware_version"] = FirmwareInfo::kVersion;
  doc["release_label"] = FirmwareInfo::kReleaseLabel;
  doc["build_date"] = FirmwareInfo::kBuildDate;
  doc["build_time"] = FirmwareInfo::kBuildTime;
  doc["build_stamp"] = FirmwareInfo::kBuildStamp;
  doc["wifi_mode"] = wifiService_.wifiModeName();
  doc["ip"] = wifiService_.ipAddress();
  doc["wifi_ssid"] = config.wifiSsid;
  doc["ota_enabled"] = config.otaEnabled;
  doc["ota_port"] = Config::kArduinoOtaPort;
  doc["ota_runtime_active"] = otaService_.isRuntimeActive();
  doc["ota_state"] = otaService_.stateText();
  doc["ota_message"] = otaService_.messageText();
  doc["sample_interval_ms"] = config.sampleIntervalMs;
  doc["sensor_health"] = sensorHealthText(sensors_.solar(), sensors_.battery(), sensors_.load());
  doc["load_ok"] = sensors_.load().ok;
  doc["i2c_map"] = buildInaMapText(config);
  doc["wifi_sta_connected"] = wifiService_.isStaConnected();
  doc["free_heap_bytes"] = ESP.getFreeHeap();
  doc["min_free_heap_bytes"] = ESP.getMinFreeHeap();
  JsonObject healthRoot = doc.as<JsonObject>();
  appendWifiRuntimeStatus(healthRoot, wifiRuntime);
  appendRuntimeConfig(
      healthRoot, config, sensors_, rawHistoryBuffer_, historyBuffer_, minuteHistoryBuffer_);
  appendInaConfig(healthRoot, config);
  appendLedPwmConfig(healthRoot, config, ledPwmController_.runtime());

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleStatus_() {
  DynamicJsonDocument doc(9216);
  const DeviceConfig& config = wifiService_.config();
  const WifiRuntime& wifiRuntime = wifiService_.runtime();
  const bool solarActive = isSolarActive(sensors_.solar(), config);
  const bool loadActive = isLoadActive(sensors_.load(), config);
  const BatteryDirection batteryDirection = evaluateBatteryDirection(sensors_.battery(), config);
  const float batteryPowerSignedMw = computeBatteryPowerSignedMw(sensors_.battery(), config);
  const VisualMode visualMode = evaluateVisualMode(sensors_.solar(), sensors_.battery(), config);
  const String visualWarning = visualWarningText(sensors_.solar(), sensors_.battery());
  const BackendSenderRuntime senderRuntime = backendSender_.runtimeSnapshot();
  const bool batteryPercentValid =
      sensors_.battery().ok &&
      isBatteryProfileRangeValid(config.batteryFullMv,
                                 config.batteryEmptyMv);
  const float batteryPercent =
      batteryPercentValid
          ? computeBatteryPercent(sensors_.battery().loadVoltageV,
                                  config.batteryFullMv,
                                  config.batteryEmptyMv)
          : 0.0f;

  doc["timestamp_ms"] = millis();
  doc["uptime_ms"] = millis();
  doc["firmware_version"] = FirmwareInfo::kVersion;
  doc["release_label"] = FirmwareInfo::kReleaseLabel;
  doc["build_date"] = FirmwareInfo::kBuildDate;
  doc["build_time"] = FirmwareInfo::kBuildTime;
  doc["build_stamp"] = FirmwareInfo::kBuildStamp;
  doc["device_id"] = wifiService_.config().deviceId;
  doc["line_id"] = wifiService_.config().lineId;
  doc["ip"] = wifiService_.ipAddress();
  doc["wifi_mode"] = wifiService_.wifiModeName();
  doc["wifi_ssid"] = config.wifiSsid;
  doc["state"] = powerSystemStateToText(currentState_);
  doc["state_label"] = powerSystemStateToLabel(currentState_);
  doc["ota_enabled"] = wifiService_.config().otaEnabled;
  doc["ota_port"] = Config::kArduinoOtaPort;
  doc["ota_hostname"] = wifiService_.config().deviceId;
  doc["ota_ready"] = otaService_.isRuntimeActive();
  doc["ota_runtime_active"] = otaService_.isRuntimeActive();
  doc["ota_state"] = otaService_.stateText();
  doc["ota_message"] = otaService_.messageText();
  doc["sensor_health"] = sensorHealthText(sensors_.solar(), sensors_.battery(), sensors_.load());
  doc["i2c_map"] = buildInaMapText(config);
  doc["solar_active"] = solarActive;
  doc["load_active"] = loadActive;
  doc["battery_direction"] = batteryDirectionToText(batteryDirection);
  doc["battery_power_signed_mw"] = batteryPowerSignedMw;
  doc["visual_mode"] = visualModeToText(visualMode);
  doc["visual_warning"] = visualWarning;
  doc["sample_interval_ms"] = config.sampleIntervalMs;
  doc["battery_capacity_mah"] = Config::kBatteryCapacityMah;
  doc["battery_percent"] = batteryPercent;
  doc["battery_percent_valid"] = batteryPercentValid;
  doc["api_key_configured"] = !config.apiKey.isEmpty();
  doc["wifi_sta_connected"] = wifiService_.isStaConnected();
  doc["free_heap_bytes"] = ESP.getFreeHeap();
  doc["min_free_heap_bytes"] = ESP.getMinFreeHeap();
  JsonObject statusRoot = doc.as<JsonObject>();
  appendWifiRuntimeStatus(statusRoot, wifiRuntime);
  appendRuntimeConfig(
      statusRoot, config, sensors_, rawHistoryBuffer_, historyBuffer_, minuteHistoryBuffer_);
  appendBatteryProfileFields(statusRoot, config);
  appendInaConfig(statusRoot, config);
  appendFlowThresholdConfig(statusRoot, config);
  appendLedPwmConfig(statusRoot, config, ledPwmController_.runtime());
  appendBackendSenderStatus(statusRoot, senderRuntime);

  JsonObject solarObject = doc.createNestedObject("solar");
  appendReading(solarObject, sensors_.solar());

  JsonObject batteryObject = doc.createNestedObject("battery");
  appendReading(batteryObject, sensors_.battery());

  JsonObject loadObject = doc.createNestedObject("load");
  appendReading(loadObject, sensors_.load());

  JsonObject analysisObject = doc.createNestedObject("analysis");
  appendAnalysis(analysisObject, analysis_);

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleHistory_() {
  const DeviceConfig& config = wifiService_.config();
  const HistoryViewMode viewMode = parseHistoryViewMode(server_.arg("view"));
  const HistoryBuffer& selectedBuffer =
      historyBufferForView(viewMode, historyBuffer_, minuteHistoryBuffer_);
  const uint32_t intervalMs = historyIntervalMsForView(config, viewMode);
  const size_t availableCount = selectedBuffer.count();

  uint32_t requestedLimit = server_.arg("limit").isEmpty() ? config.chartPointLimit
                                                           : server_.arg("limit").toInt();
  if (requestedLimit == 0U) {
    requestedLimit = config.chartPointLimit;
  }

  if (requestedLimit < Config::kMinChartPointLimit) {
    requestedLimit = Config::kMinChartPointLimit;
  }

  if (requestedLimit > selectedBuffer.capacity()) {
    requestedLimit = selectedBuffer.capacity();
  }

  const size_t emitCount = availableCount > requestedLimit ? requestedLimit : availableCount;
  const size_t startIndex = availableCount > emitCount ? availableCount - emitCount : 0U;

  const size_t docCapacity = 4096U + emitCount * 224U;
  DynamicJsonDocument doc(docCapacity);
  doc["view"] = historyViewModeText(viewMode);
  doc["sample_mode"] = "instant_snapshot";
  doc["sample_note"] = historySampleNote(viewMode);
  doc["count"] = emitCount;
  doc["available_count"] = availableCount;
  doc["capacity"] = selectedBuffer.capacity();
  doc["interval_ms"] = intervalMs;
  doc["history_interval_ms"] = intervalMs;
  doc["chart_point_limit"] = config.chartPointLimit;
  doc["ina_averaging_samples"] = config.inaAveragingSamples;

  JsonArray points = doc.createNestedArray("points");
  for (size_t index = startIndex; index < availableCount; ++index) {
    HistoryPoint point;
    if (!selectedBuffer.getOrdered(index, point)) {
      continue;
    }

    JsonObject item = points.createNestedObject();
    item["timestamp_ms"] = point.timestampMs;
    item["p_solar_mw"] = point.solarPowerMw;
    item["p_bat_mw"] = point.batteryPowerMw;
    item["p_bat_signed_mw"] = point.batteryPowerSignedMw;
    item["p_load_mw"] = point.loadPowerMw;
    item["v_solar_v"] = point.solarVoltageV;
    item["v_bat_v"] = point.batteryVoltageV;
    item["v_load_v"] = point.loadVoltageV;
    item["i_solar_ma"] = point.solarCurrentMa;
    item["i_bat_ma"] = point.batteryCurrentMa;
    item["i_load_ma"] = point.loadCurrentMa;
  }

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleI2cScan_() {
  I2cScanResult results[I2cScanner::kMaxResults];
  const size_t count = i2cScanner_.scan(results, I2cScanner::kMaxResults);

  DynamicJsonDocument doc(2048);
  doc["count"] = count;
  doc["truncated"] = count >= I2cScanner::kMaxResults;

  JsonArray devices = doc.createNestedArray("devices");
  for (size_t index = 0; index < count; ++index) {
    JsonObject item = devices.createNestedObject();
    item["address"] = results[index].address;
    item["address_hex"] = results[index].addressHex;
    item["label"] = results[index].label;
  }

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleConfig_() {
  DynamicJsonDocument doc(8192);
  const DeviceConfig& config = wifiService_.config();
  doc["device_id"] = config.deviceId;
  doc["line_id"] = config.lineId;
  doc["wifi_ssid"] = config.wifiSsid;
  doc["sample_interval_ms"] = config.sampleIntervalMs;
  doc["wifi_mode"] = wifiService_.wifiModeName();
  doc["server_enabled"] = config.serverEnabled;
  doc["backend_sender_enabled"] = config.serverEnabled;
  doc["ota_enabled"] = config.otaEnabled;
  doc["ota_port"] = Config::kArduinoOtaPort;
  doc["ota_hostname"] = config.deviceId;
  doc["ota_runtime_active"] = otaService_.isRuntimeActive();
  doc["ota_state"] = otaService_.stateText();
  doc["ota_message"] = otaService_.messageText();
  doc["api_base"] = config.apiBase;
  doc["api_key_configured"] = !config.apiKey.isEmpty();
  doc["ap_ssid"] = wifiService_.accessPointSsid();
  doc["firmware_version"] = FirmwareInfo::kVersion;
  doc["release_label"] = FirmwareInfo::kReleaseLabel;
  doc["build_date"] = FirmwareInfo::kBuildDate;
  doc["build_time"] = FirmwareInfo::kBuildTime;
  doc["build_stamp"] = FirmwareInfo::kBuildStamp;
  doc["battery_capacity_mah"] = Config::kBatteryCapacityMah;
  doc["i2c_map"] = buildInaMapText(config);
  JsonObject configRoot = doc.as<JsonObject>();
  appendRuntimeConfig(
      configRoot, config, sensors_, rawHistoryBuffer_, historyBuffer_, minuteHistoryBuffer_);
  appendBatteryProfileFields(configRoot, config);
  appendInaConfig(configRoot, config);
  appendFlowThresholdConfig(configRoot, config);
  appendLedPwmConfig(configRoot, config, ledPwmController_.runtime());
  appendBackendSenderStatus(configRoot, backendSender_.runtimeSnapshot());
  JsonArray batteryPresets = doc.createNestedArray("battery_presets");
  appendBatteryPresets(batteryPresets);

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveWifi_() {
  const String ssid = server_.arg("wifi_ssid");
  const String requestedPassword = server_.arg("wifi_pass");

  if (ssid.isEmpty()) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"wifi_ssid is required\"}");
    return;
  }

  const String password = requestedPassword.isEmpty() ? wifiService_.config().wifiPass : requestedPassword;
  wifiService_.saveWifiConfig(ssid, password);
  wifiService_.scheduleReboot(millis(), Config::kRebootDelayMs);

  DynamicJsonDocument doc(384);
  doc["ok"] = true;
  doc["message"] = "Wi-Fi saved, reboot scheduled";
  doc["wifi_ssid"] = wifiService_.config().wifiSsid;
  doc["wifi_mode"] = wifiService_.wifiModeName();

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleLiveHistory_() {
  const size_t availableCount = rawHistoryBuffer_.count();
  const uint32_t oldestSequence = rawHistoryBuffer_.oldestSequence();
  const uint32_t latestSequence = rawHistoryBuffer_.latestSequence();

  uint32_t afterSequence = server_.arg("after_seq").isEmpty() ? 0U : server_.arg("after_seq").toInt();
  uint32_t requestedLimit =
      server_.arg("limit").isEmpty() ? Config::kRawHistoryFetchLimit : server_.arg("limit").toInt();
  if (requestedLimit == 0U) {
    requestedLimit = Config::kRawHistoryFetchLimit;
  }

  if (requestedLimit > rawHistoryBuffer_.capacity()) {
    requestedLimit = rawHistoryBuffer_.capacity();
  }

  String body;
  body.reserve(768U + requestedLimit * 136U);
  body += F("{\"sample_mode\":\"sensor_poll_live\",");
  body += F("\"sample_note\":\"ESP keeps short raw burst; browser keeps and freezes old history.\",");
  body += F("\"after_seq\":");
  body += String(afterSequence);
  body += F(",\"oldest_seq\":");
  body += String(oldestSequence);
  body += F(",\"latest_seq\":");
  body += String(latestSequence);
  body += F(",\"capacity\":");
  body += String(static_cast<unsigned long>(rawHistoryBuffer_.capacity()));
  body += F(",\"available_count\":");
  body += String(static_cast<unsigned long>(availableCount));

  const bool overflowed = afterSequence != 0U && oldestSequence != 0U && afterSequence < (oldestSequence - 1U);
  body += F(",\"overflowed\":");
  body += overflowed ? F("true") : F("false");

  size_t emittedCount = 0U;
  bool truncated = false;
  body += F(",\"points\":[");
  bool firstPoint = true;
  for (size_t index = 0; index < availableCount; ++index) {
    RawHistoryPoint point;
    if (!rawHistoryBuffer_.getOrdered(index, point) || point.sequence <= afterSequence) {
      continue;
    }

    if (emittedCount >= requestedLimit) {
      truncated = true;
      break;
    }

    if (!firstPoint) {
      body += ',';
    }
    firstPoint = false;

    body += '[';
    body += String(point.sequence);
    body += ',';
    body += String(point.timestampMs);
    body += ',';
    body += jsonNumber(point.solarVoltageV);
    body += ',';
    body += jsonNumber(point.batteryVoltageV);
    body += ',';
    body += jsonNumber(point.loadVoltageV);
    body += ',';
    body += jsonNumber(point.solarCurrentMa);
    body += ',';
    body += jsonNumber(point.batteryCurrentMa);
    body += ',';
    body += jsonNumber(point.loadCurrentMa);
    body += ']';
    ++emittedCount;
  }
  body += F("],\"count\":");
  body += String(static_cast<unsigned long>(emittedCount));
  body += F(",\"truncated\":");
  body += truncated ? F("true") : F("false");
  body += F(",\"measured_hz\":");
  body += jsonNumber(sensors_.measuredReadRateHz(), 2U);
  body += F(",\"estimated_hz\":");
  body += jsonNumber(sensors_.timingProfile().estimatedChannelRateHz, 2U);
  body += '}';

  server_.send(200, "application/json", body);
}

void WebUi::handleSaveIna_() {
  const DeviceConfig& currentConfig = wifiService_.config();
  const String solarRaw = server_.arg("solar_shunt_milliohms");
  const String batteryRaw = server_.arg("battery_shunt_milliohms");
  const String loadRaw = server_.arg("load_shunt_milliohms");

  const float solarShuntMilliOhms =
      solarRaw.isEmpty() ? currentConfig.solarShuntMilliOhms : solarRaw.toFloat();
  const float batteryShuntMilliOhms =
      batteryRaw.isEmpty() ? currentConfig.batteryShuntMilliOhms : batteryRaw.toFloat();
  const float loadShuntMilliOhms =
      loadRaw.isEmpty() ? currentConfig.loadShuntMilliOhms : loadRaw.toFloat();

  if (!wifiService_.saveInaConfig(
          solarShuntMilliOhms, batteryShuntMilliOhms, loadShuntMilliOhms)) {
    server_.send(500, "application/json", "{\"ok\":false,\"error\":\"INA config save failed\"}");
    return;
  }

  sensors_.applyConfig(wifiService_.config());
  sensors_.refreshNow();

  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["message"] = "INA shunt config updated";
  doc["i2c_map"] = buildInaMapText(wifiService_.config());
  appendInaConfig(doc.as<JsonObject>(), wifiService_.config());

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveDevice_() {
  wifiService_.saveDeviceConfig(server_.arg("device_id"), server_.arg("line_id"));
  server_.send(200, "application/json", "{\"ok\":true,\"message\":\"Device config saved\"}");
}

void WebUi::handleSaveFlow_() {
  const String solarRaw = server_.arg("solar_active_threshold_mw");
  const String loadRaw = server_.arg("load_active_threshold_mw");
  const String batteryRaw = server_.arg("battery_flow_threshold_mw");

  const uint32_t solarThreshold =
      solarRaw.isEmpty() ? wifiService_.config().solarActiveThresholdMw : solarRaw.toInt();
  const uint32_t loadThreshold =
      loadRaw.isEmpty() ? wifiService_.config().loadActiveThresholdMw : loadRaw.toInt();
  const uint32_t batteryThreshold =
      batteryRaw.isEmpty() ? wifiService_.config().batteryFlowThresholdMw : batteryRaw.toInt();

  wifiService_.saveFlowConfig(solarThreshold, loadThreshold, batteryThreshold);

  DynamicJsonDocument doc(384);
  doc["ok"] = true;
  doc["message"] = "Flow thresholds updated";
  appendFlowThresholdConfig(doc.as<JsonObject>(), wifiService_.config());

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveServer_() {
  const String enabledRaw = server_.arg("server_enabled");
  const bool enabled = enabledRaw == "1" || enabledRaw == "true" || enabledRaw == "on" ||
                       enabledRaw == "yes";
  const bool clearApiKey = server_.hasArg("clear_api_key") &&
                           (server_.arg("clear_api_key") == "1" ||
                            server_.arg("clear_api_key") == "true" ||
                            server_.arg("clear_api_key") == "on" ||
                            server_.arg("clear_api_key") == "yes");

  wifiService_.saveBackendConfig(enabled,
                                 server_.arg("api_base"),
                                 server_.arg("api_key"),
                                 clearApiKey);

  DynamicJsonDocument doc(512);
  doc["ok"] = true;
  doc["message"] = enabled ? "Backend sender saved" : "Backend sender disabled";
  doc["server_enabled"] = wifiService_.config().serverEnabled;
  doc["api_base"] = wifiService_.config().apiBase;
  doc["api_key_configured"] = !wifiService_.config().apiKey.isEmpty();

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveRuntime_() {
  const DeviceConfig& currentConfig = wifiService_.config();
  uint32_t requestedReportInterval = server_.arg("sample_interval_ms").isEmpty()
                                         ? currentConfig.sampleIntervalMs
                                         : server_.arg("sample_interval_ms").toInt();
  uint32_t requestedSensorPollInterval = server_.arg("sensor_poll_interval_ms").isEmpty()
                                             ? currentConfig.sensorPollIntervalMs
                                             : server_.arg("sensor_poll_interval_ms").toInt();
  uint32_t requestedHistoryInterval = server_.arg("history_interval_ms").isEmpty()
                                          ? currentConfig.historyIntervalMs
                                          : server_.arg("history_interval_ms").toInt();
  uint32_t requestedChartPointLimit = server_.arg("chart_point_limit").isEmpty()
                                          ? currentConfig.chartPointLimit
                                          : server_.arg("chart_point_limit").toInt();
  uint32_t requestedInaAverage = server_.arg("ina_averaging_samples").isEmpty()
                                     ? currentConfig.inaAveragingSamples
                                     : server_.arg("ina_averaging_samples").toInt();
  uint32_t requestedInaBusConvUs = server_.arg("ina_bus_conv_us").isEmpty()
                                       ? currentConfig.inaBusConvTimeUs
                                       : server_.arg("ina_bus_conv_us").toInt();
  uint32_t requestedInaShuntConvUs = server_.arg("ina_shunt_conv_us").isEmpty()
                                         ? currentConfig.inaShuntConvTimeUs
                                         : server_.arg("ina_shunt_conv_us").toInt();

  if (requestedReportInterval == 0U) {
    requestedReportInterval = Config::kDefaultSampleIntervalMs;
  }
  if (requestedSensorPollInterval == 0U) {
    requestedSensorPollInterval = Config::kDefaultSensorPollIntervalMs;
  }
  if (requestedHistoryInterval == 0U) {
    requestedHistoryInterval = Config::kDefaultHistoryIntervalMs;
  }
  if (requestedChartPointLimit == 0U) {
    requestedChartPointLimit = Config::kDefaultChartPointLimit;
  }
  if (requestedInaAverage == 0U) {
    requestedInaAverage = Config::kDefaultInaAveragingSamples;
  }
  if (requestedInaBusConvUs == 0U) {
    requestedInaBusConvUs = Config::kDefaultInaBusConvTimeUs;
  }
  if (requestedInaShuntConvUs == 0U) {
    requestedInaShuntConvUs = Config::kDefaultInaShuntConvTimeUs;
  }

  wifiService_.saveRuntimeConfig(requestedReportInterval,
                                 requestedSensorPollInterval,
                                 requestedHistoryInterval,
                                 requestedChartPointLimit,
                                 requestedInaAverage,
                                 requestedInaBusConvUs,
                                 requestedInaShuntConvUs);
  sensors_.applyConfig(wifiService_.config());
  sensors_.setSampleIntervalMs(wifiService_.config().sensorPollIntervalMs);
  sensors_.refreshNow();

  DynamicJsonDocument doc(2048);
  doc["ok"] = true;
  doc["message"] = "Runtime sampling applied";
  JsonObject root = doc.as<JsonObject>();
  root["sample_interval_ms"] = wifiService_.config().sampleIntervalMs;
  appendRuntimeConfig(
      root, wifiService_.config(), sensors_, rawHistoryBuffer_, historyBuffer_, minuteHistoryBuffer_);

  String body;
  serializeJson(doc, body);
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveLedPwm_() {
  const String enabledRaw = server_.arg("led_pwm_enabled");
  const String flashEnabledRaw = server_.arg("led_pwm_flash_enabled");
  const String invertedRaw = server_.arg("led_pwm_inverted");
  const String frequencyRaw = server_.arg("led_pwm_frequency_hz");
  const String dutyRaw = server_.arg("led_pwm_duty_percent");
  const String flashOnRaw = server_.arg("led_pwm_flash_on_ms");
  const String flashPeriodRaw = server_.arg("led_pwm_flash_period_ms");

  const bool enabled = argIsTruthy(enabledRaw);
  const bool flashEnabled = argIsTruthy(flashEnabledRaw);
  const bool inverted = argIsTruthy(invertedRaw);
  const uint32_t frequencyHz =
      frequencyRaw.isEmpty() ? wifiService_.config().ledPwmFrequencyHz : frequencyRaw.toInt();
  const uint32_t dutyPercent =
      dutyRaw.isEmpty() ? wifiService_.config().ledPwmDutyPercent : dutyRaw.toInt();
  const uint32_t flashOnMs =
      flashOnRaw.isEmpty() ? wifiService_.config().ledPwmFlashOnMs : flashOnRaw.toInt();
  const uint32_t flashPeriodMs = flashPeriodRaw.isEmpty()
                                     ? wifiService_.config().ledPwmFlashPeriodMs
                                     : flashPeriodRaw.toInt();

  wifiService_.saveLedPwmConfig(enabled,
                                frequencyHz,
                                dutyPercent,
                                inverted,
                                flashEnabled,
                                flashOnMs,
                                flashPeriodMs);
  const bool attached = ledPwmController_.applyConfig(wifiService_.config());

  DynamicJsonDocument doc(896);
  doc["ok"] = attached;
  doc["message"] = attached ? "LED PWM saved" : "LED PWM saved, but PWM attach failed";
  appendLedPwmConfig(doc.as<JsonObject>(), wifiService_.config(), ledPwmController_.runtime());

  String body;
  serializeJson(doc, body);
  server_.send(attached ? 200 : 500, "application/json", body);
}

void WebUi::handleControlLedPwm_() {
  const String enabledRaw = server_.arg("led_pwm_enabled");
  const String flashEnabledRaw = server_.arg("led_pwm_flash_enabled");
  const String invertedRaw = server_.arg("led_pwm_inverted");
  const String frequencyRaw = server_.arg("led_pwm_frequency_hz");
  const String dutyRaw = server_.arg("led_pwm_duty_percent");
  const String flashOnRaw = server_.arg("led_pwm_flash_on_ms");
  const String flashPeriodRaw = server_.arg("led_pwm_flash_period_ms");

  const bool enabled = argIsTruthy(enabledRaw);
  const bool flashEnabled = argIsTruthy(flashEnabledRaw);
  const bool inverted = argIsTruthy(invertedRaw);
  const uint32_t frequencyHz =
      frequencyRaw.isEmpty() ? wifiService_.config().ledPwmFrequencyHz : frequencyRaw.toInt();
  const uint32_t dutyPercent =
      dutyRaw.isEmpty() ? wifiService_.config().ledPwmDutyPercent : dutyRaw.toInt();
  const uint32_t flashOnMs =
      flashOnRaw.isEmpty() ? wifiService_.config().ledPwmFlashOnMs : flashOnRaw.toInt();
  const uint32_t flashPeriodMs = flashPeriodRaw.isEmpty()
                                     ? wifiService_.config().ledPwmFlashPeriodMs
                                     : flashPeriodRaw.toInt();

  wifiService_.applyLedPwmConfig(enabled,
                                 frequencyHz,
                                 dutyPercent,
                                 inverted,
                                 flashEnabled,
                                 flashOnMs,
                                 flashPeriodMs);
  const bool attached = ledPwmController_.applyConfig(wifiService_.config());

  DynamicJsonDocument doc(896);
  doc["ok"] = attached;
  doc["message"] = attached ? "LED PWM applied live" : "LED PWM live apply failed";
  appendLedPwmConfig(doc.as<JsonObject>(), wifiService_.config(), ledPwmController_.runtime());

  String body;
  serializeJson(doc, body);
  server_.send(attached ? 200 : 500, "application/json", body);
}

void WebUi::handleSaveBattery_() {
  const String profileId = server_.arg("battery_profile_id");
  const String fullRaw = server_.arg("battery_full_voltage_v");
  const String emptyRaw = server_.arg("battery_empty_voltage_v");

  const float fullVoltage = fullRaw.toFloat();
  const float emptyVoltage = emptyRaw.toFloat();
  const uint32_t fullMv = static_cast<uint32_t>(lroundf(fullVoltage * 1000.0f));
  const uint32_t emptyMv = static_cast<uint32_t>(lroundf(emptyVoltage * 1000.0f));

  if (!isBatteryProfileRangeValid(fullMv, emptyMv)) {
    server_.send(
        400,
        "application/json",
        "{\"ok\":false,\"error\":\"battery_full_voltage_v and battery_empty_voltage_v are invalid\"}");
    return;
  }

  if (!wifiService_.saveBatteryConfig(profileId, fullMv, emptyMv)) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"battery profile save failed\"}");
    return;
  }

  String body;
  body.reserve(192);
  body += F("{\"ok\":true,\"message\":\"Battery profile saved\",\"battery_profile_id\":\"");
  body += wifiService_.config().batteryProfileId;
  body += F("\",\"battery_full_voltage_v\":");
  body += String(static_cast<float>(wifiService_.config().batteryFullMv) / 1000.0f, 2);
  body += F(",\"battery_empty_voltage_v\":");
  body += String(static_cast<float>(wifiService_.config().batteryEmptyMv) / 1000.0f, 2);
  body += '}';
  server_.send(200, "application/json", body);
}

void WebUi::handleSaveOta_() {
  const String rawValue = server_.arg("ota_enabled");
  const bool enabled =
      rawValue == "1" || rawValue == "true" || rawValue == "on" || rawValue == "yes";

  wifiService_.saveOtaConfig(enabled);

  String body;
  if (enabled) {
    const bool startedNow = otaService_.enable();
    if (startedNow) {
      body = "{\"ok\":true,\"message\":\"Arduino OTA enabled and advertising now.\"}";
    } else {
      body = "{\"ok\":true,\"message\":\"Arduino OTA enabled. Waiting for STA connection.\"}";
    }
  } else {
    otaService_.disable("Arduino OTA disabled from web dashboard.");
    body = "{\"ok\":true,\"message\":\"Arduino OTA disabled immediately.\"}";
  }

  server_.send(200, "application/json", body);
}

void WebUi::handleAnalysisReset_() {
  analysisResetSession(analysis_, millis());
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebUi::handleReboot_() {
  wifiService_.scheduleReboot(millis(), Config::kRebootDelayMs);
  server_.send(200, "application/json", "{\"ok\":true,\"message\":\"Reboot scheduled\"}");
}

void WebUi::handleNotFound_() {
  server_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
}

String WebUi::buildDashboardHtml_() const {
  return String(FPSTR(kDashboardHtml));
}

String WebUi::buildSetupHtml_() const {
  String html;
  html.reserve(5200);
  html += F(
      "<!doctype html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Solar Monitor Setup</title>"
      "<style>"
      "body{margin:0;font-family:'Segoe UI',Tahoma,sans-serif;background:linear-gradient(180deg,#f6f5ff,#ece8ff);color:#1f2544}"
      ".wrap{max-width:640px;margin:0 auto;padding:28px}.card{background:white;border-radius:18px;padding:20px;box-shadow:0 16px 40px rgba(31,37,68,.12)}"
      "h1{margin:0 0 8px;font-size:28px}.muted{color:#5b647b;margin-bottom:16px}label{display:block;margin-top:12px;margin-bottom:6px;font-weight:600}"
      "input,button{width:100%;padding:12px 14px;border-radius:12px;border:1px solid #d7d8ef;font-size:14px;box-sizing:border-box}button{margin-top:16px;background:#1f2544;color:white;border:none}"
      ".info{margin-top:16px;padding:12px 14px;border-radius:12px;background:#f5f7ff;color:#4b5570}.small{font-size:13px}.status{margin-top:14px;font-size:14px;color:#28416c}"
      "</style></head><body><div class='wrap'><div class='card'><h1>Solar Monitor Setup</h1><div class='muted'>Wi-Fi provisioning mode aktif.</div>"
      "<div class='info small'>Firmware: ");
  html += FirmwareInfo::kVersion;
  html += F(" ");
  html += FirmwareInfo::kReleaseLabel;
  html += F("<br>Build: ");
  html += FirmwareInfo::kBuildStamp;
  html += F("</div>"
      "<div class='info small'>AP SSID: ");
  html += htmlEscape(wifiService_.accessPointSsid());
  html += F("</div><form id='wifiForm'>"
            "<label>Wi-Fi SSID</label><input name='wifi_ssid' placeholder='Nama Wi-Fi' required>"
            "<label>Wi-Fi Password</label><input name='wifi_pass' type='password' placeholder='Password Wi-Fi'>"
            "<button type='submit'>Save Wi-Fi and Reboot</button></form>"
            "<form id='deviceForm'>"
            "<label>Device ID</label><input name='device_id' value='");
  html += htmlEscape(wifiService_.config().deviceId);
  html += F("'><label>Line ID</label><input name='line_id' value='");
  html += htmlEscape(wifiService_.config().lineId);
  html += F("'><button type='submit'>Save Device Identity</button></form>"
            "<div class='status' id='statusBox'>Ready.</div>"
            "<div class='info small'>Device akan mencoba mode STA setelah Wi-Fi disimpan. Jika gagal join, device kembali ke AP setup otomatis.</div>"
            "</div></div><script>"
            "const statusBox=document.getElementById('statusBox');"
            "async function postForm(formId,url){const form=document.getElementById(formId);const body=new URLSearchParams(new FormData(form));"
            "const res=await fetch(url,{method:'POST',body});const data=await res.json();if(!res.ok){throw new Error(data.error||'Request failed');}statusBox.textContent=data.message||'Saved';return data;}"
            "document.getElementById('wifiForm').addEventListener('submit',async e=>{e.preventDefault();try{const data=await postForm('wifiForm','/api/config/wifi');statusBox.textContent=data.message+' Device will reboot shortly.';}catch(err){statusBox.textContent=err.message;}});"
            "document.getElementById('deviceForm').addEventListener('submit',async e=>{e.preventDefault();try{const data=await postForm('deviceForm','/api/config/device');statusBox.textContent=data.message;}catch(err){statusBox.textContent=err.message;}});"
            "</script></body></html>");
  return html;
}
