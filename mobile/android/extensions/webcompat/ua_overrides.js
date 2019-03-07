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
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/54.0.2840.98 Safari/537.36";
      },
    },
  ],
  desktop: [
    /*
     * Bug 1464106 - directvnow.com - Create a UA override for Directvnow.com for playback on desktop
     * WebCompat issue #3846 - https://webcompat.com/issues/3846
     *
     * directvnow.com is blocking Firefox via UA sniffing. Outreach is still going
     * on, and playback works fine if we spoof as Chrome.
     */
    {
      matches: ["*://*.directvnow.com/*"],
      uaTransformer: (originalUA) => {
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.110 Safari/537.36";
      },
    },
  ],
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
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.85 Mobile Safari/537.36";
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

    /*
     * Bug 1518625 - rottentomatoes.com - Add UA override for videos on www.rottentomatoes.com
     *
     * The video framework loaded in via pdk.theplatform.com fails to
     * acknowledge that Firefox does support HLS, so it fails to find a
     * supported video format and shows the loading bar forever. Spoofing as
     * Chrome works.
     *
     * Contrary to other PDK sites, rottentomatoes sometimes uses an iFrame to
     * player.theplatform.com to show a video, so we need to override that domain
     * as well.
     */
    {
      matches: [
        "*://*.rottentomatoes.com/*",
        "*://player.theplatform.com/*",
      ],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    {
      matches: ["*://weather.yahoo.co.jp/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 5.0.2; Galaxy Nexus Build/IMM76B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    {
      matches: ["*://*.lohaco.jp/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 5.0.2; Galaxy Nexus Build/IMM76B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/43.0.2357.93 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    {
      matches: ["*://*.nhk.or.jp/*"],
      uaTransformer: (originalUA) => {
        return originalUA + " AppleWebKit";
      },
    },

    /*
     * Bug 1177298 - Write UA overrides for top Japanese Sites
     * (Imported from ua-update.json.in)
     *
     * To receive the proper mobile version instead of the desktop version or
     * a lower grade mobile experience, the UA is spoofed.
     */
    {
      matches: ["*://*.uniqlo.com/*"],
      uaTransformer: (originalUA) => {
        return originalUA + " Mobile Safari";
      },
    },

    /*
     * Bug 1338260 - Add UA override for directTV
     * (Imported from ua-update.json.in)
     *
     * DirectTV has issues with scrolling and cut-off images. Pretending to be
     * Chrome for Android fixes those issues.
     */
    {
      matches: ["*://*.directv.com/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1385206 - Create UA override for rakuten.co.jp on Firefox Android
     * (Imported from ua-update.json.in)
     *
     * rakuten.co.jp serves a Desktop version if Firefox is included in the UA.
     */
    {
      matches: ["*://*.rakuten.co.jp/*"],
      uaTransformer: (originalUA) => {
        return originalUA.replace(/Firefox.+$/, "");
      },
    },

    /*
     * Bug 969844 - mobile.de sends desktop site to Firefox on Android
     *
     * mobile.de sends the desktop site to Fennec. Spooing as Chrome works fine.
     */
    {
      matches: ["*://*.mobile.de/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1476436 - mobile.bet365.com - add UA override for fennec
     * WebCompat issue #17010 - https://webcompat.com/issues/17010
     *
     * mobile.bet365.com serves fennec an alternative version with less interactive
     * elements, although they work just fine. Spoofing as Chrome makes the
     * interactive elements appear.
     */
    {
      matches: ["*://mobile.bet365.com/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1509831 - cc.com - Add UA override for CC.com
     * WebCompat issue #329 - https://webcompat.com/issues/329
     *
     * ComedyCentral blocks Firefox for not being able to play HLS, which was
     * true in previous versions, but no longer is. With a spoofed Chrome UA,
     * the site works just fine.
     */
    {
      matches: ["*://*.cc.com/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1508564 - cnbc.com - Add UA override for videos on www.cnbc.com
     * WebCompat issue #8410 - https://webcompat.com/issues/8410
     *
     * The video framework loaded in via pdk.theplatform.com fails to
     * acknowledge that Firefox does support HLS, so it fails to find a
     * supported video format and shows the loading bar forever. Spoofing as
     * Chrome works.
     */
    {
      matches: ["*://*.cnbc.com/*"],
      uaTransformer: (_) => {
        return "Mozilla/5.0 (Linux; Android 6.0.1; SM-G920F Build/MMB29K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1508516 - cineflix.com.br - Add UA override for cineflix.com.br/m/
     * WebCompat issue #21553 - https://webcompat.com/issues/21553
     *
     * The site renders a blank page with any Firefox snipped in the UA as it
     * is running into an exception. Spoofing as Chrome makes the site work
     * fine.
     */
    {
      matches: ["*://*.cineflix.com.br/m/*"],
      uaTransformer: (originalUA) => {
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1509852 - redbull.com - Add UA override for redbull.com
     * WebCompat issue #21439 - https://webcompat.com/issues/21439
     *
     * Redbull.com blocks some features, for example the live video player, for
     * Fennec. Spoofing as Chrome results in us rendering the video just fine,
     * and everything else works as well.
     */
    {
      matches: ["*://*.redbull.com/*"],
      uaTransformer: (originalUA) => {
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },

    /*
     * Bug 1509873 - zmags.com - Add UA override for secure.viewer.zmags.com
     * WebCompat issue #21576 - https://webcompat.com/issues/21576
     *
     * The zmags viewer locks out Fennec with a "Browser unsupported" message,
     * but tests showed that it works just fine with a Chrome UA. Outreach
     * attempts were unsuccessful, and as the site has a relatively high rank,
     * we alter the UA.
     */
    {
      matches: ["*://*.viewer.zmags.com/*"],
      uaTransformer: (originalUA) => {
        return UAHelpers.getPrefix(originalUA) + " AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.91 Mobile Safari/537.36";
      },
    },
  ],
};

/* globals browser */

const UAHelpers = {
  getPrefix(originalUA) {
    return originalUA.substr(0, originalUA.indexOf(")") + 1);
  },
};

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
