// This code is TEMPORARY for submitting crashes via an ugly popup dialog:
// bug 525849 tracks the real implementation.

var id;

function getExtraData() {
  let appData = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime);
  appData.QueryInterface(Ci.nsICrashReporter);
  appData.QueryInterface(Ci.nsIXULAppInfo);

  let r = "";
  r += "ServerURL=" + appData.serverURL.spec + "\n";
  r += "BuildID=" + appData.appBuildID + "\n";
  r += "ProductName=" + appData.name + "\n";
  r += "Vendor=" + appData.vendor + "\n";
  r += "Version=" + appData.version + "\n";
  r += "CrashTime=" + ((new Date()).getTime() / 1000).toFixed() + "\n";
  r += "ProcessType=plugin\n";
  return r;
}

function collectData() {
  // HACK: crashes.js uses document.body, so we just alias it
  document.body = document.getElementById('iframe-holder');

  getL10nStrings();

  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIProperties);
  pendingDir = directoryService.get("UAppData", Ci.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");
  if (!pendingDir.exists())
    pendingDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0770);

  reportsDir = directoryService.get("UAppData", Ci.nsIFile);
  reportsDir.append("Crash Reports");
  reportsDir.append("submitted");
  if (!reportsDir.exists())
      reportsDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0770);

  let dumpFile = window.arguments[0].QueryInterface(Ci.nsIFile);
  dumpFile.moveTo(pendingDir, "");
  let leafName = dumpFile.leafName;

  id = /([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.dmp/(leafName)[1];

  dumpFile = pendingDir.clone();
  dumpFile.append(leafName);

  let extraFile = pendingDir.clone();
  extraFile.append(id + ".extra");

  let fstream = Cc["@mozilla.org/network/file-output-stream;1"].
    createInstance(Ci.nsIFileOutputStream);
  fstream.init(extraFile, -1, -1, 0);

  var os = Cc["@mozilla.org/intl/converter-output-stream;1"].
    createInstance(Ci.nsIConverterOutputStream);
  os.init(fstream, "UTF-8", 0, 0x0000);
  os.writeString(getExtraData());
  os.close();
  fstream.close();
}

function onSubmit()
{
  document.documentElement.getButton('accept').disabled = true;
  document.documentElement.getButton('accept').label = 'Sending';
  document.getElementById('throbber').src = 'chrome://global/skin/icons/loading_16.png';
  createAndSubmitForm(id, null);
  return false;
}
