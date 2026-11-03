// SPDX-License-Identifier: MPL-2.0
const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Reversible placement in the disposable profile, then genuine chrome-widget
// input. No parent action invocation or caller-supplied user-gesture flag.
/**
 * @param {import("./native-test-types.ts").NativeCorpusExtension} extension
 * @param {import("./native-test-types.ts").NativeCorpusWindow} window
 * @param {import("./native-test-types.ts").NativeCorpusTab} tab
 * @returns {Promise<import("./browser-tools-test-types.ts").ActionObservation>}
 */
export async function invokeNativeCorpusAction(extension, window, tab) {
  window.gBrowser.selectedTab = tab;
  window.focus();
  const api = await extension.apiManager.asyncGetAPI(
    "browserAction",
    extension,
    "addon_parent",
  );
  const widgetId = api.widget?.id;
  if (!widgetId) throw Error("Actual extension action widget missing");
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);
  let button, rect;
  const deadline = Date.now() + 10000;
  while (Date.now() < deadline) {
    button = window.document.getElementById(widgetId)?.querySelector(
      ".unified-extensions-item-action-button",
    );
    rect = button?.getBoundingClientRect();
    if (rect?.width && rect.height && Services.focus.activeWindow === window) {
      break;
    }
    await pause(100);
  }
  /** @type {import("./browser-tools-test-types.ts").ActionObservation} */
  const details = {
    widgetId,
    focused: Services.focus.activeWindow === window,
    enabled: api.action.getProperty(tab, "enabled"),
    disabled: !!(button && "disabled" in button && button.disabled) ||
      button?.getAttribute("disabled") === "true",
    rect: rect
      ? { x: rect.x, y: rect.y, width: rect.width, height: rect.height }
      : null,
    nativeEvents: [],
  };
  if (
    !details.focused || details.disabled || details.enabled === false ||
    !button || !rect?.width || !rect.height
  ) {
    throw Error(
      "Actual toolbar action unavailable: " + JSON.stringify(details),
    );
  }
  const x = rect.x + rect.width / 2, y = rect.y + rect.height / 2;
  const hit = window.document.elementFromPoint(x, y);
  if (hit !== button && !button.contains(hit)) {
    throw Error("Actual toolbar action is occluded");
  }
  /** @param {Event} event */
  const observe = (event) =>
    details.nativeEvents.push({ type: event.type, trusted: event.isTrusted });
  button.addEventListener("mousedown", observe);
  button.addEventListener("click", observe);
  try {
    window.synthesizeMouseEvent("mousedown", x, y, {
      button: 0,
      buttons: 1,
      clickCount: 1,
    });
    window.synthesizeMouseEvent("mouseup", x, y, {
      button: 0,
      buttons: 0,
      clickCount: 1,
    });
    await pause(100);
    if (
      !details.nativeEvents.some((event) =>
        event.type === "click" && event.trusted
      )
    ) {
      throw Error("Native extension action did not receive a trusted click");
    }
    return details;
  } finally {
    button.removeEventListener("mousedown", observe);
    button.removeEventListener("click", observe);
  }
}
