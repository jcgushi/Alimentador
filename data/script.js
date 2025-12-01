document.addEventListener("DOMContentLoaded", () => {
  const form = document.getElementById("timerForm");
  const timersList = document.getElementById("timersList");
  const timeInput = document.getElementById("time");
  const stepsInput = document.getElementById("steps");
  const timeForm = document.getElementById("timeForm");
  const wifiForm = document.getElementById("wifiForm");

  // estado do motor retornado pelo firmware
  let motorBusy = false;
  let motorStepsLeft = 0;
  let motorPending = 0;

  function pad2(n) { return String(n).padStart(2, "0"); }

  function renderTimers(timers) {
    timersList.innerHTML = "";
    timers.forEach((t, index) => {
      const li = document.createElement("li");
      const display = document.createElement("div");
      display.className = "row";
      display.innerHTML = `<strong>${pad2(t.hour)}:${pad2(t.minute)}</strong> — ${t.steps} passos`;

      const actions = document.createElement("div");
      actions.className = "actions";

      const delBtn = document.createElement("button");
      delBtn.textContent = "Excluir";
      delBtn.addEventListener("click", () => {
        if (!confirm('Excluir este timer?')) return;
        delBtn.disabled = true;
        fetch(`/deleteTimer?index=${index}`)
          .then(res => {
            if (!res.ok) throw new Error('Falha ao excluir (status ' + res.status + ')');
            return res.text();
          })
          .then(loadTimers)
          .catch(err => { delBtn.disabled = false; alert('Erro ao excluir: ' + err.message); });
      });

      const editBtn = document.createElement("button");
      editBtn.textContent = "Editar";
      editBtn.addEventListener("click", () => {
        // Remover nós atuais do li e montar editor usando createElement para evitar innerHTML/id conflicts
        while (li.firstChild) li.removeChild(li.firstChild);

        const editor = document.createElement("div");
        editor.className = "row";

        // label + time input
        const timeLabel = document.createElement('label');
        timeLabel.textContent = 'Horário: ';
        const timeInp = document.createElement('input');
        timeInp.type = 'time';
        timeInp.value = `${pad2(t.hour)}:${pad2(t.minute)}`;

        // label + steps input
        const stepsLabel = document.createElement('label');
        stepsLabel.textContent = ' Passos: ';
        const stepsInp = document.createElement('input');
        stepsInp.type = 'number';
        stepsInp.min = 1;
        // use MAX_STEPS_UI if available otherwise fallback
        stepsInp.max = (typeof MAX_STEPS_UI !== 'undefined') ? MAX_STEPS_UI : 100000;
        stepsInp.value = t.steps;

        // buttons
        const saveBtn = document.createElement('button'); saveBtn.textContent = 'Salvar';
        const cancelBtn = document.createElement('button'); cancelBtn.textContent = 'Cancelar';
        const testEditBtn = document.createElement('button'); testEditBtn.textContent = 'Testar dose';
        // se motor está ocupado, não permite testar
        if (motorBusy) testEditBtn.disabled = true;

        // append to editor and li
        timeLabel.appendChild(timeInp);
        stepsLabel.appendChild(stepsInp);
        editor.appendChild(timeLabel);
        editor.appendChild(stepsLabel);
        editor.appendChild(saveBtn);
        editor.appendChild(cancelBtn);
        editor.appendChild(testEditBtn);
        li.appendChild(editor);

        // helper to enable/disable all editor buttons
        function setEditorBusy(busy) {
          saveBtn.disabled = busy;
          cancelBtn.disabled = busy;
          testEditBtn.disabled = busy;
        }

        saveBtn.addEventListener('click', () => {
          const timeVal = timeInp.value;
          if (!timeVal) return alert('Horário inválido');
          const [h, m] = timeVal.split(':');
          const s = parseInt(stepsInp.value, 10);
          if (!Number.isInteger(s) || s <= 0) return alert('Passos inválidos');

          setEditorBusy(true);
          fetch(`/editTimer?index=${index}&hour=${h}&minute=${m}&steps=${s}`)
            .then(res => {
              if (!res.ok) return res.text().then(t => Promise.reject(t || ('Status ' + res.status)));
              return res.text();
            })
            .then(() => loadTimers())
            .catch(err => { alert('Erro ao editar: ' + err); setEditorBusy(false); });
        });

        cancelBtn.addEventListener('click', () => {
          loadTimers();
        });

        testEditBtn.addEventListener('click', () => {
          const s = parseInt(stepsInp.value, 10);
          if (!Number.isInteger(s) || s <= 0) return alert('Steps inválido');

          setEditorBusy(true);
          fetch(`/testTimer?steps=${s}`)
            .then(res => {
              if (!res.ok) return res.text().then(t => Promise.reject(t || ('Status ' + res.status)));
              return res.text();
            })
            .then(() => { alert('Teste acionado'); setEditorBusy(false); })
            .catch(e => { alert('Erro ao testar: ' + e); setEditorBusy(false); });
        });
      });

      const testBtn = document.createElement("button");
      testBtn.textContent = "Testar dose";
      // desabilita se motor já estiver ocupado
      testBtn.disabled = motorBusy;
      testBtn.addEventListener("click", () => {
        testBtn.disabled = true;
        fetch(`/testTimer?steps=${t.steps}`)
          .then(res => {
            if (!res.ok) return res.text().then(t => Promise.reject(t || ('Status ' + res.status)));
            return res.text();
          })
          .then(() => alert('Teste acionado'))
          .catch(e => alert('Erro ao testar: ' + e))
          .finally(() => testBtn.disabled = false);
      });

      actions.appendChild(editBtn);
      actions.appendChild(delBtn);
      actions.appendChild(testBtn);

      li.appendChild(display);
      li.appendChild(actions);
      timersList.appendChild(li);
    });
  }

  function loadTimers() {
    const statusEl = document.getElementById("timersMessage");
    if (statusEl) statusEl.textContent = "Carregando timers...";
    // timeout via AbortController
    const controller = new AbortController();
    const timeoutMs = 5000;
    const timerId = setTimeout(() => controller.abort(), timeoutMs);

    fetch("/timers.json", { signal: controller.signal })
      .then(res => {
        clearTimeout(timerId);
        if (!res.ok) throw new Error('Servidor retornou ' + res.status);
        return res.json();
      })
      .then(data => {
        if (!Array.isArray(data)) throw new Error('Formato inesperado da resposta');

        // validação mínima dos itens e filtragem
        const clean = data.filter(it => {
          return it && typeof it === 'object' &&
            Number.isInteger(it.hour) && it.hour >= 0 && it.hour <= 23 &&
            Number.isInteger(it.minute) && it.minute >= 0 && it.minute <= 59 &&
            Number.isInteger(it.steps) && it.steps > 0;
        });
        if (clean.length !== data.length) {
          console.warn('Alguns timers inválidos foram descartados', data);
        }

        renderTimers(clean);
        if (statusEl) statusEl.textContent = (clean.length > 0) ? "Timers carregados do dispositivo." : "Nenhum timer configurado.";
      })
      .catch(err => {
        if (err && err.name === 'AbortError') {
          if (statusEl) statusEl.textContent = "Timeout ao carregar timers.";
        } else {
          if (statusEl) statusEl.textContent = "Erro ao recuperar timers: " + (err && err.message ? err.message : '');
        }
        timersList.innerHTML = "<li>Falha ao carregar timers.</li>";
        console.error('loadTimers error:', err);
      });
  }

  function loadTime() {
    fetch("/time.json")
      .then(res => res.json())
      .then(data => {
        const el = document.getElementById("currentTime");
        if (data.formatted) {
          el.textContent = data.formatted;
        } else {
          el.textContent = "indisponível";
        }
      });
  }

  function loadWifiStatus() {
    fetch("/wifiStatus.json")
      .then(res => res.json())
      .then(data => {
        document.getElementById("localIp").textContent = data.localIp;
        document.getElementById("apIp").textContent = data.apIp;
        const statusEl = document.getElementById("wifiStatus");
        const linkWrap = document.getElementById("localLinkWrap");
        const link = document.getElementById("localLink");

        if (data.localIp && data.localIp !== "—" && data.localIp !== "Falha ao conectar") {
          statusEl.textContent = "Status: conectado em STA";
          link.href = `http://${data.localIp}/`;
          link.textContent = data.localIp;
          linkWrap.style.display = "block";
        } else {
          statusEl.textContent = "Status: não conectado em STA";
          linkWrap.style.display = "none";
        }
      });
  }

    // Carrega status do motor (ex.: ocupado / passos restantes)
    function loadMotorStatus() {
      const el = document.getElementById('motorStatus');
      fetch('/motorStatus.json')
        .then(res => {
          if (!res.ok) throw new Error('Status ' + res.status);
          return res.json();
        })
        .then(data => {
          motorBusy = !!data.busy;
          motorStepsLeft = data.stepsToGo || 0;
          motorPending = data.pending || 0;
          if (el) el.textContent = motorBusy ? `Motor ocupado — passos restantes: ${motorStepsLeft}` : 'Motor: ocioso';
          // atualizar botões de teste na lista sem re-renderizar
          document.querySelectorAll('#timersList button').forEach(b => {
            if (b.textContent && b.textContent.trim() === 'Testar dose') b.disabled = motorBusy;
          });
        })
        .catch(err => {
          if (el) el.textContent = 'Motor: erro';
          console.error('loadMotorStatus error', err);
        });
    }

  const MAX_STEPS_UI = 2000000; // deve acompanhar limite do firmware
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    const timeValue = timeInput.value; // "HH:MM"
    if (!timeValue) return;
    const [hour, minute] = timeValue.split(":");
    const stepsRaw = stepsInput.value;
    const steps = parseInt(stepsRaw, 10);
    if (!Number.isInteger(steps) || steps <= 0) return alert('Passos inválidos');
    if (steps > MAX_STEPS_UI) return alert('Passos muito grandes (limite ' + MAX_STEPS_UI + ')');

    fetch(`/addTimer?hour=${hour}&minute=${minute}&steps=${steps}`)
      .then(res => {
        if (!res.ok) return res.text().then(t => Promise.reject(t || ('Status ' + res.status)));
        return res.text();
      })
      .then(() => {
        timeInput.value = "";
        stepsInput.value = "";
        loadTimers();
      })
      .catch(e => alert('Erro ao adicionar timer: ' + e));
  });

  timeForm.addEventListener("submit", (e) => {
    e.preventDefault();
    const h = document.getElementById("manualHour").value;
    const m = document.getElementById("manualMinute").value;
    fetch(`/setTime?hour=${h}&minute=${m}`)
      .then(res => res.text())
      .then(msg => {
        alert(msg);
        loadTime(); // atualiza hora exibida imediatamente
      });
  });

  wifiForm.addEventListener("submit", (e) => {
    e.preventDefault();
    const ssid = document.getElementById("ssid").value;
    const pass = document.getElementById("password").value;
    fetch(`/setWifi?ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(pass)}`)
      .then(res => res.text())
      .then(msg => {
        document.getElementById("wifiStatus").textContent = "Status: " + msg;
        loadWifiStatus();
      });
  });

  // Inicializações
  loadTimers();
  loadTime();
  loadWifiStatus();
  // atualiza status do motor periodicamente
  loadMotorStatus();
  setInterval(loadMotorStatus, 2000);


  // Relatório removido — não há mais polling de /report.txt

  // Alternar seções pela barra de navegação (dentro do DOMContentLoaded)
  document.querySelectorAll("nav ul li a").forEach(link => {
    link.addEventListener("click", (e) => {
      e.preventDefault();
      const targetId = link.getAttribute("data-target");
      document.querySelectorAll(".page-section").forEach(sec => {
        sec.style.display = (sec.id === targetId) ? "block" : "none";
      });
    });
  });

});
