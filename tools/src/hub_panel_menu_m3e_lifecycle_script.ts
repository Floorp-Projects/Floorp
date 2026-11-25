// SPDX-License-Identifier: MPL-2.0

export const HUB_PANEL_MENU_M3E_INITIAL_WINDOW_SCRIPT = `
  function initialWindow(existing) {
    if (window.closed || !window.browsingContext) return null;
    return existing.find((win) => !win.closed && win.browsingContext === window.browsingContext && !PrivateBrowsingUtils.isWindowPrivate(win)) ?? null;
  }
  function preparationWindows(existing) {
    const initial = initialWindow(existing);
    if (!initial) return null;
    const second = existing.find((win) => win !== initial) ?? null;
    return { initial, second, surplus: existing.filter((win) => win !== initial && win !== second) };
  }
`;

export const HUB_PANEL_MENU_M3E_LIFECYCLE_SCRIPT = `
  if (input.kind === "lifecycle") {
    const task = getTaskState(input.testId, "C");
    input.__check = input.action;
    const registry = ChromeUtils.importESModule(REGISTRY_URI).getSharedAppMenuRegistry();
    const hostModule = ChromeUtils.importESModule(FEATURE_HOST_URI);
    const host = task.hosts["hub-panel-menu"] ?? task.closedHosts["hub-panel-menu"];
    function windows() {
      const result = [];
      const enumerator = Services.wm.getEnumerator("navigator:browser");
      while (enumerator.hasMoreElements()) result.push(enumerator.getNext());
      return result;
    }
    function requireState(condition) { if (!condition) throw new Error("M3e lifecycle precondition differs"); }
    ${HUB_PANEL_MENU_M3E_INITIAL_WINDOW_SCRIPT}
    function windowContext() {
      const { NavigableManager } = ChromeUtils.importESModule("chrome://remote/content/shared/NavigableManager.sys.mjs");
      const handle = (win) => win && !win.closed ? NavigableManager.getIdForBrowsingContext(win.browsingContext) : null;
      return { scriptWindow: handle(window), preparation: task.preparationWindows ?? null, windows: windows().filter((win) => !win.closed).map((win) => ({ handle: handle(win), role: Object.entries(task.windows ?? {}).find(([, entry]) => entry === win)?.[0] ?? null, private: PrivateBrowsingUtils.isWindowPrivate(win), foreground: Services.focus.activeWindow === win })) };
    }
    async function menu(win, open) {
      const popup = win.document.getElementById("appMenu-popup");
      const trigger = win.document.getElementById("PanelUI-menu-button");
      requireState(popup && trigger);
      if ((popup.state === "open") === open) return;
      const event = waitForPopupEvent(popup, open ? "popupshown" : "popuphidden", 5000);
      try { if (open) { win.focus(); await win.PanelUI.show(); } else popup.hidePopup(); await event.promise; } finally { event.cancel(); }
    }
    async function ready(win, isPrivate) {
      await waitFor(() => !win.closed && win.gBrowserInit?.delayedStartupFinished && win.document.getElementById("PanelUI-menu-button"), 15000, "lifecycle window readiness");
      requireState(PrivateBrowsingUtils.isWindowPrivate(win) === isPrivate);
      await menu(win, true);
      await waitFor(() => win.document.getElementById("appMenu-settings-button"), 5000, "lifecycle Settings");
      if (isPrivate) await waitFor(() => win.document.getElementById("appMenu-floorp-hub-button")?.getAttribute("label")?.trim(), 5000, "private legacy button");
      await menu(win, false);
    }
    if (input.action === "window-context") {
      return windowContext();
    } else if (input.action === "interactive-menu") {
      const observations = [];
      for (const [role, win] of Object.entries(task.windows)) {
        if (PrivateBrowsingUtils.isWindowPrivate(win)) continue;
        const popup = win.document.getElementById("appMenu-popup");
        const trigger = win.document.getElementById("PanelUI-menu-button");
        const observation = { role, before: popup?.state ?? null, beforeSettingsPresent: !!win.document.getElementById("appMenu-settings-button"), context: windowContext(), shown: false, hidden: false };
        observations.push(observation);
        if (!popup || !trigger || popup.state !== "closed") return { passed: false, observations };
        win.focus();
        await waitFor(() => Services.focus.activeWindow === win, 3000, "interactive menu foreground");
        const shown = waitForPopupEvent(popup, "popupshown", 5000);
        try { trigger.click(); await shown.promise; observation.shown = true; } catch { observation.state = popup.state; observation.triggerOpen = trigger.open; observation.active = win.document.activeElement?.id ?? null; return { passed: false, observations }; } finally { shown.cancel(); }
        try { await waitFor(() => {
          const nodes = win.document.querySelectorAll("#appMenu-floorp-hub-button");
          return nodes.length === input.expectedCount && (nodes.length === 0 || nodes[0].getAttribute("label")?.trim() === input.expectedLabel);
        }, 5000, "interactive restored HUB"); } catch { observation.state = popup.state; observation.count = win.document.querySelectorAll("#appMenu-floorp-hub-button").length; return { passed: false, observations }; }
        const nodes = win.document.querySelectorAll("#appMenu-floorp-hub-button");
        observation.count = nodes.length;
        observation.label = nodes[0]?.getAttribute("label") ?? null;
        observation.nodeId = nodeId(task, nodes[0]);
        const settings = win.document.getElementById("appMenu-settings-button");
        observation.verified = nodes.length === input.expectedCount && (nodes.length === 0 || (!nodes[0].disabled && nodes[0].isConnected && observation.label === input.expectedLabel && nodes[0].previousElementSibling === settings));
        const hidden = waitForPopupEvent(popup, "popuphidden", 5000);
        try { popup.hidePopup(); await hidden.promise; observation.hidden = true; } finally { hidden.cancel(); }
        observation.after = popup.state;
      }
      return { passed: observations.length === 2 && observations.every((entry) => entry.shown && entry.hidden && entry.verified && entry.after === "closed"), observations };
    } else if (input.action === "observe-generations") {
      requireState(!task.registryObserver);
      const original = registry.beginGeneration;
      const descriptor = Object.getOwnPropertyDescriptor(registry, "beginGeneration");
      task.registryOwners = [];
      task.registryObserver = { restore() { if (descriptor) Object.defineProperty(registry, "beginGeneration", descriptor); else delete registry.beginGeneration; } };
      Object.defineProperty(registry, "beginGeneration", { configurable: true, writable: true, value: function(...args) {
        const lease = original.apply(this, args);
        task.registryOwners.push({ featureId: lease.owner.featureId, hostEpoch: lease.owner.hostEpoch, generation: lease.owner.generation, leaseId: lease.id });
        return lease;
      } });
      return { observing: true };
    } else if (input.action === "prepare-windows") {
      const existing = windows();
      const plan = preparationWindows(existing);
      const initial = plan?.initial;
      requireState(initial && !task.windows && (input.reopen === true ? existing.length <= 8 && existing.every((win) => !PrivateBrowsingUtils.isWindowPrivate(win) && !win.document.getElementById("appMenu-floorp-hub-button")) : existing.length === 1));
      task.windows = { initial };
      task.preparationWindows = { restoredNormalCount: existing.length, closedSurplus: [] };
      for (const win of plan.surplus) {
        const context = windowContext().windows.find((entry) => entry.handle === ChromeUtils.importESModule("chrome://remote/content/shared/NavigableManager.sys.mjs").NavigableManager.getIdForBrowsingContext(win.browsingContext));
        win.close();
        await waitFor(() => win.closed, 5000, "surplus restored window close");
        task.preparationWindows.closedSurplus.push({ ...context, closed: win.closed });
      }
      task.windows.second = plan.second ?? initial.OpenBrowserWindow();
      await ready(task.windows.second, false);
      task.windows.private = initial.OpenBrowserWindow({ private: true });
      await ready(task.windows.private, true);
      task.privateNode = task.windows.private.document.getElementById("appMenu-floorp-hub-button");
      await waitFor(() => registry.diagnosticsForTests.connections === 2, 10000, "second real adapter");
    } else if (input.action === "new-window") {
      requireState(!task.windows.late);
      task.windows.late = task.windows.initial.OpenBrowserWindow();
      await ready(task.windows.late, false);
      await waitFor(() => registry.diagnosticsForTests.connections === 3 && !!task.windows.late.document.getElementById("appMenu-floorp-hub-button"), 10000, "late signed adapter");
    } else if (input.action === "retain") {
      task.retained ??= Object.create(null);
      const service = Components.classes["@mozilla.org/eventlistenerservice;1"].getService(Components.interfaces.nsIEventListenerService);
      task.retained[input.tag] = [];
      for (const [role, win] of Object.entries(task.windows)) {
        if (PrivateBrowsingUtils.isWindowPrivate(win)) continue;
        const node = win.document.getElementById("appMenu-floorp-hub-button");
        requireState(node && !node.disabled);
        const handlers = service.getListenerInfoFor(node).filter((info) => info.type === "command").map((info) => info.listenerObject).filter(Boolean);
        requireState(handlers.length > 0);
        task.retained[input.tag].push({ role, win, node, handlers });
      }
      return { retained: task.retained[input.tag].length, handlers: task.retained[input.tag].reduce((count, item) => count + item.handlers.length, 0) };
    } else if (input.action === "command" || input.action === "stale") {
      const probes = [];
      const calls = [];
      const opened = [];
      const locations = [];
      const hubUrl = (url) => url === "about:hub" || url === "about:hub/" || url.startsWith("chrome://noraneko-hub/content/index.html");
      let result;
      try {
        for (const win of windows()) {
          const browser = win.gBrowser;
          const original = browser.addTab;
          const descriptor = Object.getOwnPropertyDescriptor(browser, "addTab");
          const before = new Set(browser.tabs);
          const selected = browser.selectedTab;
          const listener = { onLocationChange(target, progress, request, location, flags) { if (progress.isTopLevel && hubUrl(location.spec) && !(flags & Components.interfaces.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) locations.push(target); } };
          const onTab = (event) => opened.push(event.target);
          const probe = { win, browser, original, descriptor, before, selected, listener, onTab };
          probes.push(probe);
          browser.tabContainer.addEventListener("TabOpen", onTab);
          browser.addTabsProgressListener(listener);
          Object.defineProperty(browser, "addTab", { configurable: true, writable: true, value: function(url, options) { const tab = original.call(this, url, options); calls.push({ url, tab, win }); return tab; } });
        }
        if (input.action === "command") {
          const win = task.windows[input.role ?? "initial"];
          await menu(win, true);
          const button = win.document.getElementById("appMenu-floorp-hub-button");
          requireState(button && !button.disabled);
          button.click();
          await waitFor(() => calls.length && hubUrl(calls[0].tab.linkedBrowser.currentURI.spec) && !calls[0].tab.linkedBrowser.webProgress.isLoadingDocument, 15000, "actual about hub load");
          await delay(100);
          requireState(calls.length === 1 && calls[0].url === "about:hub" && opened.length === 1 && locations.length === 1 && calls[0].win.gBrowser.selectedTab === calls[0].tab);
          result = { tabOpens: opened.length, hubLoads: locations.length, addTabCalls: calls.length, selected: true };
        } else {
          const retained = task.retained?.[input.tag];
          requireState(retained?.length > 0);
          let handlersInvoked = 0;
          for (const item of retained) {
            requireState(!item.node.isConnected);
            item.node.doCommand();
            for (const handler of item.handlers) {
              const event = new item.win.Event("command", { bubbles: true });
              const returned = typeof handler === "function" ? handler.call(item.node, event) : handler.handleEvent(event);
              requireState(returned === false || returned === undefined);
              handlersInvoked++;
            }
          }
          await delay(250);
          requireState(opened.length === 0 && calls.length === 0 && locations.length === 0);
          result = { detachedNodes: retained.length, handlersInvoked, tabOpens: 0, hubLoads: 0 };
        }
      } finally {
        for (const probe of probes.reverse()) {
          if (probe.descriptor) Object.defineProperty(probe.browser, "addTab", probe.descriptor); else delete probe.browser.addTab;
          probe.browser.tabContainer.removeEventListener("TabOpen", probe.onTab);
          probe.browser.removeTabsProgressListener(probe.listener);
          for (const tab of [...probe.browser.tabs]) if (!probe.before.has(tab)) probe.browser.removeTab(tab, { animate: false });
          if (probe.before.has(probe.selected)) probe.browser.selectedTab = probe.selected;
        }
      }
      return result;
    } else if (input.action === "reject-operation") {
      requireState(host);
      let rejected = false;
      try {
        if (input.operation === "stage") await host.stage(Uint8Array.from(atob(input.packageBase64), (c) => c.charCodeAt(0)));
        else await host[input.operation](input.txId, input.expectedRevision, input.packageSha256);
      } catch (error) {
        if (error.code !== input.expectedCode || !["operation-failed", "sequence-too-low"].includes(error.code)) throw new Error("M3e unexpected rejection category");
        rejected = true;
      }
      requireState(rejected);
      return { rejected: true, code: input.expectedCode };
    } else if (input.action === "passive-ui") {
      return { registry: registry.diagnosticsForTests, diagnostics: host ? host.diagnostics() : null, evaluationCount: hostModule.m0PayloadEvaluationsForTests(), windows: windows().map((win) => ({ role: Object.entries(task.windows ?? {}).find(([, entry]) => entry === win)?.[0] ?? null, closed: win.closed, foreground: Services.focus.activeWindow === win, popupState: win.document.getElementById("appMenu-popup")?.state ?? null, buttonCount: win.document.querySelectorAll("#appMenu-floorp-hub-button").length, label: win.document.getElementById("appMenu-floorp-hub-button")?.getAttribute("label")?.slice(0, 128) ?? null })) };
    } else if (input.action === "operation-close-window") {
      const win = task.windows.second;
      requireState(win && !PrivateBrowsingUtils.isWindowPrivate(win));
      win.close();
      await waitFor(() => win.closed && registry.diagnosticsForTests.connections === 1, 5000, "operation window cleanup");
      task.operationClosedWindow = win;
      delete task.windows.second;
      return { closed: win.closed, connections: registry.diagnosticsForTests.connections };
    } else if (input.action === "operation-closed-window-command") {
      const entry = task.retained?.[input.tag]?.find((item) => item.role === "second");
      requireState(entry && task.operationClosedWindow?.closed && !entry.node.isConnected);
      const before = task.windows.initial.gBrowser.tabs.length;
      const returns = [];
      for (const handler of entry.handlers) {
        const event = new task.windows.initial.Event("command", { bubbles: true });
        const returned = typeof handler === "function" ? handler.call(entry.node, event) : handler.handleEvent(event);
        requireState(returned === false || returned === undefined);
        returns.push(returned === false ? "rejected" : "ignored");
      }
      await delay(100);
      requireState(task.windows.initial.gBrowser.tabs.length === before);
      return { closed: true, detached: true, handlers: returns.length, results: returns, tabDelta: 0 };
    } else if (input.action === "operation-begin-host-close") {
      requireState(host && !task.closingHost);
      task.closingHost = { done: false, failed: false };
      task.closingHost.promise = host.close().then(() => { task.closingHost.done = true; task.closedHosts["hub-panel-menu"] = host; delete task.hosts["hub-panel-menu"]; }, () => { task.closingHost.done = true; task.closingHost.failed = true; });
      return { closeRequested: true, settled: task.closingHost.done };
    } else if (input.action === "operation-await-host-close") {
      requireState(task.closingHost);
      await waitFor(() => task.closingHost.done, 15000, "queued host cleanup");
      requireState(!task.closingHost.failed);
      task.registryObserver?.restore();
      delete task.registryObserver;
      return { closed: true, cleanup: host.cleanupDiagnosticsForTests() };
    } else if (input.action === "close-windows") {
      for (const [role, win] of Object.entries(task.windows)) if (role !== "initial") { win.close(); await waitFor(() => win.closed, 5000, "owned window close"); }
      return { closed: true, context: windowContext() };
    } else if (input.action !== "snapshot") {
      throw new Error("unknown lifecycle action");
    }
    const currentWindows = windows();
    const ui = [];
    for (const win of currentWindows) {
      const role = Object.entries(task.windows ?? {}).find(([, entry]) => entry === win)?.[0];
      input.__check = "snapshot-window-role";
      requireState(role);
      const nodes = win.document.querySelectorAll("#appMenu-floorp-hub-button");
      input.__check = "snapshot-translated-label-" + role;
      if (nodes.length) await waitFor(() => nodes[0].getAttribute("label")?.trim(), 5000, "translated lifecycle label");
        const node = nodes[0] ?? null;
        const settings = win.document.getElementById("appMenu-settings-button");
        ui.push({ role, private: PrivateBrowsingUtils.isWindowPrivate(win), count: nodes.length, nodeId: nodeId(task, node), enabled: node ? !node.disabled : null, label: node?.getAttribute("label") ?? null, afterSettings: !!node && node.parentElement === settings?.parentElement && node.previousElementSibling === settings, privateNodeUnchanged: !PrivateBrowsingUtils.isWindowPrivate(win) || node === task.privateNode });
    }
    input.__check = "snapshot-journal";
    const root = PathUtils.join(PathUtils.profileDir, "floorp-hotfix-signed-tests-v1", input.testId, "hub-panel-menu");
    const path = PathUtils.join(root, "state.json");
    let journal = null;
    if (await IOUtils.exists(path)) {
      requireState(!new FileUtils.File(path).isSymlink());
      const info = await IOUtils.stat(path);
      requireState(info.type === "regular" && info.size <= MAX_PINNED_STATE_BYTES);
      const bytes = await IOUtils.read(path, { maxBytes: MAX_PINNED_STATE_BYTES + 1 });
      requireState(bytes.length === info.size);
      journal = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(bytes));
    }
    input.__check = "snapshot-cache";
    const cache = await snapshotDirectory(PathUtils.join(PathUtils.profileDir, "floorp-signed-package-cache-v1", "tests", input.testId, "hub-panel-menu"));
    const journalSnapshot = await snapshotDirectory(root);
    return { profileDir: PathUtils.profileDir, ui, registry: registry.diagnosticsForTests, registryOwner: registry.diagnosticsForTests.published ? task.registryOwners?.at(-1) ?? null : null, registryOwners: task.registryOwners ?? [], diagnostics: host ? host.diagnostics() : null, metadata: host ? host.metadata() : [], journal, cache, journalSnapshot, evaluationCount: hostModule.m0PayloadEvaluationsForTests(), canonicalRootExists: await IOUtils.exists(PathUtils.join(PathUtils.profileDir, "floorp-feature-hub-panel-menu")), cleanup: host ? host.cleanupDiagnosticsForTests() : null };
  }
`;
