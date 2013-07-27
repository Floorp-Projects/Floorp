/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let outOfProcess = __marionetteParams[0]
let mochitestUrl = __marionetteParams[1]

const CHILD_SCRIPT = "chrome://specialpowers/content/specialpowers.js";
const CHILD_SCRIPT_API = "chrome://specialpowers/content/specialpowersAPI.js";
const CHILD_LOGGER_SCRIPT = "chrome://specialpowers/content/MozillaLogger.js";

let homescreen = document.getElementById('homescreen');
let container = homescreen.contentWindow.document.getElementById('test-container');

function openWindow(aEvent) {
  var popupIframe = aEvent.detail.frameElement;
  popupIframe.setAttribute('style', 'position: absolute; left: 0; top: 300px; background: white; ');

  popupIframe.addEventListener('mozbrowserclose', function(e) {
    container.parentNode.removeChild(popupIframe);
    container.focus();
  });

  // yes, the popup can call window.open too!
  popupIframe.addEventListener('mozbrowseropenwindow', openWindow);

  popupIframe.addEventListener('mozbrowserloadstart', function(e) {
    popupIframe.focus();
  });

  container.parentNode.appendChild(popupIframe);
}
container.addEventListener('mozbrowseropenwindow', openWindow);

let specialpowers = {};
let loader = Cc["@mozilla.org/moz/jssubscript-loader;1"].getService(Ci.mozIJSSubScriptLoader);
loader.loadSubScript("chrome://specialpowers/content/SpecialPowersObserver.js", specialpowers);
let specialPowersObserver = new specialpowers.SpecialPowersObserver();
specialPowersObserver.init();

let mm = container.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
mm.addMessageListener("SPPrefService", specialPowersObserver);
mm.addMessageListener("SPProcessCrashService", specialPowersObserver);
mm.addMessageListener("SPPingService", specialPowersObserver);
mm.addMessageListener("SpecialPowers.Quit", specialPowersObserver);
mm.addMessageListener("SpecialPowers.Focus", specialPowersObserver);
mm.addMessageListener("SPPermissionManager", specialPowersObserver);

mm.loadFrameScript(CHILD_LOGGER_SCRIPT, true);
mm.loadFrameScript(CHILD_SCRIPT_API, true);
mm.loadFrameScript(CHILD_SCRIPT, true);
specialPowersObserver._isFrameScriptLoaded = true;

container.src = mochitestUrl;
