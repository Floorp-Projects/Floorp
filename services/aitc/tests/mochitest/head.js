/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tmp = {};
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://services-aitc/browserid.js", tmp);

const BrowserID = tmp.BrowserID;
const testPath = "http://mochi.test:8888/browser/services/aitc/tests/";

function loadURL(aURL, aCB) {
  gBrowser.selectedBrowser.addEventListener("load", function () {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    is(gBrowser.currentURI.spec, aURL, "loaded expected URL");
    aCB();
  }, true);
  gBrowser.loadURI(aURL);
}

function setEndpoint(name) {
  let fullPath = testPath + "file_" + name + ".html";
  Services.prefs.setCharPref("services.aitc.browserid.url", fullPath);
}