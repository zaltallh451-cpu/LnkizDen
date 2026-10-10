(() => {
  "use strict";

  const apiBase = "/api/v1";
  const $ = (selector) => document.querySelector(selector);
  const pairingPanel = $("#pairing-panel");
  const dashboard = $("#dashboard");
  const pairingForm = $("#pairing-form");
  const pairingError = $("#pairing-error");
  const connectionDot = $("#connection-dot");
  const connectionLabel = $("#connection-label");
  const toast = $("#toast");
  let toastTimer = 0;
  let activePanel = "overview";
  let refreshTimer = 0;

  async function request(path, options = {}) {
    const controller = new AbortController();
    const timeout = window.setTimeout(() => controller.abort(), 8000);
    try {
      const response = await fetch(`${apiBase}${path}`, {
        ...options,
        credentials: "same-origin",
        cache: "no-store",
        headers: {
          Accept: "application/json",
          ...(options.body ? { "Content-Type": "application/json" } : {}),
          ...options.headers,
        },
        signal: controller.signal,
      });
      if (response.status === 401) {
        showPairing();
        throw new Error("This browser is no longer paired. Pair it again to continue.");
      }
      if (!response.ok) {
        const message = await response.text();
        throw new Error(message || `Request failed (${response.status})`);
      }
      if (response.status === 204) return null;
      return await response.json();
    } finally {
      window.clearTimeout(timeout);
    }
  }

  function notify(message, isError = false) {
    toast.textContent = message;
    toast.style.borderColor = isError ? "var(--danger)" : "var(--line)";
    toast.classList.add("show");
    window.clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => toast.classList.remove("show"), 3600);
  }

  function setConnection(connected, label) {
    connectionDot.classList.toggle("online", connected);
    connectionLabel.textContent = label;
  }

  function showPairing() {
    window.clearInterval(refreshTimer);
    dashboard.hidden = true;
    pairingPanel.hidden = false;
    setConnection(false, "Not paired");
  }

  function showDashboard() {
    pairingPanel.hidden = true;
    dashboard.hidden = false;
    setConnection(true, "Paired");
  }

  function appendText(parent, tag, text, className) {
    const element = document.createElement(tag);
    element.textContent = text == null ? "" : String(text);
    if (className) element.className = className;
    parent.append(element);
    return element;
  }

  function clear(element, placeholder) {
    element.replaceChildren();
    if (placeholder) appendText(element, "p", placeholder, "muted");
  }

  async function pair(code) {
    await request("/pair", {
      method: "POST",
      body: JSON.stringify({ code }),
    });
    showDashboard();
    pairingForm.reset();
    await loadDashboard();
    beginStatusRefresh();
  }

  function renderServices(services) {
    const container = $("#service-list");
    clear(container);
    if (!Array.isArray(services) || services.length === 0) {
      appendText(container, "p", "No service status is available.", "muted");
      return;
    }
    for (const service of services) {
      const row = document.createElement("div");
      row.className = "service-row";
      const name = appendText(row, "span", service.name || "Service", "service-name");
      const state = appendText(row, "span", service.state || "unknown", "service-state");
      state.classList.add(["ready", "pending", "failed"].includes(service.state) ? service.state : "pending");
      container.append(row);
    }
  }

  function renderCompatibility(records) {
    const container = $("#compatibility-list");
    clear(container);
    if (!Array.isArray(records) || records.length === 0) {
      appendText(container, "p", "No compatibility evidence is published yet. Treat this setup as unverified.", "muted");
      return;
    }
    for (const record of records) {
      const card = document.createElement("article");
      card.className = "evidence-card";
      const top = document.createElement("div");
      top.className = "evidence-top";
      const heading = document.createElement("div");
      appendText(heading, "h3", record.setup || "Setup", "evidence-title");
      appendText(heading, "p", record.components || "Component details unavailable.");
      const label = record.confidence || "unknown";
      appendText(top, "span", label.replaceAll("_", " "), `badge ${label}`);
      top.prepend(heading);
      card.append(top);
      appendText(card, "p", record.evidence || "No supporting evidence attached.");
      appendText(card, "p", `Test basis: ${record.test_basis || "not recorded"}`);
      container.append(card);
    }
  }

  function renderTimeline(events) {
    const container = $("#timeline-list");
    clear(container);
    if (!Array.isArray(events) || events.length === 0) {
      appendText(container, "li", "No startup events have been recorded yet.", "muted");
      $("#latest-event").textContent = "No startup events have been recorded yet.";
      return;
    }
    for (const event of events) {
      const item = document.createElement("li");
      item.className = "timeline-item";
      appendText(item, "span", event.time || "Time unavailable", "timeline-time");
      const detail = document.createElement("span");
      appendText(detail, "span", event.stage || "Startup event", "timeline-title");
      appendText(detail, "span", event.detail || "", "timeline-detail");
      item.append(detail);
      const outcome = event.outcome || "unknown";
      appendText(item, "span", outcome, `badge ${outcome}`);
      container.append(item);
    }
    const latest = events[events.length - 1];
    $("#latest-event").textContent = `${latest.stage || "Startup event"} — ${latest.outcome || "unknown"}`;
  }

  function renderConflicts(report) {
    const container = $("#conflict-list");
    clear(container);
    if (!report || report.available !== true) {
      appendText(container, "article",
        "Forecast unavailable: conflict analysis requires supported cheat metadata. No conflicts are inferred.",
        "report-card");
      return;
    }
    if (!Array.isArray(report.findings) || report.findings.length === 0) {
      appendText(container, "article", "No conflicts were found in the available metadata. This is not a guarantee of runtime compatibility.", "report-card");
      return;
    }
    for (const finding of report.findings) {
      const row = document.createElement("article");
      row.className = "conflict-row";
      const description = document.createElement("div");
      appendText(description, "strong", finding.title || "Possible conflict");
      appendText(description, "p", finding.reason || "Reason not provided.");
      row.append(description);
      appendText(row, "span", finding.confidence || "reported", "badge reported");
      container.append(row);
    }
  }

  function renderRecovery(points) {
    const container = $("#recovery-list");
    clear(container);
    if (!Array.isArray(points) || points.length === 0) {
      appendText(container, "p", "No recovery points exist yet.", "muted");
      return;
    }
    for (const point of points) {
      const row = document.createElement("div");
      row.className = "recovery-row";
      const label = document.createElement("div");
      appendText(label, "strong", point.label || "Recovery point");
      appendText(label, "p", point.created_at || "Creation time unavailable", "muted");
      row.append(label);
      const button = document.createElement("button");
      button.className = "danger-button";
      button.type = "button";
      button.textContent = "Restore";
      button.addEventListener("click", async () => {
        const accepted = await confirmAction(
          "Restore settings?",
          "This replaces LnkizDen configuration with the selected recovery point. Sony system settings are not touched.");
        if (!accepted) return;
        try {
          await request("/recovery/restore", {
            method: "POST",
            body: JSON.stringify({ id: point.id }),
          });
          $("#recovery-message").textContent = "Recovery point restored. Reopen the Toolbox if a setting is not reflected immediately.";
          notify("LnkizDen settings restored.");
          await loadSettings();
        } catch (error) {
          notify(error.message, true);
        }
      });
      row.append(button);
      container.append(row);
    }
  }

  function setCheckbox(form, name, value) {
    const input = form.elements.namedItem(name);
    if (input) input.checked = value === true;
  }

  async function loadSettings() {
    const data = await request("/settings");
    const form = $("#overlay-form");
    const overlay = data.overlay || {};
    setCheckbox(form, "enabled", overlay.enabled);
    setCheckbox(form, "background", overlay.background);
    setCheckbox(form, "cpu", overlay.cpu);
    setCheckbox(form, "gpu", overlay.gpu);
    setCheckbox(form, "memory", overlay.memory);
    setCheckbox(form, "fps", overlay.fps);
    for (const name of ["edge", "align", "font_size"]) {
      if (overlay[name]) form.elements.namedItem(name).value = overlay[name];
    }
    renderRecovery(data.recovery_points);
  }

  async function loadStatus() {
    const status = await request("/status");
    $("#firmware-value").textContent = status.firmware || "Unknown";
    $("#firmware-evidence").textContent = status.firmware_evidence || "Detected locally; compatibility is not implied.";
    $("#app-version").textContent = status.version || "Unknown";
    $("#daemon-state").textContent = `Daemon: ${status.runtime && status.runtime.daemon || "unknown"}`;
    $("#toolbox-state").textContent = status.runtime && status.runtime.toolbox || "unknown";
    $("#util-state").textContent = `Utility: ${status.runtime && status.runtime.util || "unknown"}`;
    renderServices(status.services);
  }

  async function loadDashboard() {
    setConnection(true, "Refreshing");
    const results = await Promise.allSettled([
      loadStatus(),
      request("/compatibility").then((data) => renderCompatibility(data.records)),
      request("/timeline").then((data) => renderTimeline(data.events)),
      request("/cheats/conflicts").then(renderConflicts),
      loadSettings(),
    ]);
    const failed = results.find((result) => result.status === "rejected");
    if (failed) {
      setConnection(false, "Status unavailable");
      notify(failed.reason.message || "Could not load dashboard data.", true);
      return false;
    }
    setConnection(true, "Paired");
    return true;
  }

  function beginStatusRefresh() {
    window.clearInterval(refreshTimer);
    refreshTimer = window.setInterval(async () => {
      if (document.hidden || dashboard.hidden) return;
      try {
        await loadStatus();
        setConnection(true, "Paired");
      } catch (error) {
        setConnection(false, "Reconnecting");
      }
    }, 15000);
  }

  function confirmAction(title, message) {
    const dialog = $("#confirm-dialog");
    $("#confirm-title").textContent = title;
    $("#confirm-message").textContent = message;
    return new Promise((resolve) => {
      const done = () => resolve(dialog.returnValue === "confirm");
      dialog.addEventListener("close", done, { once: true });
      dialog.showModal();
    });
  }

  document.querySelectorAll(".tab").forEach((tab) => {
    tab.addEventListener("click", () => {
      activePanel = tab.dataset.panel;
      document.querySelectorAll(".tab").forEach((item) => item.classList.toggle("active", item === tab));
      document.querySelectorAll(".panel").forEach((panel) => {
        panel.hidden = panel.id !== `panel-${activePanel}`;
      });
    });
  });

  document.querySelectorAll("[data-go]").forEach((button) => {
    button.addEventListener("click", () => {
      const tab = document.querySelector(`.tab[data-panel="${button.dataset.go}"]`);
      if (tab) tab.click();
    });
  });

  pairingForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    pairingError.hidden = true;
    const code = String(new FormData(pairingForm).get("code") || "").trim();
    if (!/^\d{12}$/.test(code)) {
      pairingError.textContent = "Enter the 12-digit code displayed on the console.";
      pairingError.hidden = false;
      return;
    }
    const button = pairingForm.querySelector("button[type=submit]");
    button.disabled = true;
    try {
      await pair(code);
      notify("This browser is paired for the current console session.");
    } catch (error) {
      pairingError.textContent = error.message || "Pairing failed. Check the code and try again.";
      pairingError.hidden = false;
    } finally {
      button.disabled = false;
    }
  });

  $("#refresh-button").addEventListener("click", async () => {
    if (dashboard.hidden) return;
    try {
      await loadDashboard();
      notify("Dashboard refreshed.");
    } catch (error) {
      notify(error.message, true);
    }
  });

  $("#download-diagnostics").addEventListener("click", async () => {
    const accepted = await confirmAction(
      "Export diagnostics?",
      "The export contains only the dashboard’s whitelisted runtime and compatibility fields. Review the file before sharing it.");
    if (!accepted) return;
    try {
      const data = await request("/diagnostics");
      const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.href = url;
      link.download = "lnkizden-diagnostics.json";
      link.click();
      URL.revokeObjectURL(url);
      notify("Diagnostics exported. Review before sharing.");
    } catch (error) {
      notify(error.message, true);
    }
  });

  $("#create-backup").addEventListener("click", async () => {
    const accepted = await confirmAction(
      "Create a recovery point?",
      "A snapshot of LnkizDen configuration will be written to the console’s LnkizDen data directory.");
    if (!accepted) return;
    try {
      await request("/recovery/points", { method: "POST", body: JSON.stringify({}) });
      $("#recovery-message").textContent = "Recovery point created.";
      await loadSettings();
      notify("Recovery point created.");
    } catch (error) {
      notify(error.message, true);
    }
  });

  $("#overlay-form").addEventListener("submit", async (event) => {
    event.preventDefault();
    const accepted = await confirmAction(
      "Save overlay settings?",
      "This writes only LnkizDen overlay preferences. Changes may require reopening the Toolbox.");
    if (!accepted) return;
    const form = event.currentTarget;
    const data = Object.fromEntries(new FormData(form).entries());
    for (const name of ["enabled", "background", "cpu", "gpu", "memory", "fps"]) {
      data[name] = form.elements.namedItem(name).checked;
    }
    try {
      await request("/settings/overlay", { method: "POST", body: JSON.stringify(data) });
      $("#overlay-message").textContent = "Saved.";
      notify("Overlay settings saved.");
    } catch (error) {
      $("#overlay-message").textContent = error.message;
      notify(error.message, true);
    }
  });

  async function checkPairing() {
    try {
      if (!await loadDashboard()) return;
      showDashboard();
      beginStatusRefresh();
    } catch {
      showPairing();
    }
  }

  checkPairing();
})();
