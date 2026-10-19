// SPDX-License-Identifier: MPL-2.0
(async () => {
  const report = { passed: false, cases: {} };
  let tab;
  const check = (name, value) => {
    report.cases[name] = !!value;
    if (!value) throw new Error(name);
  };
  const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  try {
    tab = await chrome.tabs.create({
      url: floorpDebuggerConfig.url,
      active: true,
    });
    for (let i = 0; i < 100; i++) {
      if ((await chrome.tabs.get(tab.id)).status === "complete") break;
      await delay(30);
    }
    for (let i = 0; i < 100; i++) {
      try {
        await chrome.tabs.sendMessage(tab.id, { auxiliaryDeclarative: true });
        break;
      } catch (error) {
        if (i === 99) throw error;
        await delay(30);
      }
    }
    await chrome.storage.session.set({ contractValue: "native session value" });
    report.beforeAccess = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
    });
    check(
      "sessionDefaultTrusted",
      report.beforeAccess.sessionType === "undefined" ||
        !!report.beforeAccess.error,
    );
    await chrome.storage.session.setAccessLevel({
      accessLevel: "TRUSTED_AND_UNTRUSTED_CONTEXTS",
    });
    report.afterAccess = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
    });
    check(
      "sessionContentAccessGranted",
      report.afterAccess.value?.contractValue === "native session value",
    );
    await chrome.storage.session.setAccessLevel({
      accessLevel: "TRUSTED_CONTEXTS",
    });
    report.afterRevoke = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
    });
    check(
      "sessionContentAccessRevoked",
      report.afterRevoke.sessionType === "undefined" ||
        !!report.afterRevoke.error,
    );
    await chrome.storage.local.set({ contractValue: "native local value" });
    await chrome.storage.local.setAccessLevel({
      accessLevel: "TRUSTED_CONTEXTS",
    });
    const localDenied = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
      area: "local",
    });
    check("localContentAccessDenied", !!localDenied.error);
    await chrome.storage.local.setAccessLevel({
      accessLevel: "TRUSTED_AND_UNTRUSTED_CONTEXTS",
    });
    const localGranted = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
      area: "local",
    });
    check(
      "localContentAccessGranted",
      localGranted.value?.contractValue === "native local value",
    );
    const untrustedSetter = await chrome.tabs.sendMessage(tab.id, {
      auxiliaryProbe: true,
      area: "local",
      setAccessLevel: true,
    });
    check("untrustedAccessSetterRejected", !!untrustedSetter.error);
    const settings = await chrome.action.getUserSettings();
    check("nativeActionSettings", typeof settings.isOnToolbar === "boolean");
    await chrome.action.setBadgeText({ text: "native" });
    check(
      "nativeActionBadge",
      await chrome.action.getBadgeText({}) === "native",
    );
    check(
      "notificationPermission",
      await chrome.notifications.getPermissionLevel() === "granted",
    );
    const notificationId = "authored-native-contract";
    const icon =
      "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==";
    await chrome.notifications.create(notificationId, {
      type: "basic",
      title: "Authored native notification",
      message: "Before native update",
      iconUrl: icon,
      requireInteraction: true,
      silent: true,
      buttons: [{ title: "Native action" }],
    });
    check(
      "notificationTracked",
      (await chrome.notifications.getAll())[notificationId] === true,
    );
    check(
      "notificationUpdated",
      await chrome.notifications.update(notificationId, {
        message: "After native update",
      }) === true,
    );
    check(
      "notificationUnknownUpdate",
      await chrome.notifications.update("missing-native-contract", {
        message: "missing",
      }) === false,
    );
    check(
      "notificationStillTracked",
      (await chrome.notifications.getAll())[notificationId] === true,
    );
    check(
      "notificationCleared",
      await chrome.notifications.clear(notificationId) === true,
    );
    check(
      "notificationClearMissing",
      await chrome.notifications.clear(notificationId) === false,
    );
    const dc = chrome.declarativeContent;
    const matcher = new dc.PageStateMatcher({
      pageUrl: { hostEquals: "127.0.0.1" },
      css: ["#item"],
    });
    check(
      "declarativeConstructors",
      matcher instanceof dc.PageStateMatcher &&
        matcher.instanceType === "declarativeContent.PageStateMatcher" &&
        new dc.ShowPageAction().instanceType ===
          "declarativeContent.ShowAction",
    );
    const iconAction = new dc.SetIcon({
      imageData: { 19: new ImageData(19, 19), 38: new ImageData(38, 38) },
    });
    check(
      "declarativeNativeImageData",
      iconAction.instanceType === "declarativeContent.SetIcon" &&
        !(iconAction instanceof dc.SetIcon),
    );
    let rejectedNoNew = false;
    try {
      dc.ShowAction();
    } catch {
      rejectedNoNew = true;
    }
    check("declarativeRequiresNew", rejectedNoNew);
    const rulesCall = (method, ...args) =>
      new Promise((resolve, reject) => {
        dc.onPageChanged[method](...args, (result) => {
          const error = chrome.runtime.lastError;
          if (error) reject(new Error(error.message));
          else resolve(result);
        });
      });
    await rulesCall("removeRules");
    await chrome.action.disable(tab.id);
    const added = await rulesCall("addRules", [{
      id: "authored-rule",
      conditions: [matcher],
      actions: [
        new dc.ShowAction(),
        iconAction,
        new dc.RequestContentScript({ js: ["declarative-script.js"] }),
      ],
    }]);
    check(
      "declarativeRulesStored",
      added[0].id === "authored-rule" &&
        (await rulesCall("getRules"))[0].id === "authored-rule",
    );
    let injection;
    for (let i = 0; i < 100; i++) {
      injection = await chrome.tabs.sendMessage(tab.id, {
        auxiliaryDeclarative: true,
      });
      if (injection.injected === "1") break;
      await delay(30);
    }
    report.declarativeEnabled = await chrome.action.isEnabled(tab.id);
    check("declarativeRealScriptInjection", injection.injected === "1");
    await delay(100);
    check(
      "declarativeInjectsOnce",
      (await chrome.tabs.sendMessage(tab.id, { auxiliaryDeclarative: true }))
        .injected === "1",
    );
    await chrome.tabs.sendMessage(tab.id, {
      auxiliaryDeclarative: true,
      removeMatch: true,
    });
    await delay(100);
    report.declarativeUnmatchedEnabled = await chrome.action.isEnabled(tab.id);
    await rulesCall("removeRules", ["authored-rule"]);
    check(
      "declarativeRulesRemoved",
      (await rulesCall("getRules")).length === 0,
    );
    await chrome.action.enable(tab.id);
    await chrome.windows.update(tab.windowId, { focused: true });
    await chrome.action.openPopup();
    check("nativePopupWithoutGesture", true);
    report.passed = true;
  } catch (error) {
    report.error = String(error);
    report.stack = error.stack;
  } finally {
    if (tab) await chrome.tabs.remove(tab.id);
    globalThis.floorpReferenceReport = report;
    document.title = JSON.stringify(report);
  }
})();
