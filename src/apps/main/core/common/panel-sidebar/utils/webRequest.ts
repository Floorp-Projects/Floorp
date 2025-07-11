/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

// Mobile User-Agent string for Android
const MOBILE_UA =
  "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36 Edg/114.0.1823.79";

/**
 * Observer for http-on-modify-request
 * Modifies User-Agent header for all HTTP requests
 */
const httpRequestObserver = {
  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe: (channel: any, topic: string) => {
    console.log("channel", globalThis.floorpBmsUserAgent);
    if (!globalThis.floorpBmsUserAgent) {
      return;
    }

    if (
      topic !== "http-on-modify-request" ||
      !(channel instanceof Ci.nsIHttpChannel)
    ) {
      return;
    }

    try {
      channel.setRequestHeader("User-Agent", MOBILE_UA, false);
    } catch (error) {
      console.error("Failed to set User-Agent:", error);
    }
  },
};

// Register observer
Services.obs.addObserver(httpRequestObserver, "http-on-modify-request", false);
