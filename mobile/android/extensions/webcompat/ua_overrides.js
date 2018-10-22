/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */
const UAOverrides = {
  universal: [
    /*
     * This is a dummy override that applies a Chrome UA to a dummy site that
     * blocks all browsers but Chrome.
     *
     * This was only put in place to allow QA to test this system addon on an
     * actual site, since we were not able to find a proper override in time.
     */
    {
      matches: ["*://webcompat-addon-testcases.schub.io/*"],
      uaTransformer: (originalUA) => {
        let prefix = originalUA.substr(0, originalUA.indexOf(")") + 1);
        return `${prefix} AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36`;
      },
    },
  ],
  desktop: [],
  android: [
    /*
     * Bug 1480710 - m.imgur.com - Build UA override
     * WebCompat issue #13154 - https://webcompat.com/issues/13154
     *
     * imgur returns a 404 for requests to CSS and JS file if requested with a Fennec
     * User Agent. By removing the Fennec identifies and adding Chrome Mobile's, we
     * receive the correct CSS and JS files.
     */
    {
      matches: ["*://m.imgur.com/*"],
      uaTransformer: (originalUA) => {
        let prefix = originalUA.substr(0, originalUA.indexOf(")") + 1);
        return prefix + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.85 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 755590 - sites.google.com - top bar doesn't show up in Firefox for Android
     *
     * Google Sites does show a different top bar template based on the User Agent.
     * For Fennec, this results in a broken top bar. Appending Chrome and Mobile Safari
     * identifiers to the UA results in a correct rendering.
     */
    {
      matches: ["*://sites.google.com/*"],
      uaTransformer: (originalUA) => {
        return originalUA + " Chrome/68.0.3440.85 Mobile Safari/537.366";
      },
    },

    /*
     * Bug 945963 - tieba.baidu.com serves simplified mobile content to Firefox Android
     * WebCompat issue #18455 - https://webcompat.com/issues/18455
     *
     * tieba.baidu.com and tiebac.baidu.com serve a heavily simplified and less functional
     * mobile experience to Firefox for Android users. Adding the AppleWebKit indicator
     * to the User Agent gets us the same experience.
     */
    {
      matches: ["*://tieba.baidu.com/*", "*://tiebac.baidu.com/*"],
      uaTransformer: (originalUA) => {
        return originalUA + " AppleWebKit/537.36 (KHTML, like Gecko)";
      },
    },
  ],
};

/* globals browser */

let activeListeners = [];
function buildAndRegisterListener(matches, transformer) {
  let listener = (details) => {
    for (var header of details.requestHeaders) {
      if (header.name.toLowerCase() === "user-agent") {
        header.value = transformer(header.value);
      }
    }
    return {requestHeaders: details.requestHeaders};
  };

  browser.webRequest.onBeforeSendHeaders.addListener(
    listener,
    {urls: matches},
    ["blocking", "requestHeaders"]
  );

  activeListeners.push(listener);
}

async function registerUAOverrides() {
  let platform = "desktop";
  let platformInfo = await browser.runtime.getPlatformInfo();
  if (platformInfo.os == "android") {
    platform = "android";
  }

  let targetOverrides = UAOverrides.universal.concat(UAOverrides[platform]);
  targetOverrides.forEach((override) => {
    buildAndRegisterListener(override.matches, override.uaTransformer);
  });
}

function unregisterUAOverrides() {
  activeListeners.forEach((listener) => {
    browser.webRequest.onBeforeSendHeaders.removeListener(listener);
  });

  activeListeners = [];
}

const OVERRIDE_PREF = "perform_ua_overrides";
function checkOverridePref() {
  browser.aboutConfigPrefs.getPref(OVERRIDE_PREF).then(value => {
    if (value === undefined) {
      browser.aboutConfigPrefs.setPref(OVERRIDE_PREF, true);
    } else if (value === false) {
      unregisterUAOverrides();
    } else {
      registerUAOverrides();
    }
  });
}
browser.aboutConfigPrefs.onPrefChange.addListener(checkOverridePref, OVERRIDE_PREF);
checkOverridePref();
