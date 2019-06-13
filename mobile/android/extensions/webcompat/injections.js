/**
 * For detailed information on our policies, and a documention on this format
 * and its possibilites, please check the Mozilla-Wiki at
 *
 * https://wiki.mozilla.org/Compatibility/Go_Faster_Addon/Override_Policies_and_Workflows#User_Agent_overrides
 */

"use strict";

/* globals browser, filterOverrides, Injections, portsToAboutCompatTabs */

let InjectionsEnabled = true;

for (const injection of [
  {
    id: "testinjection",
    platform: "all",
    domain: "webcompat-addon-testcases.schub.io",
    bug: "1287966",
    contentScripts: {
      matches: ["*://webcompat-addon-testcases.schub.io/*"],
      css: [{file: "injections/css/bug0000000-dummy-css-injection.css"}],
      js: [{file: "injections/js/bug0000000-dummy-js-injection.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1452707",
    platform: "desktop",
    domain: "ib.absa.co.za",
    bug: "1452707",
    contentScripts: {
      matches: ["https://ib.absa.co.za/*"],
      js: [{file: "injections/js/bug1452707-window.controllers-shim-ib.absa.co.za.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1457335",
    platform: "desktop",
    domain: "histography.io",
    bug: "1457335",
    contentScripts: {
      matches: ["*://histography.io/*"],
      js: [{file: "injections/js/bug1457335-histography.io-ua-change.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1472075",
    platform: "desktop",
    domain: "bankofamerica.com",
    bug: "1472075",
    contentScripts: {
      matches: ["*://*.bankofamerica.com/*"],
      js: [{file: "injections/js/bug1472075-bankofamerica.com-ua-change.js"}],
      runAt: "document_start",
    },
  }, {
    id: "bug1472081",
    platform: "desktop",
    domain: "election.gov.np",
    bug: "1472081",
    contentScripts: {
      matches: ["http://202.166.205.141/bbvrs/*"],
      js: [{file: "injections/js/bug1472081-election.gov.np-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
  }, {
    id: "bug1482066",
    platform: "desktop",
    domain: "portalminasnet.com",
    bug: "1482066",
    contentScripts: {
      matches: ["*://portalminasnet.com/*"],
      js: [{file: "injections/js/bug1482066-portalminasnet.com-window.sidebar-shim.js"}],
      runAt: "document_start",
      allFrames: true,
    },
  }, {
    id: "bug1526977",
    platform: "desktop",
    domain: "sreedharscce.in",
    bug: "1526977",
    contentScripts: {
      matches: ["*://*.sreedharscce.in/authenticate"],
      css: [{file: "injections/css/bug1526977-sreedharscce.in-login-fix.css"}],
    },
  }, {
    id: "bug1518781",
    platform: "desktop",
    domain: "twitch.tv",
    bug: "1518781",
    contentScripts: {
      matches: ["*://*.twitch.tv/*"],
      css: [{file: "injections/css/bug1518781-twitch.tv-webkit-scrollbar.css"}],
    },
  }, {
    id: "bug1554014",
    platform: "all",
    domain: "Sites using TinyMCE 3, version 3.4.4 and older",
    bug: "1554014",
    rewriteResponses: {
      urls: ["*://*/*tiny_mce.js", "*://*/*tiny_mce_src.js"],
      types: ["script"],
      match: /\bisGecko\s*=([^=])/,
      maxMatchLength: 9 + 4, // only consider up to 4 optional whitespaces
      replacement: "isGecko=false && $1",
      precondition: {
        match: /majorVersion\s*:\s*["'](\d+)["']\s*,\s*minorVersion\s*:\s*["'](\d+(\.\d+)?)/,
        maxMatchLength: 37 + 24, // only consider up to 24 optional whitespaces
        checkMatch: (_, major, minor) => {
          return major == 3 && parseFloat(minor) < 4.5;
        },
      },
    },
  }, {
    id: "bug1551672",
    platform: "android",
    domain: "Sites using PDK 5 video",
    bug: "1551672",
    rewriteResponses: {
      urls: ["https://*/*/tpPdk.js", "https://*/*/pdk/js/*/*.js"],
      types: ["script"],
      match: /\bVideoContextChromeAndroid\b/,
      maxMatchLength: 25,
      replacement: "VideoContextAndroid",
    },
  }, {
    id: "bug1305028",
    platform: "desktop",
    domain: "gaming.youtube.com",
    bug: "1305028",
    contentScripts: {
      matches: ["*://gaming.youtube.com/*"],
      css: [{file: "injections/css/bug1305028-gaming.youtube.com-webkit-scrollbar.css"}],
    },
  },
]) {
  Injections.push(injection);
}

let port = browser.runtime.connect();
const ActiveInjections = new Map();

async function registerContentScripts() {
  const platformMatches = ["all"];
  let platformInfo = await browser.runtime.getPlatformInfo();
  platformMatches.push(platformInfo.os == "android" ? "android" : "desktop");

  for (const injection of Injections) {
    if (platformMatches.includes(injection.platform)) {
      injection.availableOnPlatform = true;
      await enableInjection(injection);
    }
  }

  InjectionsEnabled = true;
  portsToAboutCompatTabs.broadcast({interventionsChanged: filterOverrides(Injections)});
}

function rewriteResponses(config) {
  const {urls, types, match, maxMatchLength, replacement,
         inputEncoding = "utf-8", precondition} = config;

  if (!(match instanceof RegExp)) {
    throw new Error("Invalid match parameter; expect a regular expression");
  }
  if (maxMatchLength instanceof Number) {
    throw new Error("Invalid maxMatch parameter; expect a number");
  }
  if (precondition) {
    if (!(precondition.match instanceof RegExp)) {
      throw new Error("Invalid precondition match parameter; expect a regular expression");
    }
    if (typeof precondition.checkMatch !== "function") {
      throw new Error("Invalid precondition check parameter; expect function");
    }
  }

  // We rewrite the response as it arrives in streamed chunks, so we need to
  // buffer a certain amount of the incoming data in case the bits we're
  // interested in span across those chunks. This buffer's length depends
  // on the maxMatchLength of all regexes we will match, including the main
  // replacement and the optional precondition.
  let streamingBufferLength = maxMatchLength;
  if (precondition) {
    streamingBufferLength = Math.max(precondition.maxMatchLength, streamingBufferLength);
  }

  const listener = config.listener = ({requestId}) => {
    replaceInRequest({
      requestId,
      match,
      replacement,
      streamingBufferLength,
      inputEncoding,
      precondition,
    });
    return {};
  };
  browser.webRequest.onBeforeRequest.addListener(listener, {urls, types}, ["blocking"]);
}

function replaceInRequest({requestId, streamingBufferLength, match, replacement, inputEncoding = "utf-8", precondition}) {
  const filter = browser.webRequest.filterResponseData(requestId);
  const decoder = new TextDecoder(inputEncoding);
  const encoder = new TextEncoder();
  // As the incoming data will be streamed, we must buffer some of it in case
  // the matches we are interested in span across two streamed chunks.
  let bufferedCarryover = "";
  // If a precondition is required, then we will not actually replace anything
  // until we've seen that precondition in the incoming data.
  let preconditionMet = !precondition;
  filter.ondata = event => {
    let chunk = bufferedCarryover + decoder.decode(event.data, {stream: true});
    if (!preconditionMet) {
      const matches = chunk.match(precondition.match);
      if (matches) {
        preconditionMet = !precondition.checkMatch ||
                          precondition.checkMatch.apply(null, matches);
      }
    }
    if (preconditionMet) {
      chunk = chunk.replace(match, replacement);
    }
    filter.write(encoder.encode(chunk.slice(0, -streamingBufferLength)));
    bufferedCarryover = chunk.slice(-streamingBufferLength);
  };
  filter.onstop = event => {
    if (bufferedCarryover.length) {
      filter.write(encoder.encode(bufferedCarryover));
    }
    filter.close();
  };
}

async function enableInjection(injection) {
  if (injection.active) {
    return;
  }

  if ("rewriteResponses" in injection) {
    rewriteResponses(injection.rewriteResponses);
    injection.active = true;
    return;
  }

  try {
    const handle = await browser.contentScripts.register(injection.contentScripts);
    ActiveInjections.set(injection, handle);
    injection.active = true;
  } catch (ex) {
    console.error("Registering WebCompat GoFaster content scripts failed: ", ex);
  }
}

function unregisterContentScripts() {
  for (const injection of Injections) {
    disableInjection(injection);
  }
  InjectionsEnabled = false;
  portsToAboutCompatTabs.broadcast({interventionsChanged: false});
}

async function disableInjection(injection) {
  if (!injection.active) {
    return;
  }

  if ("rewriteResponses" in injection) {
    const {listener} = injection.rewriteResponses;
    browser.webRequest.onBeforeRequest.removeListener(listener);
    injection.active = false;
    delete injection.rewriteResponses.listener;
    return;
  }

  const contentScript = ActiveInjections.get(injection);
  await contentScript.unregister();
  ActiveInjections.delete(injection);
  injection.active = false;
}

port.onMessage.addListener((message) => {
  switch (message.type) {
    case "injection-pref-changed":
      if (message.prefState) {
        registerContentScripts();
      } else {
        unregisterContentScripts();
      }
      break;
  }
});

const INJECTION_PREF = "perform_injections";
function checkInjectionPref() {
  browser.aboutConfigPrefs.getPref(INJECTION_PREF).then(value => {
    if (value === undefined) {
      browser.aboutConfigPrefs.setPref(INJECTION_PREF, true);
    } else if (value === false) {
      unregisterContentScripts();
    } else {
      registerContentScripts();
    }
  });
}
browser.aboutConfigPrefs.onPrefChange.addListener(checkInjectionPref, INJECTION_PREF);
checkInjectionPref();
