const Cc = Components.classes;
const Ci = Components.interfaces;

var reportURL = null;
var reportsDir, pendingDir;
var strings = null;
var myListener = null;

function parseKeyValuePairs(text) {
  var lines = text.split('\n');
  var data = {};
  for (let i = 0; i < lines.length; i++) {
    if (lines[i] == '')
      continue;

    // can't just .split() because the value might contain = characters
    let eq = lines[i].indexOf('=');
    if (eq != -1) {
      let [key, value] = [lines[i].substring(0, eq),
                          lines[i].substring(eq + 1)];
      if (key && value)
        data[key] = value.replace("\\n", "\n", "g").replace("\\\\", "\\", "g");
    }
  }
  return data;
}

function parseKeyValuePairsFromFile(file) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  var is = Cc["@mozilla.org/intl/converter-input-stream;1"].
           createInstance(Ci.nsIConverterInputStream);
  is.init(fstream, "UTF-8", 1024, Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
  var str = {};
  var contents = '';
  while (is.readString(4096, str) != 0) {
    contents += str.value;
  }
  is.close();
  fstream.close();
  return parseKeyValuePairs(contents);
}

function parseINIStrings(file) {
  var factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(Ci.nsIINIParserFactory);
  var parser = factory.createINIParser(file);
  var obj = {};
  var en = parser.getKeys("Strings");
  while (en.hasMore()) {
    var key = en.getNext();
    obj[key] = parser.getString("Strings", key);
  }
  return obj;
}

// Since we're basically re-implementing part of the crashreporter
// client here, we'll just steal the strings we need from crashreporter.ini
function getL10nStrings() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let path = dirSvc.get("GreD", Ci.nsIFile);
  path.append("crashreporter.ini");
  if (!path.exists()) {
    // see if we're on a mac
    path = path.parent;
    path.append("crashreporter.app");
    path.append("Contents");
    path.append("MacOS");
    path.append("crashreporter.ini");
    if (!path.exists()) {
      // very bad, but I don't know how to recover
      return;
    }
  }
  let crstrings = parseINIStrings(path);
  strings = {
    'crashid': crstrings.CrashID,
    'reporturl': crstrings.CrashDetailsURL
  };

  path = dirSvc.get("XCurProcD", Ci.nsIFile);
  path.append("crashreporter-override.ini");
  if (path.exists()) {
    crstrings = parseINIStrings(path);
    if ('CrashID' in crstrings)
      strings['crashid'] = crstrings.CrashID;
    if ('CrashDetailsURL' in crstrings)
      strings['reporturl'] = crstrings.CrashDetailsURL;
  }
}

function getPendingMinidump(id) {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
                         getService(Ci.nsIProperties);
  let dump = pendingDir.clone();
  let extra = pendingDir.clone();
  dump.append(id + ".dmp");
  extra.append(id + ".extra");
  return [dump, extra];
}

function addFormEntry(doc, form, name, value) {
  var input = doc.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  form.appendChild(input);
}

function writeSubmittedReport(crashID, viewURL) {
  let reportFile = reportsDir.clone();
  reportFile.append(crashID + ".txt");
  var fstream = Cc["@mozilla.org/network/file-output-stream;1"].
                createInstance(Ci.nsIFileOutputStream);
  // open, write, truncate
  fstream.init(reportFile, -1, -1, 0);
  var os = Cc["@mozilla.org/intl/converter-output-stream;1"].
           createInstance(Ci.nsIConverterOutputStream);
  os.init(fstream, "UTF-8", 0, 0x0000);

  var data = strings.crashid.replace("%s", crashID);
  if (viewURL)
     data += "\n" + strings.reporturl.replace("%s", viewURL);

  os.writeString(data);
  os.close();
  fstream.close();
}

function submitSuccess(ret, link, dump, extra) {
  if (!ret.CrashID)
    return;
  // Write out the details file to submitted/
  writeSubmittedReport(ret.CrashID, ret.ViewURL);

  // Delete from pending dir
  try {
    dump.remove(false);
    extra.remove(false);
  }
  catch (ex) {
    // report an error? not much the user can do here.
  }

  // reset the link to point at our new crash report. this way, if the
  // user clicks "Back", the link will be correct.
  let CrashID = ret.CrashID;
  link.firstChild.textContent = CrashID;
  link.setAttribute("id", CrashID);
  link.removeEventListener("click", submitPendingReport, true);

  if (reportURL) {
    link.setAttribute("href", reportURL + CrashID);
    // redirect the user to their brand new crash report
    window.location.href = reportURL + CrashID;
  }
}

function submitForm(iframe, dump, extra, link)
{
  let reportData = parseKeyValuePairsFromFile(extra);
  let form = iframe.contentDocument.forms[0];
  if ('ServerURL' in reportData) {
    form.action = reportData.ServerURL;
    delete reportData.ServerURL;
  }
  else {
    return false;
  }
  // add the other data
  for (let [name, value] in Iterator(reportData)) {
    addFormEntry(iframe.contentDocument, form, name, value);
  }
  // tell the server not to throttle this, since it was manually submitted
  addFormEntry(iframe.contentDocument, form, "Throttleable", "0");
  // add the minidump
  iframe.contentDocument.getElementById('minidump').value = dump.path;

  // web progress listener
  const STATE_START = Ci.nsIWebProgressListener.STATE_START;
  const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;
  myListener = {
    QueryInterface: function(aIID) {
      if (aIID.equals(Ci.nsIWebProgressListener) ||
          aIID.equals(Ci.nsISupportsWeakReference) ||
          aIID.equals(Ci.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    },

    onStateChange: function(aWebProgress, aRequest, aFlag, aStatus) {
      if(aFlag & STATE_STOP) {
        iframe.docShell.removeProgressListener(myListener);
        myListener = null;
        link.className = "";

        //XXX: give some indication of failure?
        // check general request status first
        if (!Components.isSuccessCode(aStatus)) {
          document.body.removeChild(iframe);
          return 0;
        }
        // check HTTP status
        if (aRequest instanceof Ci.nsIHttpChannel &&
            aRequest.responseStatus != 200) {
          document.body.removeChild(iframe);
          return 0;
        }

        var ret = parseKeyValuePairs(iframe.contentDocument.documentElement.textContent);
        document.body.removeChild(iframe);
        submitSuccess(ret, link, dump, extra);
      }
      return 0;
    },

    onLocationChange: function(aProgress, aRequest, aURI) {return 0;},
    onProgressChange: function() {return 0;},
    onStatusChange: function() {return 0;},
    onSecurityChange: function() {return 0;},
    onLinkIconAvailable: function() {return 0;}
  };
  iframe.docShell.QueryInterface(Ci.nsIWebProgress);
  iframe.docShell.addProgressListener(myListener, Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);
  form.submit();
  return true;
}

function createAndSubmitForm(id, link) {
  let [dump, extra] = getPendingMinidump(id);
  if (!dump.exists() || !extra.exists())
    return false;
  let iframe = document.createElementNS("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "iframe");
  iframe.setAttribute("type", "content");
  iframe.onload = function() {
    if (iframe.contentWindow.location == "about:blank")
      return;
    iframe.onload = null;
    submitForm(iframe, dump, extra, link);
  };
  document.body.appendChild(iframe);
  iframe.webNavigation.loadURI("chrome://global/content/crash-submit-form.xhtml", 0, null, null, null);
  return true;
}

function submitPendingReport(event) {
  var link = event.target;
  var id = link.firstChild.textContent;
  if (createAndSubmitForm(id, link))
    link.className = "submitting";
  event.preventDefault();
  return false;
}

function findInsertionPoint(reports, date) {
  if (reports.length == 0)
    return 0;

  var min = 0;
  var max = reports.length - 1;
  while (min < max) {
    var mid = parseInt((min + max) / 2);
    if (reports[mid].date < date)
      max = mid - 1;
    else if (reports[mid].date > date)
      min = mid + 1;
    else
      return mid;
  }
  if (reports[min].date <= date)
    return min;
  return min+1;
}

function populateReportList() {
  var prefService = Cc["@mozilla.org/preferences-service;1"].
                    getService(Ci.nsIPrefBranch);

  try {
    reportURL = prefService.getCharPref("breakpad.reportURL");
    // Ignore any non http/https urls
    if (!/^https?:/i.test(reportURL))
      reportURL = null;
  }
  catch (e) { }
  if (!reportURL) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noConfig").style.display = "block";
    return;
  }
  var directoryService = Cc["@mozilla.org/file/directory_service;1"].
                         getService(Ci.nsIProperties);

  reportsDir = directoryService.get("UAppData", Ci.nsIFile);
  reportsDir.append("Crash Reports");
  reportsDir.append("submitted");

  var reports = [];
  if (reportsDir.exists() && reportsDir.isDirectory()) {
    var entries = reportsDir.directoryEntries;
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Ci.nsIFile);
      var leaf = file.leafName;
      if (leaf.substr(0, 3) == "bp-" &&
          leaf.substr(-4) == ".txt") {
        var entry = {
          id: leaf.slice(0, -4),
          date: file.lastModifiedTime,
          pending: false
        };
        var pos = findInsertionPoint(reports, entry.date);
        reports.splice(pos, 0, entry);
      }
    }
  }

  pendingDir = directoryService.get("UAppData", Ci.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");

  if (pendingDir.exists() && pendingDir.isDirectory()) {
    var entries = pendingDir.directoryEntries;
    while (entries.hasMoreElements()) {
      var file = entries.getNext().QueryInterface(Ci.nsIFile);
      var leaf = file.leafName;
      if (leaf.substr(-4) == ".dmp") {
        var entry = {
          id: leaf.slice(0, -4),
          date: file.lastModifiedTime,
          pending: true
        };
        var pos = findInsertionPoint(reports, entry.date);
        reports.splice(pos, 0, entry);
      }
    }
  }

  if (reports.length == 0) {
    document.getElementById("clear-reports").style.display = "none";
    document.getElementById("reportList").style.display = "none";
    document.getElementById("noReports").style.display = "block";
    return;
  }

  var formatter = Cc["@mozilla.org/intl/scriptabledateformat;1"].
                  createInstance(Ci.nsIScriptableDateFormat);
  var body = document.getElementById("tbody");
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var reportURI = ios.newURI(reportURL, null, null);
  // resolving this URI relative to /report/index
  var aboutThrottling = ios.newURI("../../about/throttling", null, reportURI);

  for (var i = 0; i < reports.length; i++) {
    var row = document.createElement("tr");
    var cell = document.createElement("td");
    row.appendChild(cell);
    var link = document.createElement("a");
    if (reports[i].pending) {
      link.setAttribute("href", aboutThrottling.spec);
      link.addEventListener("click", submitPendingReport, true);
    }
    else {
      link.setAttribute("href", reportURL + reports[i].id);
    }
    link.setAttribute("id", reports[i].id);
    link.appendChild(document.createTextNode(reports[i].id));
    cell.appendChild(link);

    var date = new Date(reports[i].date);
    cell = document.createElement("td");
    var datestr = formatter.FormatDate("",
                                       Ci.nsIScriptableDateFormat.dateFormatShort,
                                       date.getFullYear(),
                                       date.getMonth() + 1,
                                       date.getDate());
    cell.appendChild(document.createTextNode(datestr));
    row.appendChild(cell);
    cell = document.createElement("td");
    var timestr = formatter.FormatTime("",
                                       Ci.nsIScriptableDateFormat.timeFormatNoSeconds,
                                       date.getHours(),
                                       date.getMinutes(),
                                       date.getSeconds());
    cell.appendChild(document.createTextNode(timestr));
    row.appendChild(cell);
    body.appendChild(row);
  }
}

function clearReports() {
  var bundles = Cc["@mozilla.org/intl/stringbundle;1"].
                getService(Ci.nsIStringBundleService);
  var bundle = bundles.createBundle("chrome://global/locale/crashes.properties");
  var prompts = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                getService(Ci.nsIPromptService);
  if (!prompts.confirm(window,
                       bundle.GetStringFromName("deleteconfirm.title"),
                       bundle.GetStringFromName("deleteconfirm.description")))
    return;

  var entries = reportsDir.directoryEntries;
  while (entries.hasMoreElements()) {
    var file = entries.getNext().QueryInterface(Ci.nsIFile);
    var leaf = file.leafName;
    if (leaf.substr(0, 3) == "bp-" &&
        leaf.substr(-4) == ".txt") {
      file.remove(false);
    }
  }
  document.getElementById("clear-reports").style.display = "none";
  document.getElementById("reportList").style.display = "none";
  document.getElementById("noReports").style.display = "block";
}

function init() {
  getL10nStrings();
  populateReportList();
}
