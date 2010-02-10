// This code is TEMPORARY for submitting crashes via an ugly popup dialog:
// bug 525849 tracks the real implementation.

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/CrashSubmit.jsm");

var id;

function collectData() {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties);

  let dumpFile = window.arguments[0].QueryInterface(Ci.nsIFile);
  id = dumpFile.leafName.replace(/.dmp$/, "");
}

function submitDone()
{
  // we don't currently distinguish between success or failure here
  window.close();
}

function onSubmit()
{
  document.documentElement.getButton('accept').disabled = true;
  document.documentElement.getButton('accept').label = 'Sending';
  document.getElementById('throbber').src = 'chrome://global/skin/icons/loading_16.png';
  CrashSubmit.submit(id, document.getElementById('iframe-holder'),
                     submitDone, submitDone);
  return false;
}
