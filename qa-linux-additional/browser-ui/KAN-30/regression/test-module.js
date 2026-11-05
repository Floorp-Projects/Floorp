// qa:config-adapter
import { c as config } from "chrome://noraneko/content/assets/js/configs.js";
function setConfig(value) {
  Services.prefs.setStringPref("floorp.design.configs", JSON.stringify(value));
}
function updateChromeExtrasSetting(key, value) {
  const next = structuredClone(config());
  next.uiCustomization.chromeExtras[key] = value;
  setConfig(next);
}

// qa-linux-additional/browser-ui/KAN-30/regression/entry.ts
import { firefoxSidebarOverlay } from "chrome://noraneko/content/assets/js/index9.js";

// floorp-additional-ui/browser-features/chrome/test/utils/test_harness.ts
function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}
async function runTests(moduleName, tests) {
  const failures = [];
  for (const [index, test] of tests.entries()) {
    globalThis.__NORA_TEST_PROGRESS__ = {
      moduleName,
      testName: test.name,
      status: "running",
      index: index + 1,
      total: tests.length,
      startedAtMs: Date.now()
    };
    try {
      await test.fn();
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "passed",
        index: index + 1,
        total: tests.length,
        startedAtMs
      };
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
      const startedAtMs = globalThis.__NORA_TEST_PROGRESS__?.startedAtMs ?? Date.now();
      globalThis.__NORA_TEST_PROGRESS__ = {
        moduleName,
        testName: test.name,
        status: "failed",
        index: index + 1,
        total: tests.length,
        startedAtMs
      };
    }
  }
  globalThis.__NORA_TEST_PROGRESS__ = {
    moduleName,
    testName: "",
    status: "done",
    index: tests.length,
    total: tests.length,
    startedAtMs: Date.now()
  };
  if (failures.length > 0) {
    throw new Error(`${moduleName} failures: ${failures.join(" | ")}`);
  }
}

// qa-linux-additional/browser-ui/KAN-30/regression/entry.ts
var native = globalThis.SidebarController;
async function waitFor(check, message) {
  const deadline = Date.now() + 1e4;
  while (!check()) {
    assert(Date.now() < deadline, message);
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}
async function settle() {
  await native.waitUntilStable();
  for (let i = 0; i < 3; i++) {
    await new Promise(
      (resolve) => requestAnimationFrame(() => resolve())
    );
  }
}
function element(id) {
  const result = document.getElementById(id);
  assert(result, `${id} must exist`);
  return result;
}
function hover(inside) {
  element("floorp-firefox-sidebar-edge-toggle").dispatchEvent(
    new MouseEvent(inside ? "mouseenter" : "mouseleave")
  );
}
function focusContent() {
  globalThis.gBrowser.selectedBrowser.focus();
}
async function setModes(overlay, hoverMode) {
  updateChromeExtrasSetting("sidebarOverlap", overlay);
  updateChromeExtrasSetting("autohideSidebar", hoverMode);
  await waitFor(
    () => document.documentElement.hasAttribute(
      "floorp-firefox-sidebar-overlay"
    ) === overlay && document.documentElement.hasAttribute("floorp-firefox-sidebar-hover") === (overlay && hoverMode),
    "sidebar settings must reach chrome"
  );
  await settle();
}
async function withSidebar(test) {
  await native.promiseInitialized;
  await waitFor(
    () => Boolean(firefoxSidebarOverlay),
    "Firefox sidebar feature must initialize"
  );
  const before = config();
  const originalID = native.currentID;
  const originalLast = native.lastOpenedId;
  const wasOpen = native.isOpen;
  const originalWidth = element("sidebar-box").style.width;
  const prefs = ["sidebar.position_start", "sidebar.visibility"];
  const savedPrefs = prefs.map((name) => ({
    name,
    user: Services.prefs.prefHasUserValue(name),
    value: name === "sidebar.position_start" ? Services.prefs.getBoolPref(name, true) : Services.prefs.getStringPref(name, "always-show")
  }));
  try {
    await setModes(false, false);
    native.hide({ dismissPanel: false });
    await settle();
    await test();
  } finally {
    hover(false);
    focusContent();
    document.documentElement.removeAttribute("inDOMFullscreen");
    native.hide({ dismissPanel: false });
    setConfig(before);
    for (const pref of savedPrefs) {
      if (!pref.user) Services.prefs.clearUserPref(pref.name);
      else if (typeof pref.value === "boolean") {
        Services.prefs.setBoolPref(pref.name, pref.value);
      } else Services.prefs.setStringPref(pref.name, pref.value);
    }
    native.setPosition();
    element("sidebar-box").style.width = originalWidth;
    native.lastOpenedId = originalLast;
    await settle();
    if (wasOpen && native.sidebars.has(originalID)) {
      await native.showInitially(originalID);
    }
    await settle();
  }
}
async function testWindowActivationHoverLifecycle() {
  await withSidebar(async () => {
    native.lastOpenedId = "viewBookmarksSidebar";
    await setModes(true, true);
    focusContent();
    hover(true);
    await waitFor(
      () => native.isOpen && Boolean(firefoxSidebarOverlay?.expanded),
      "hover must open the native sidebar before changing focus"
    );
    await settle();
    window.dispatchEvent(new FocusEvent("blur"));
    await new Promise((resolve) => setTimeout(resolve, 450));
    assert(
      native.isOpen && firefoxSidebarOverlay?.expanded,
      "internal window blur must preserve a hover-owned sidebar"
    );
    window.dispatchEvent(new Event("deactivate"));
    await waitFor(
      () => !firefoxSidebarOverlay?.expanded,
      "window deactivation must close the hover preview"
    );
    hover(false);
    hover(true);
    await waitFor(
      () => native.isOpen && Boolean(firefoxSidebarOverlay?.expanded),
      "a new hover must reopen after window deactivation"
    );
    hover(false);
    focusContent();
    await waitFor(
      () => !firefoxSidebarOverlay?.expanded,
      "pointer leave must still close the reopened preview"
    );
  });
}
window.qaKan30Run = () => runTests("sidebarOverlay.test.ts", [{ name: "window activation hover lifecycle", fn: testWindowActivationHoverLifecycle }]);
