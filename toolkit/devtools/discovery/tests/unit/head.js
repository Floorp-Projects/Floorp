"use strict";
const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// We also need a valid nsIXulAppInfo
Cu.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
