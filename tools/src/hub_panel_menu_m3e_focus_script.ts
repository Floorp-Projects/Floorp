// SPDX-License-Identifier: MPL-2.0

export const HUB_PANEL_MENU_M3E_FOCUS_SCRIPT = `
  if (input.kind === "focus") {
    const task = getTaskState(input.testId, "C");
    input.__check = "focus-" + input.action;
    const role = input.role ?? "initial";
    const win = task.windows?.[role];
    const requireFocus = (condition) => { if (!condition) throw new Error("M3e focus precondition differs"); };
    const buttonId = "appMenu-floorp-hub-button";
    function visible(node) {
      if (!node?.isConnected || node.disabled) return false;
      const style = win.getComputedStyle(node);
      return style.display !== "none" && style.visibility !== "hidden" && node.getClientRects().length > 0;
    }
    function observation() {
      if (win.closed) return { at: Date.now(), closed: true, activeButton: false, buttonCount: 0 };
      const document = win.document;
      const button = document.getElementById(buttonId);
      const settings = document.getElementById("appMenu-settings-button");
      const trigger = document.getElementById("PanelUI-menu-button");
      const active = document.activeElement;
      const detail = (node) => ({ connected: !!node?.isConnected, visible: visible(node), hiddenAttribute: [null, "true", "false", ""].includes(node?.getAttribute("hidden")) ? node?.getAttribute("hidden") : "other", collapsed: node?.getAttribute("collapsed") === "true", disabled: !!node?.disabled, tabIndex: typeof node?.tabIndex === "number" ? node.tabIndex : null, tabIndexAttribute: [null, "-1", "0"].includes(node?.getAttribute("tabindex")) ? node?.getAttribute("tabindex") : "other", focusable: !!node && Services.focus.elementIsFocusable(node, 0) });
      return { at: Date.now(), closed: false, foreground: document.hasFocus() && Services.focus.activeWindow === win, popupOpen: win.document.getElementById("appMenu-popup").state === "open", triggerOpen: trigger?.getAttribute("open") === "true", activeButton: !!button && active === button, activeSettings: active === settings, activeTrigger: active === trigger, fallbackVisible: active === settings ? visible(settings) : active === trigger && visible(trigger), settings: detail(settings), trigger: detail(trigger), buttonCount: document.querySelectorAll("#" + buttonId).length, nodeId: nodeId(task, button), activeId: active?.id ?? null, activeTag: active?.localName ?? null, generation: task.registryOwners?.at(-1)?.generation ?? null };
    }
    if (input.action === "state") return observation();
    if (input.action === "open" || input.action === "prepare") {
      requireFocus(win && !win.closed && !task.focusProbe);
      win.focus();
      await waitFor(() => win.document.hasFocus() && Services.focus.activeWindow === win, 5000, "real foreground focus");
      const popup = win.document.getElementById("appMenu-popup");
      if (popup.state !== "open") {
        const event = waitForPopupEvent(popup, "popupshown", 5000);
        try { await win.PanelUI.show(); await event.promise; } finally { event.cancel(); }
      }
      const old = win.document.getElementById(buttonId);
      requireFocus(visible(old));
      if (input.action === "open") {
        if (role !== "late") return observation();
        const { NavigableManager } = ChromeUtils.importESModule("chrome://remote/content/shared/NavigableManager.sys.mjs");
        return { ...observation(), chromeHandle: NavigableManager.getIdForBrowsingContext(win.browsingContext), sandboxHandle: NavigableManager.getIdForBrowsingContext(task.windows.initial.browsingContext) };
      }
      requireFocus(observation().activeButton && observation().foreground);
      const audit = { preparedAt: Date.now(), actionAt: null, fallbackAt: null, old, oldNodeId: nodeId(task, old), events: [], removers: [], revocations: [], focusCalls: [] };
      const registry = ChromeUtils.importESModule(REGISTRY_URI).getSharedAppMenuRegistry();
      const revoke = registry.revoke;
      const revokeDescriptor = Object.getOwnPropertyDescriptor(registry, "revoke");
      Object.defineProperty(registry, "revoke", { configurable: true, writable: true, value: function(...args) {
        const before = observation();
        const result = revoke.apply(this, args);
        if (audit.revocations.length < 8) audit.revocations.push({ reason: args[1] === "replacement" ? "replacement" : args[1] === undefined || args[1] === "remove" ? "remove" : "other", before, after: observation() });
        return result;
      } });
      audit.removers.push(() => { if (revokeDescriptor) Object.defineProperty(registry, "revoke", revokeDescriptor); else delete registry.revoke; });
      for (const id of ["appMenu-settings-button", "PanelUI-menu-button"]) {
        const target = win.document.getElementById(id);
        const original = target.focus;
        const descriptor = Object.getOwnPropertyDescriptor(target, "focus");
        Object.defineProperty(target, "focus", { configurable: true, writable: true, value: function(...args) {
          const before = observation();
          const result = original.apply(this, args);
          if (audit.focusCalls.length < 8) audit.focusCalls.push({ target: id, focused: win.document.activeElement === target, at: Date.now(), before });
          return result;
        } });
        audit.removers.push(() => { if (descriptor) Object.defineProperty(target, "focus", descriptor); else delete target.focus; });
      }
      for (const type of ["focusin", "keydown", "pointerdown", "pointermove", "blur", "popuphidden", "unload"]) {
        const target = type === "blur" || type === "unload" ? win : win.document;
        const listener = (event) => {
          if (audit.events.length >= 128) throw new Error("M3e focus audit overflow");
          const current = win.closed ? null : win.document.getElementById(buttonId);
          const active = win.closed ? null : win.document.activeElement;
          const fallback = active?.id === "appMenu-settings-button" || active?.id === "PanelUI-menu-button";
          if (type === "focusin" && fallback && audit.fallbackAt === null) audit.fallbackAt = Date.now();
          audit.events.push({ type, trusted: event.isTrusted, at: Date.now(), fallback, newButton: type === "focusin" && !!current && current !== old && active === current, windowTarget: event.target === win || event.target === win.document });
        };
        target.addEventListener(type, listener, true);
        audit.removers.push(() => target.removeEventListener(type, listener, true));
      }
      task.focusProbe = audit;
      audit.claimListeners = new Map();
      audit.claimLiveAtAction = false;
      audit.claimInvalidatedAt = null;
      const document = win.document;
      const add = document.addEventListener;
      const remove = document.removeEventListener;
      const addDescriptor = Object.getOwnPropertyDescriptor(document, "addEventListener");
      const removeDescriptor = Object.getOwnPropertyDescriptor(document, "removeEventListener");
      Object.defineProperty(document, "addEventListener", { configurable: true, value: function(type, listener, options) {
        const result = add.call(this, type, listener, options);
        if (options === true && ["keydown", "pointerdown", "pointermove"].includes(type)) {
          const types = audit.claimListeners.get(listener) ?? new Set();
          types.add(type); audit.claimListeners.set(listener, types);
        }
        return result;
      } });
      Object.defineProperty(document, "removeEventListener", { configurable: true, value: function(type, listener, options) {
        const result = remove.call(this, type, listener, options);
        if (options === true && audit.claimListeners.get(listener)?.size === 3 && ["keydown", "pointerdown", "pointermove"].includes(type)) audit.claimInvalidatedAt ??= Date.now();
        return result;
      } });
      audit.removers.push(() => {
        if (addDescriptor) Object.defineProperty(document, "addEventListener", addDescriptor); else delete document.addEventListener;
        if (removeDescriptor) Object.defineProperty(document, "removeEventListener", removeDescriptor); else delete document.removeEventListener;
      });
      return { ...observation(), preparedAt: audit.preparedAt };
    }
    requireFocus(task.focusProbe && win);
    const audit = task.focusProbe;
    if (input.action === "act") {
      const atGate = observation();
      if (!(!win.closed && !audit.old.isConnected && atGate.foreground && atGate.popupOpen && atGate.activeSettings && atGate.fallbackVisible && audit.fallbackAt !== null && Date.now() - audit.preparedAt < 2500)) return { precondition: false, atGate, oldConnected: audit.old.isConnected, fallbackAt: audit.fallbackAt, elapsed: Date.now() - audit.preparedAt };
      audit.actionAt = Date.now();
      audit.claimLiveAtAction = [...audit.claimListeners.values()].some((types) => types.size === 3) && audit.claimInvalidatedAt === null;
      requireFocus(audit.claimLiveAtAction);
      let pointerTarget = null;
      if (input.caseId === "focus-pointer-cancellation") {
        const rect = win.document.getElementById("appMenu-settings-button").getBoundingClientRect();
        pointerTarget = { x: Math.round(rect.x + rect.width / 2), y: Math.round(rect.y + rect.height / 2) };
        requireFocus(rect.width > 0 && rect.height > 0 && Number.isFinite(pointerTarget.x) && Number.isFinite(pointerTarget.y) && pointerTarget.x >= 0 && pointerTarget.y >= 0 && pointerTarget.x < win.innerWidth && pointerTarget.y < win.innerHeight);
      } else if (input.caseId === "focus-movement-cancellation") {
        requireFocus(atGate.activeSettings);
        const target = win.document.getElementById("appMenu-new-tab-button2");
        audit.movementTarget = { present: !!target, visible: visible(target), focusable: !!target && Services.focus.elementIsFocusable(target, 0) };
        requireFocus(audit.movementTarget.visible && audit.movementTarget.focusable);
        target.focus();
        requireFocus(win.document.activeElement === target);
      } else if (input.caseId === "focus-blur-cancellation") {
        task.windows.second.focus();
        await waitFor(() => !win.document.hasFocus(), 1000, "real window blur");
      } else if (input.caseId === "focus-menu-close-cancellation") {
        const popup = win.document.getElementById("appMenu-popup");
        const event = waitForPopupEvent(popup, "popuphidden", 1000);
        try { popup.hidePopup(); await event.promise; } finally { event.cancel(); }
      } else if (input.caseId === "focus-navigation-cancellation") {
        win.gURLBar.focus();
        win.gBrowser.selectedBrowser.loadURI(Services.io.newURI("about:blank#hub-menu-focus-navigation"), { triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal() });
        await waitFor(() => win.gBrowser.selectedBrowser.currentURI.spec === "about:blank#hub-menu-focus-navigation", 1000, "actual focus navigation");
      } else if (input.caseId === "focus-window-close-intent-cancellation") {
        requireFocus(role === "late");
        win.close();
        await waitFor(() => win.closed, 1000, "actual close intent");
      } else if (input.caseId === "focus-bounded-expiry") {
        await delay(3100);
      } else requireFocus(["focus-restoration", "focus-keyboard-cancellation"].includes(input.caseId));
      return { precondition: true, before: atGate, after: observation(), actionAt: audit.actionAt, pointerTarget };
    }
    if (input.action === "gate-result" || input.action === "result") {
      const result = observation();
      if (input.action === "gate-result") audit.gatedCancellation = { gateCheckedAt: Date.now(), actionAt: audit.actionAt, claimLiveAtAction: audit.claimLiveAtAction, claimInvalidatedAt: audit.claimInvalidatedAt, events: audit.events.map((entry) => ({ ...entry })), buttonCount: result.buttonCount, activeButton: result.activeButton };
      return { ...result, gatedCancellation: audit.gatedCancellation ?? null, movementTarget: audit.movementTarget ?? null, claimLiveAtAction: audit.claimLiveAtAction, claimInvalidatedAt: audit.claimInvalidatedAt, preparedAt: audit.preparedAt, actionAt: audit.actionAt, fallbackAt: audit.fallbackAt, oldConnected: win.closed ? false : audit.old.isConnected, oldNodeId: audit.oldNodeId, events: audit.events.map((entry) => ({ ...entry })), revocations: audit.revocations, focusCalls: audit.focusCalls };
    }
    if (input.action === "monitor") {
      requireFocus(audit.fallbackAt !== null);
      const deadline = audit.fallbackAt + 3100;
      while (Date.now() < deadline) {
        const current = observation();
        requireFocus(current.foreground && current.fallbackVisible && !current.activeButton && current.buttonCount === 1 && !audit.events.some((event) => event.newButton));
        await delay(20);
      }
      return { ...observation(), fallbackAt: audit.fallbackAt, elapsed: Date.now() - audit.fallbackAt, events: audit.events.map((entry) => ({ ...entry })) };
    }
    if (input.action === "dispose") {
      for (const remove of audit.removers) remove();
      delete task.focusProbe;
      if (win.closed) { delete task.windows[role]; return { disposed: true, windowClosed: true }; }
      const popup = win.document.getElementById("appMenu-popup");
      if (popup.state === "open") {
        const event = waitForPopupEvent(popup, "popuphidden", 1000);
        try { popup.hidePopup(); await event.promise; } finally { event.cancel(); }
      }
      return { disposed: true, windowClosed: false };
    }
    throw new Error("unknown focus action");
  }
`;
