/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

const { WebRequest } = ChromeUtils.importESModule(
  "resource://gre/modules/WebRequest.sys.mjs",
);

// Mobile User-Agent string for Android
const MOBILE_UA =
  "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";

// Store target request IDs with timeout management
let targetRequestIds: string[] = [];

// Timeout duration in milliseconds (180 seconds)
const TIMEOUT_DURATION = 180 * 1000;

/**
 * Listener for onBeforeRequest event
 */
function onBeforeRequestListener(
  e: Event & { bmsUseragent: boolean; requestId: string },
) {
  // Add requestId to target list
  targetRequestIds.push(e.requestId);

  // Remove requestId after timeout
  setTimeout(() => {
    targetRequestIds = targetRequestIds.filter((id) => id !== e.requestId);
  }, TIMEOUT_DURATION);
}

/**
 * Listener for onBeforeSendHeaders event
 * Modifies User-Agent header for target requests
 */
function onBeforeSendHeadersListener(
  e: Event & {
    requestId: string;
    requestHeaders: Array<{ name: string; value: string }>;
  },
) {
  if (targetRequestIds.includes(e.requestId)) {
    // Remove requestId from target list
    targetRequestIds = targetRequestIds.filter((id) => id !== e.requestId);

    // Modify User-Agent header
    e.requestHeaders.forEach((header) => {
      console.log(header.name.toLowerCase());
      if (header.name.toLowerCase() === "user-agent") {
        header.value = MOBILE_UA;
      }
    });

    return { requestHeaders: e.requestHeaders };
  }
}

// Register listeners
WebRequest.onBeforeRequest.addListener(onBeforeRequestListener, null, [
  "blocking",
]);

WebRequest.onBeforeSendHeaders.addListener(
  onBeforeSendHeadersListener,
  { patterns: ["*://*/*"], matchesAllWebUrls: false },
  ["blocking", "requestHeaders"],
);
