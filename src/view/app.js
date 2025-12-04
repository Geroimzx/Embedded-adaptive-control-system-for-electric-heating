document.addEventListener('DOMContentLoaded', () => {
    // --- Constants ---
    const API_STATUS = '/api/status';
    const API_ACTION = '/api/action';
    const API_SCHEDULE = '/api/schedule';
    const API_SETTINGS = '/api/settings';

    // --- State ---
    let currentManualTemp = 21.0;
    let fullSchedule = null; 
    let selectedDay = 1; 

    // --- UI References ---
    const manualSpElem = document.getElementById('manual-setpoint');
    const schedEditor = document.getElementById('schedule-editor');
    const dayBtns = document.querySelectorAll('.day-btn');
    const reloadBtn = document.getElementById('btn-reload');
    const configForm = document.getElementById('config-form');

    // ============================
    //      HELPER FUNCTIONS
    // ============================

    /**
     * Безпечне встановлення значення в input.
     * @param {string} id - ID елемента input
     * @param {number} val - значення з API
     * @param {number} def - дефолтне значення (якщо прийшло сміття)
     * @param {number} min - мінімально допустиме
     * @param {number} max - максимально допустиме
     * @param {number} prec - кількість знаків після коми (null для int)
     */
    function safeSetInput(id, val, def, min, max, prec) {
        const el = document.getElementById(id);
        if (!el) return;

        let v = parseFloat(val);
        
        if (isNaN(v) || v < min || v > max) {
            console.warn(`Value for ${id} was invalid (${val}), using default: ${def}`);
            v = def;
        }

        if (prec !== null) {
            el.value = v.toFixed(prec);
        } else {
            el.value = Math.round(v);
        }
    }

    // ============================
    //      STATUS & DASHBOARD
    // ============================

    const btnDec = document.getElementById('btn-dec-temp');
    const btnInc = document.getElementById('btn-inc-temp');

    if(btnDec && btnInc) {
        btnDec.addEventListener('click', () => changeTemp(-0.1));
        btnInc.addEventListener('click', () => changeTemp(0.1));
    }

    function changeTemp(delta) {
        currentManualTemp += delta;
        if (currentManualTemp < 5.0) currentManualTemp = 5.0;
        if (currentManualTemp > 35.0) currentManualTemp = 35.0;
        
        updateManualTempDisplay();
        sendTempAction(currentManualTemp);
    }

    function updateManualTempDisplay() {
        if(manualSpElem) manualSpElem.innerText = currentManualTemp.toFixed(1) + " °C";
    }

    async function sendTempAction(val) {
        try {
            await fetch(API_ACTION, {
                method: 'POST', 
                body: JSON.stringify({ action: 'set_temp', value: val })
            });
        } catch(e) { console.error("Temp set failed", e); }
    }

    async function fetchStatus() {
        try {
            const res = await fetch(API_STATUS);
            const data = await res.json();
            
            const elRoom = document.getElementById('temp-room');
            if(elRoom && data.room_temp !== undefined) {
                let t = data.room_temp;
                if(t < -50 || t > 100) elRoom.innerText = "--";
                else elRoom.innerText = t.toFixed(1);
            }

            const elOutside = document.getElementById('temp-outside');
            if(elOutside && data.outside_temp !== undefined) {
                let t = data.outside_temp;
                if(t < -90) elOutside.innerText = "--"; 
                else elOutside.innerText = t.toFixed(1);
            }

            const elSet = document.getElementById('current-setpoint');
            if(elSet && data.current_setpoint !== undefined) {
                elSet.innerText = data.current_setpoint.toFixed(1);
            }

            const elMode = document.getElementById('sys-mode');
            if(elMode) elMode.innerText = data.state;

            const elHeat = document.getElementById('heater-state');
            if(elHeat) {
                elHeat.innerText = data.relay ? "ON" : "OFF";
                elHeat.style.color = data.relay ? "#ff9800" : "#aaa";
            }
            
            if (data.manual_setpoint && Math.abs(data.manual_setpoint - currentManualTemp) > 0.1) {
                currentManualTemp = data.manual_setpoint;
                updateManualTempDisplay();
            }
            
            const badge = document.getElementById('connection-status');
            if(badge) {
                badge.className = "status-badge online";
                badge.innerText = "Online";
            }
        } catch (e) {
            const badge = document.getElementById('connection-status');
            if(badge) {
                badge.className = "status-badge offline";
                badge.innerText = "Offline";
            }
        }
    }

    // ============================
    //         SETTINGS
    // ============================

    async function loadSettings() {
        console.log("Loading settings...");
        try {
            const res = await fetch(API_SETTINGS);
            if(!res.ok) throw new Error("Settings fetch error");
            const cfg = await res.json();
            
            console.log("Settings received:", cfg);

            // WiFi
            if(document.getElementById('wifi_ssid')) document.getElementById('wifi_ssid').value = cfg.wifi.ssid || "";
            
            // MQTT
            if(document.getElementById('mqtt_host')) document.getElementById('mqtt_host').value = cfg.mqtt.host || "";
            safeSetInput('mqtt_port', cfg.mqtt.port, 1883, 1, 65535, null);
            if(document.getElementById('mqtt_token')) document.getElementById('mqtt_token').value = cfg.mqtt.token || "";

            // Geo
            if(cfg.geo) {
                safeSetInput('geo_lat', cfg.geo.lat, 50.27, -90, 90, 4);
                safeSetInput('geo_lon', cfg.geo.lon, 30.31, -180, 180, 4);
                safeSetInput('geo_interval', cfg.geo.interval, 15, 1, 1440, null);
            }

            // Controls
            if(cfg.control) {
                if (cfg.control.pwm_cycle_s !== undefined) {
                    safeSetInput('pwm_cycle', cfg.control.pwm_cycle_s, 60, 10, 300, null);
                }

                if (cfg.control.pid) {
                    safeSetInput('pid_kp', cfg.control.pid.kp, 10.0, 0, 1000, 1);
                    safeSetInput('pid_ki', cfg.control.pid.ki, 0.0, 0, 100, 2);
                    safeSetInput('pid_kd', cfg.control.pid.kd, 0.0, 0, 100, 2);
                }

                if (cfg.control.limits) {
                    safeSetInput('lim_rad', cfg.control.limits.rad_max, 60.0, 20, 95, 1);
                    safeSetInput('lim_min', cfg.control.limits.room_min, 10.0, 5, 30, 2);
                    safeSetInput('lim_max', cfg.control.limits.room_max, 30.0, 10, 40, 2);
                }
            }

        } catch (e) { 
            console.error("Load settings failed", e);
        }
    }

    if(configForm) {
        configForm.addEventListener('submit', async (e) => {
            e.preventDefault();

            const getVal = (id, type) => {
                const el = document.getElementById(id);
                if (!el) return 0;
                if (type === 'float') return parseFloat(el.value) || 0.0;
                if (type === 'int') return parseInt(el.value) || 0;
                return el.value;
            };

            const data = {
                wifi: {
                    ssid: document.getElementById('wifi_ssid').value,
                    pass: document.getElementById('wifi_pass').value
                },
                mqtt: {
                    host: document.getElementById('mqtt_host').value,
                    port: getVal('mqtt_port', 'int'),
                    token: document.getElementById('mqtt_token').value
                },
                geo: {
                    lat: getVal('geo_lat', 'float'),
                    lon: getVal('geo_lon', 'float'),
                    interval: getVal('geo_interval', 'int')
                },
                control: {
                    pwm_cycle_s: getVal('pwm_cycle', 'int'), 
                    pid: {
                        kp: getVal('pid_kp', 'float'),
                        ki: getVal('pid_ki', 'float'),
                        kd: getVal('pid_kd', 'float')
                    },
                    limits: {
                        rad_max: getVal('lim_rad', 'float'),
                        room_min: getVal('lim_min', 'float'),
                        room_max: getVal('lim_max', 'float')
                    }
                }
            };

            if(confirm("Save settings and reboot device?")) {
                try {
                    await fetch(API_SETTINGS, {
                        method: 'POST',
                        headers: {'Content-Type': 'application/json'},
                        body: JSON.stringify(data)
                    });
                    alert("Saved! System rebooting...");
                } catch(e) {
                    alert("Error saving settings");
                }
            }
        });
    }

    if(reloadBtn) reloadBtn.addEventListener('click', loadSettings);

    // ============================
    //         SCHEDULE
    // ============================

    dayBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            saveCurrentDayToMemory();
            dayBtns.forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
            selectedDay = parseInt(btn.dataset.day);
            renderScheduleDay();
        });
    });

    async function loadSchedule() {
        try {
            const res = await fetch(API_SCHEDULE);
            const data = await res.json();
            if(data.days && Array.isArray(data.days)) {
                fullSchedule = data.days;
                renderScheduleDay();
            }
        } catch(e) { console.error("Schedule load error", e); }
    }

    function renderScheduleDay() {
        if(!fullSchedule || !schedEditor) return;
        let dayData = fullSchedule[selectedDay];
        if(!dayData) dayData = { points: [] }; 

        schedEditor.innerHTML = '';
        dayData.points.forEach((pt, idx) => {
            const row = document.createElement('div');
            row.className = 'sched-row';
            row.innerHTML = `
                <span class="lbl">P${idx+1}</span>
                <input type="number" min="0" max="23" value="${pt.h}" class="sc-h"> :
                <input type="number" min="0" max="59" value="${pt.m}" class="sc-m">
                <span class="sep">-></span>
                <input type="number" step="0.5" min="5" max="35" value="${pt.t}" class="sc-t"> °C
            `;
            schedEditor.appendChild(row);
        });

        if(dayData.points.length === 0) {
            schedEditor.innerHTML = '<p>No points for this day</p>';
        }
    }

    function saveCurrentDayToMemory() {
        if(!fullSchedule || !schedEditor) return;
        const rows = schedEditor.querySelectorAll('.sched-row');
        if(rows.length === 0) return;

        const points = [];
        rows.forEach(row => {
            const h = parseInt(row.querySelector('.sc-h').value);
            const m = parseInt(row.querySelector('.sc-m').value);
            let t = parseFloat(row.querySelector('.sc-t').value);
            
            if(!isNaN(h) && !isNaN(m) && !isNaN(t)) {
                if (t < 5) t = 5; 
                if (t > 35) t = 35;
                points.push({ h, m, t });
            }
        });
        
        fullSchedule[selectedDay].points = points;
    }

    window.saveSchedule = async function() {
        saveCurrentDayToMemory(); 
        if(!fullSchedule) return;

        if(confirm("Upload schedule to device?")) {
            try {
                await fetch(API_SCHEDULE, {
                    method: 'POST',
                    body: JSON.stringify({ days: fullSchedule })
                });
                alert("Schedule Saved!");
            } catch(e) {
                alert("Failed to save schedule");
            }
        }
    };

    // ============================
    //      TABS & MODES
    // ============================

    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            
            btn.classList.add('active');
            const tabId = btn.dataset.tab;
            document.getElementById(tabId).classList.add('active');
            
            if(tabId === 'schedule' && !fullSchedule) {
                loadSchedule();
            }
            if(tabId === 'settings') {
                loadSettings();
            }
        });
    });
    
    document.querySelectorAll('.mode-btn').forEach(btn => {
        btn.addEventListener('click', async () => {
            await fetch(API_ACTION, {
                method: 'POST',
                body: JSON.stringify({ action: 'set_mode', mode: btn.dataset.mode })
            });
            setTimeout(fetchStatus, 200); 
        });
    });

    setInterval(fetchStatus, 2000);
    fetchStatus();
    loadSettings();
});