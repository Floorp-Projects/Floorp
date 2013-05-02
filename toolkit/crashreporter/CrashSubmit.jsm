/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/KeyValueParser.jsm");

this.EXPORTED_SYMBOLS = [
  "CrashSubmit"
];

const Cc = Components.classes;
const Ci = Components.interfaces;
const STATE_START = Ci.nsIWebProgressListener.STATE_START;
const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;

const SUCCESS = "success";
const FAILED  = "failed";
const SUBMITTING = "submitting";

let reportURL = null;
let strings = null;
let myListener = null;

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

function getPendingDir() {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
                         getService(Ci.nsIProperties);
  let pendingDir = directoryService.get("UAppData", Ci.nsIFile);
  pendingDir.append("Crash Reports");
  pendingDir.append("pending");
  return pendingDir;
}

function getPendingMinidump(id) {
  let pendingDir = getPendingDir();
  let dump = pendingDir.clone();
  let extra = pendingDir.clone();
  dump.append(id + ".dmp");
  extra.append(id + ".extra");
  return [dump, extra];
}

function getAllPendingMinidumpsIDs() {
  let minidumps = [];
  let pendingDir = getPendingDir();

  if (!(pendingDir.exists() && pendingDir.isDirectory()))
    return [];
  let entries = pendingDir.directoryEntries;

  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);
    if (entry.isFile()) {
      let matches = entry.leafName.match(/(.+)\.extra$/);
      if (matches)
        minidumps.push(matches[1]);
    }
  }

  return minidumps;
}

function pruneSavedDumps() {
  const KEEP = 10;

  let pendingDir = getPendingDir();
  if (!(pendingDir.exists() && pendingDir.isDirectory()))
    return;
  let entries = pendingDir.directoryEntries;
  let entriesArray = [];

  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);
    if (entry.isFile()) {
      let matches = entry.leafName.match(/(.+)\.extra$/);
      if (matches)
	entriesArray.push(entry);
    }
  }

  entriesArray.sort(function(a,b) {
    let dateA = a.lastModifiedTime;
    let dateB = b.lastModifiedTime;
    if (dateA < dateB)
      return -1;
    if (dateB < dateA)
      return 1;
    return 0;
  });

  if (entriesArray.length > KEEP) {
    for (let i = 0; i < entriesArray.length - KEEP; ++i) {
      let extra = entriesArray[i];
      let matches = extra.leafName.match(/(.+)\.extra$/);
      if (matches) {
        let dump = extra.clone();
        dump.leafName = matches[1] + '.dmp';
        dump.remove(false);
        extra.remove(false);
      }
    }
  }
}

function addFormEntry(doc, form, name, value) {
  var input = doc.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  form.appendChild(input);
}

function writeSubmittedReport(crashID, viewURL) {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"].
                           getService(Ci.nsIProperties);
  let reportFile = directoryService.get("UAppData", Ci.nsIFile);
  reportFile.append("Crash Reports");
  reportFile.append("submitted");
  if (!reportFile.exists())
    reportFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0700);
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

// the Submitter class represents an individual submission.
function Submitter(id, submitSuccess, submitError, noThrottle,
                   extraExtraKeyVals) {
  this.id = id;
  this.successCallback = submitSuccess;
  this.errorCallback = submitError;
  this.noThrottle = noThrottle;
  this.additionalDumps = [];
  this.extraKeyVals = extraExtraKeyVals || {};
}

Submitter.prototype = {
  submitSuccess: function Submitter_submitSuccess(ret)
  {
    if (!ret.CrashID) {
      this.notifyStatus(FAILED);
      this.cleanup();
      return;
    }

    // Write out the details file to submitted/
    writeSubmittedReport(ret.CrashID, ret.ViewURL);

    // Delete from pending dir
    try {
      this.dump.remove(false);
      this.extra.remove(false);
      for (let i of this.additionalDumps) {
        i.dump.remove(false);
      }
    }
    catch (ex) {
      // report an error? not much the user can do here.
    }

    this.notifyStatus(SUCCESS, ret);
    this.cleanup();
  },

  cleanup: function Submitter_cleanup() {
    // drop some references just to be nice
    this.successCallback = null;
    this.errorCallback = null;
    this.iframe = null;
    this.dump = null;
    this.extra = null;
    this.additionalDumps = null;
    // remove this object from the list of active submissions
    let idx = CrashSubmit._activeSubmissions.indexOf(this);
    if (idx != -1)
      CrashSubmit._activeSubmissions.splice(idx, 1);
  },

  submitForm: function Submitter_submitForm()
  {
    if (!('ServerURL' in this.extraKeyVals)) {
      return false;
    }
    let serverURL = this.extraKeyVals.ServerURL;

    // Override the submission URL from the environment or prefs.

    var envOverride = Cc['@mozilla.org/process/environment;1'].
      getService(Ci.nsIEnvironment).get("MOZ_CRASHREPORTER_URL");
    if (envOverride != '') {
      serverURL = envOverride;
    }
    else if ('PluginHang' in this.extraKeyVals) {
      try {
        serverURL = Services.prefs.
          getCharPref("toolkit.crashreporter.pluginHangSubmitURL");
      } catch(e) { }
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", serverURL, true);

    let formData = Cc["@mozilla.org/files/formdata;1"]
                   .createInstance(Ci.nsIDOMFormData);
    // add the data
    for (let [name, value] in Iterator(this.extraKeyVals)) {
      if (name != "ServerURL") {
        formData.append(name, value);
      }
    }
    if (this.noThrottle) {
      // tell the server not to throttle this, since it was manually submitted
      formData.append("Throttleable", "0");
    }
    // add the minidumps
    formData.append("upload_file_minidump", File(this.dump.path));
    if (this.additionalDumps.length > 0) {
      let names = [];
      for (let i of this.additionalDumps) {
        names.push(i.name);
        formData.append("upload_file_minidump_"+i.name,
                        File(i.dump.path));
      }
    }

    let self = this;
    xhr.addEventListener("readystatechange", function (aEvt) {
      if (xhr.readyState == 4) {
        if (xhr.status != 200) {
          self.notifyStatus(FAILED);
          self.cleanup();
        } else {
          let ret = parseKeyValuePairs(xhr.responseText);
          self.submitSuccess(ret);
        }
      }
    }, false);

    xhr.send(formData);
    return true;
  },

  notifyStatus: function Submitter_notify(status, ret)
  {
    let propBag = Cc["@mozilla.org/hash-property-bag;1"].
                  createInstance(Ci.nsIWritablePropertyBag2);
    propBag.setPropertyAsAString("minidumpID", this.id);
    if (status == SUCCESS) {
      propBag.setPropertyAsAString("serverCrashID", ret.CrashID);
    }

    let extraKeyValsBag = Cc["@mozilla.org/hash-property-bag;1"].
                          createInstance(Ci.nsIWritablePropertyBag2);
    for (let key in this.extraKeyVals) {
      extraKeyValsBag.setPropertyAsAString(key, this.extraKeyVals[key]);
    }
    propBag.setPropertyAsInterface("extra", extraKeyValsBag);

    Services.obs.notifyObservers(propBag, "crash-report-status", status);

    switch (status) {
      case SUCCESS:
        if (this.successCallback)
          this.successCallback(this.id, ret);
        break;
      case FAILED:
        if (this.errorCallback)
          this.errorCallback(this.id);
        break;
      default:
        // no callbacks invoked.
    }
  },

  submit: function Submitter_submit()
  {
    let [dump, extra] = getPendingMinidump(this.id);
    if (!dump.exists() || !extra.exists()) {
      this.notifyStatus(FAILED);
      this.cleanup();
      return false;
    }

    let extraKeyVals = parseKeyValuePairsFromFile(extra);
    for (let key in extraKeyVals) {
      if (!(key in this.extraKeyVals)) {
        this.extraKeyVals[key] = extraKeyVals[key];
      }
    }

    let additionalDumps = [];
    if ("additional_minidumps" in this.extraKeyVals) {
      let names = this.extraKeyVals.additional_minidumps.split(',');
      for (let name of names) {
        let [dump, extra] = getPendingMinidump(this.id + "-" + name);
        if (!dump.exists()) {
          this.notifyStatus(FAILED);
          this.cleanup();
          return false;
        }
        additionalDumps.push({'name': name, 'dump': dump});
      }
    }

    this.notifyStatus(SUBMITTING);

    this.dump = dump;
    this.extra = extra;
    this.additionalDumps = additionalDumps;

    if (!this.submitForm()) {
       this.notifyStatus(FAILED);
       this.cleanup();
       return false;
    }
    return true;
  }
};

//===================================
// External API goes here
this.CrashSubmit = {
  /**
   * Submit the crash report named id.dmp from the "pending" directory.
   *
   * @param id
   *        Filename (minus .dmp extension) of the minidump to submit.
   * @param params
   *        An object containing any of the following optional parameters:
   *        - submitSuccess
   *          A function that will be called if the report is submitted
   *          successfully with two parameters: the id that was passed
   *          to this function, and an object containing the key/value
   *          data returned from the server in its properties.
   *        - submitError
   *          A function that will be called with one parameter if the
   *          report fails to submit: the id that was passed to this
   *          function.
   *        - noThrottle
   *          If true, this crash report should be submitted with
   *          an extra parameter of "Throttleable=0" indicating that
   *          it should be processed right away. This should be set
   *          when the report is being submitted and the user expects
   *          to see the results immediately. Defaults to false.
   *        - extraExtraKeyVals
   *          An object whose key-value pairs will be merged with the data from
   *          the ".extra" file submitted with the report.  The properties of
   *          this object will override properties of the same name in the
   *          .extra file.
   *
   * @return true if the submission began successfully, or false if
   *         it failed for some reason. (If the dump file does not
   *         exist, for example.)
   */
  submit: function CrashSubmit_submit(id, params)
  {
    params = params || {};
    let submitSuccess = null;
    let submitError = null;
    let noThrottle = false;
    let extraExtraKeyVals = null;

    if ('submitSuccess' in params)
      submitSuccess = params.submitSuccess;
    if ('submitError' in params)
      submitError = params.submitError;
    if ('noThrottle' in params)
      noThrottle = params.noThrottle;
    if ('extraExtraKeyVals' in params)
      extraExtraKeyVals = params.extraExtraKeyVals;

    let submitter = new Submitter(id,
                                  submitSuccess,
                                  submitError,
                                  noThrottle,
                                  extraExtraKeyVals);
    CrashSubmit._activeSubmissions.push(submitter);
    return submitter.submit();
  },

  /**
   * Delete the minidup from the "pending" directory.
   *
   * @param id
   *        Filename (minus .dmp extension) of the minidump to delete.
   */
  delete: function CrashSubmit_delete(id) {
    let [dump, extra] = getPendingMinidump(id);
    dump.QueryInterface(Ci.nsIFile).remove(false);
    extra.QueryInterface(Ci.nsIFile).remove(false);
  },

  /**
   * Get the list of pending crash IDs.
   *
   * @return an array of string, each being an ID as
   *         expected to be passed to submit()
   */
  pendingIDs: function CrashSubmit_pendingIDs() {
    return getAllPendingMinidumpsIDs();
  },

  /**
   * Prune the saved dumps.
   */
  pruneSavedDumps: function CrashSubmit_pruneSavedDumps() {
    pruneSavedDumps();
  },

  // List of currently active submit objects
  _activeSubmissions: []
};

// Run this when first loaded
getL10nStrings();
