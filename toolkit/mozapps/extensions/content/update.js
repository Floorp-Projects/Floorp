"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

let BRAND_PROPS = "chrome://branding/locale/brand.properties";
let UPDATE_PROPS = "chrome://mozapps/locale/extensions/update.properties";

let appName = Services.strings.createBundle(BRAND_PROPS)
                      .GetStringFromName("brandShortName");
let bundle = Services.strings.createBundle(UPDATE_PROPS);

let titleText = bundle.formatStringFromName("addonUpdateTitle", [appName], 1);
let messageText = bundle.formatStringFromName("addonUpdateMessage", [appName], 1);
let cancelText = bundle.GetStringFromName("addonUpdateCancelMessage");
let cancelButtonText = bundle.formatStringFromName("addonUpdateCancelButton", [appName], 1);

document.title = titleText;

window.addEventListener("load", e => {
  document.getElementById("message").textContent = messageText;
  document.getElementById("cancel-message").textContent = cancelText;
  document.getElementById("cancel-btn").textContent = cancelButtonText;
  window.sizeToContent();
});
