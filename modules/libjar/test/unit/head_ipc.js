/*
 * If we're running in e10s, determines whether we're in child directory or
 * not.
 */

var inChild = false;
var filePrefix = "";
try {
  inChild = Components.classes["@mozilla.org/xre/runtime;1"].
              getService(Components.interfaces.nsIXULRuntime).processType
              != Components.interfaces.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
  if (inChild) {
    // use "jar:remoteopenfile://" in child instead of "jar:file://"
    filePrefix = "remoteopen";
  }
} 
catch (e) { }
