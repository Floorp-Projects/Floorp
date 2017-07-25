/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/KeyValueParser.jsm");
Cu.importGlobalProperties(["File"]);

XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

this.EXPORTED_SYMBOLS = [
  "CrashSubmit"
];

const STATE_START = Ci.nsIWebProgressListener.STATE_START;
const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;

const SUCCESS = "success";
const FAILED  = "failed";
const SUBMITTING = "submitting";

const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

// TODO: this is still synchronous; need an async INI parser to make it async
function parseINIStrings(path) {
  let file = new FileUtils.File(path);
  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(Ci.nsIINIParserFactory);
  let parser = factory.createINIParser(file);
  let obj = {};
  let en = parser.getKeys("Strings");
  while (en.hasMore()) {
    let key = en.getNext();
    obj[key] = parser.getString("Strings", key);
  }
  return obj;
}

// Since we're basically re-implementing (with async) part of the crashreporter
// client here, we'll just steal the strings we need from crashreporter.ini
async function getL10nStrings() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let path = OS.Path.join(dirSvc.get("GreD", Ci.nsIFile).path,
                          "crashreporter.ini");
  let pathExists = await OS.File.exists(path);

  if (!pathExists) {
    // we if we're on a mac
    let parentDir = OS.Path.dirname(path);
    path = OS.Path.join(parentDir, "MacOS", "crashreporter.app", "Contents",
                        "Resources", "crashreporter.ini");

    let pathExists = await OS.File.exists(path);

    if (!pathExists) {
      // This happens on Android where everything is in an APK.
      // Android users can't see the contents of the submitted files
      // anyway, so just hardcode some fallback strings.
      return {
        "crashid": "Crash ID: %s",
        "reporturl": "You can view details of this crash at %s"
      };
    }
  }

  let crstrings = parseINIStrings(path);
  let strings = {
    "crashid": crstrings.CrashID,
    "reporturl": crstrings.CrashDetailsURL
  };

  path = OS.Path.join(dirSvc.get("XCurProcD", Ci.nsIFile).path,
                      "crashreporter-override.ini");
  pathExists = await OS.File.exists(path);

  if (pathExists) {
    crstrings = parseINIStrings(path);

    if ("CrashID" in crstrings) {
      strings.crashid = crstrings.CrashID;
    }

    if ("CrashDetailsURL" in crstrings) {
      strings.reporturl = crstrings.CrashDetailsURL;
    }
  }

  return strings;
}

function getDir(name) {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let uAppDataPath = dirSvc.get("UAppData", Ci.nsIFile).path;
  return OS.Path.join(uAppDataPath, "Crash Reports", name);
}

async function isDirAsync(path) {
  try {
    let dirInfo = await OS.File.stat(path);

    if (!dirInfo.isDir) {
      return false;
    }
  } catch (ex) {
    return false;
  }

  return true;
}

async function writeFileAsync(dirName, fileName, data) {
  let dirPath = getDir(dirName);
  let filePath = OS.Path.join(dirPath, fileName);
  // succeeds even with existing path, permissions 700
  await OS.File.makeDir(dirPath, { unixFlags: OS.Constants.libc.S_IRWXU });
  await OS.File.writeAtomic(filePath, data, { encoding: "utf-8" });
}

function getPendingMinidump(id) {
  let pendingDir = getDir("pending");

  return [".dmp", ".extra", ".memory.json.gz"].map(suffix => {
    return OS.Path.join(pendingDir, `${id}${suffix}`);
  });
}

function addFormEntry(doc, form, name, value) {
  let input = doc.createElement("input");
  input.type = "hidden";
  input.name = name;
  input.value = value;
  form.appendChild(input);
}

async function writeSubmittedReportAsync(crashID, viewURL) {
  let strings = await getL10nStrings();
  let data = strings.crashid.replace("%s", crashID);

  if (viewURL) {
    data += "\n" + strings.reporturl.replace("%s", viewURL);
  }

  await writeFileAsync("submitted", `${crashID}.txt`, data);
}

// the Submitter class represents an individual submission.
function Submitter(id, recordSubmission, noThrottle, extraExtraKeyVals) {
  this.id = id;
  this.recordSubmission = recordSubmission;
  this.noThrottle = noThrottle;
  this.additionalDumps = [];
  this.extraKeyVals = extraExtraKeyVals || {};
  // mimic deferred Promise behavior
  this.submitStatusPromise = new Promise((resolve, reject) => {
    this.resolveSubmitStatusPromise = resolve;
    this.rejectSubmitStatusPromise = reject;
  });
}

Submitter.prototype = {
  submitSuccess: async function Submitter_submitSuccess(ret) {
    // Write out the details file to submitted
    await writeSubmittedReportAsync(ret.CrashID, ret.ViewURL);

    try {
      let toDelete = [this.dump, this.extra];

      if (this.memory) {
        toDelete.push(this.memory);
      }

      for (let dump of this.additionalDumps) {
        toDelete.push(dump);
      }

      await Promise.all(toDelete.map(path => {
        return OS.File.remove(path, { ignoreAbsent: true });
      }));
    } catch (ex) {
      Cu.reportError(ex);
    }

    this.notifyStatus(SUCCESS, ret);
    this.cleanup();
  },

  cleanup: function Submitter_cleanup() {
    // drop some references just to be nice
    this.iframe = null;
    this.dump = null;
    this.extra = null;
    this.memory = null;
    this.additionalDumps = null;
    // remove this object from the list of active submissions
    let idx = CrashSubmit._activeSubmissions.indexOf(this);
    if (idx != -1) {
      CrashSubmit._activeSubmissions.splice(idx, 1);
    }
  },

  submitForm: function Submitter_submitForm() {
    if (!("ServerURL" in this.extraKeyVals)) {
      return false;
    }
    let serverURL = this.extraKeyVals.ServerURL;

    // Override the submission URL from the environment

    let envOverride = Cc["@mozilla.org/process/environment;1"].
      getService(Ci.nsIEnvironment).get("MOZ_CRASHREPORTER_URL");
    if (envOverride != "") {
      serverURL = envOverride;
    }

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("POST", serverURL, true);

    let formData = Cc["@mozilla.org/files/formdata;1"]
                   .createInstance(Ci.nsIDOMFormData);
    // add the data
    for (let [name, value] of Object.entries(this.extraKeyVals)) {
      if (name != "ServerURL") {
        formData.append(name, value);
      }
    }
    if (this.noThrottle) {
      // tell the server not to throttle this, since it was manually submitted
      formData.append("Throttleable", "0");
    }
    // add the minidumps
    let promises = [
      File.createFromFileName(this.dump).then(file => {
        formData.append("upload_file_minidump", file);
      })
    ];

    if (this.memory) {
      promises.push(File.createFromFileName(this.memory).then(file => {
        formData.append("memory_report", file);
      }));
    }

    if (this.additionalDumps.length > 0) {
      let names = [];
      for (let i of this.additionalDumps) {
        names.push(i.name);
        promises.push(File.createFromFileName(i.dump).then(file => {
          formData.append("upload_file_minidump_" + i.name, file);
        }));
      }
    }

    let manager = Services.crashmanager;
    let submissionID = manager.generateSubmissionID();

    xhr.addEventListener("readystatechange", (evt) => {
      if (xhr.readyState == 4) {
        let ret =
          xhr.status === 200 ? parseKeyValuePairs(xhr.responseText) : {};
        let submitted = !!ret.CrashID;
        let p = Promise.resolve();

        if (this.recordSubmission) {
          let result = submitted ? manager.SUBMISSION_RESULT_OK :
                                   manager.SUBMISSION_RESULT_FAILED;
          p = manager.addSubmissionResult(this.id, submissionID, new Date(),
                                          result);
          if (submitted) {
            manager.setRemoteCrashID(this.id, ret.CrashID);
          }
        }

        p.then(() => {
          if (submitted) {
            this.submitSuccess(ret);
          } else {
            this.notifyStatus(FAILED);
            this.cleanup();
          }
        });
      }
    });

    let p = Promise.all(promises);
    let id = this.id;

    if (this.recordSubmission) {
      p = p.then(() => { return manager.ensureCrashIsPresent(id); })
           .then(() => {
             return manager.addSubmissionAttempt(id, submissionID, new Date());
            });
    }
    p.then(() => { xhr.send(formData); });
    return true;
  },

  notifyStatus: function Submitter_notify(status, ret) {
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
        this.resolveSubmitStatusPromise(ret.CrashID);
        break;
      case FAILED:
        this.rejectSubmitStatusPromise(FAILED);
        break;
      default:
        // no callbacks invoked.
    }
  },

  submit: async function Submitter_submit() {
    let [dump, extra, memory] = getPendingMinidump(this.id);
    let [dumpExists, extraExists, memoryExists] = await Promise.all([
      OS.File.exists(dump),
      OS.File.exists(extra),
      OS.File.exists(memory)
    ]);

    if (!dumpExists || !extraExists) {
      this.notifyStatus(FAILED);
      this.cleanup();
      return this.submitStatusPromise;
    }

    this.dump = dump;
    this.extra = extra;
    this.memory = memoryExists ? memory : null;

    let extraKeyVals = await parseKeyValuePairsFromFileAsync(extra);

    for (let key in extraKeyVals) {
      if (!(key in this.extraKeyVals)) {
        this.extraKeyVals[key] = extraKeyVals[key];
      }
    }

    let additionalDumps = [];

    if ("additional_minidumps" in this.extraKeyVals) {
      let dumpsExistsPromises = [];
      let names = this.extraKeyVals.additional_minidumps.split(",");

      for (let name of names) {
        let [dump /* , extra, memory */] = getPendingMinidump(this.id + "-" + name);

        dumpsExistsPromises.push(OS.File.exists(dump));
        additionalDumps.push({name, dump});
      }

      let dumpsExist = await Promise.all(dumpsExistsPromises);
      let allDumpsExist = dumpsExist.every(exists => exists);

      if (!allDumpsExist) {
        this.notifyStatus(FAILED);
        this.cleanup();
        return this.submitStatusPromise;
      }
    }

    this.notifyStatus(SUBMITTING);
    this.additionalDumps = additionalDumps;

    if (!(await this.submitForm())) {
       this.notifyStatus(FAILED);
       this.cleanup();
    }

    return this.submitStatusPromise;
  }
};

// ===================================
// External API goes here
this.CrashSubmit = {
  /**
   * Submit the crash report named id.dmp from the "pending" directory.
   *
   * @param id
   *        Filename (minus .dmp extension) of the minidump to submit.
   * @param params
   *        An object containing any of the following optional parameters:
   *        - recordSubmission
   *          If true, a submission event is recorded in CrashManager.
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
   *  @return a Promise that is fulfilled with the server crash ID when the
   *          submission succeeds and rejected otherwise.
   */
  submit: function CrashSubmit_submit(id, params) {
    params = params || {};
    let recordSubmission = false;
    let noThrottle = false;
    let extraExtraKeyVals = null;

    if ("recordSubmission" in params) {
      recordSubmission = params.recordSubmission;
    }

    if ("noThrottle" in params) {
      noThrottle = params.noThrottle;
    }

    if ("extraExtraKeyVals" in params) {
      extraExtraKeyVals = params.extraExtraKeyVals;
    }

    let submitter = new Submitter(id, recordSubmission,
                                  noThrottle, extraExtraKeyVals);
    CrashSubmit._activeSubmissions.push(submitter);
    return submitter.submit();
  },

  /**
   * Delete the minidup from the "pending" directory.
   *
   * @param id
   *        Filename (minus .dmp extension) of the minidump to delete.
   *
   * @return a Promise that is fulfilled when the minidump is deleted and
   *         rejected otherwise
   */
  delete: async function CrashSubmit_delete(id) {
    await Promise.all(getPendingMinidump(id).map(path => {
      return OS.File.remove(path, { ignoreAbsent: true });
    }));
  },

  /**
   * Add a .dmg.ignore file along side the .dmp file to indicate that the user
   * shouldn't be prompted to submit this crash report again.
   *
   * @param id
   *        Filename (minus .dmp extension) of the report to ignore
   *
   * @return a Promise that is fulfilled when (if) the .dmg.ignore is created
   *         and rejected otherwise.
   */
  ignore: async function CrashSubmit_ignore(id) {
    let [dump /* , extra, memory */] = getPendingMinidump(id);
    let file = await OS.File.open(`${dump}.ignore`, { create: true },
                                  { unixFlags: OS.Constants.libc.O_CREAT });
    await file.close();
  },

  /**
   * Get the list of pending crash IDs, excluding those marked to be ignored
   * @param minFileDate
   *     A Date object. Any files last modified before that date will be ignored
   *
   * @return a Promise that is fulfilled with an array of string, each
   *         being an ID as expected to be passed to submit() or ignore()
   */
  pendingIDs: async function CrashSubmit_pendingIDs(minFileDate) {
    let ids = [];
    let dirIter = null;
    let pendingDir = getDir("pending");

    try {
      dirIter = new OS.File.DirectoryIterator(pendingDir);
    } catch (ex) {
      if (ex.becauseNoSuchFile) {
        return ids;
      }

      Cu.reportError(ex);
      throw ex;
    }

    try {
      let entries = Object.create(null);
      let ignored = Object.create(null);

      // `await` in order to ensure all callbacks are called
      await dirIter.forEach(entry => {
        if (!entry.isDir /* is file */) {
          let matches = entry.name.match(/(.+)\.dmp$/);

          if (matches) {
            let id = matches[1];

            if (UUID_REGEX.test(id)) {
              entries[id] = OS.File.stat(entry.path);
            }
          } else {
            // maybe it's a .ignore file
            let matchesIgnore = entry.name.match(/(.+)\.dmp.ignore$/);

            if (matchesIgnore) {
              let id = matchesIgnore[1];

              if (UUID_REGEX.test(id)) {
                ignored[id] = true;
              }
            }
          }
        }
      });

      for (let entry in entries) {
        let entryInfo = await entries[entry];

        if (!(entry in ignored) &&
            entryInfo.lastAccessDate > minFileDate) {
          ids.push(entry);
        }
      }
    } catch (ex) {
      Cu.reportError(ex);
      throw ex;
    } finally {
      dirIter.close();
    }

    return ids;
  },

  /**
   * Prune the saved dumps.
   *
   * @return a Promise that is fulfilled when the daved dumps are deleted and
   *         rejected otherwise
   */
  pruneSavedDumps: async function CrashSubmit_pruneSavedDumps() {
    const KEEP = 10;

    let dirIter = null;
    let dirEntries = [];
    let pendingDir = getDir("pending");

    try {
      dirIter = new OS.File.DirectoryIterator(pendingDir);
    } catch (ex) {
      if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        return [];
      }

      throw ex;
    }

    try {
      await dirIter.forEach(entry => {
        if (!entry.isDir /* is file */) {
          let matches = entry.name.match(/(.+)\.extra$/);

          if (matches) {
            dirEntries.push({
              name: entry.name,
              path: entry.path,
              // dispatch promise instead of blocking iteration on `await`
              infoPromise: OS.File.stat(entry.path)
            });
          }
        }
      });
    } catch (ex) {
      Cu.reportError(ex);
      throw ex;
    } finally {
      dirIter.close();
    }

    dirEntries.sort(async (a, b) => {
      let dateA = (await a.infoPromise).lastModificationDate;
      let dateB = (await b.infoPromise).lastModificationDate;

      if (dateA < dateB) {
        return -1;
      }

      if (dateB < dateA) {
        return 1;
      }

      return 0;
    });

    if (dirEntries.length > KEEP) {
      let toDelete = [];

      for (let i = 0; i < dirEntries.length - KEEP; ++i) {
        let extra = dirEntries[i];
        let matches = extra.leafName.match(/(.+)\.extra$/);

        if (matches) {
          let pathComponents = OS.Path.split(extra.path);
          pathComponents[pathComponents.length - 1] = matches[1];
          let path = OS.Path.join(...pathComponents);

          toDelete.push(extra.path, `${path}.dmp`, `${path}.memory.json.gz`);
        }
      }

      await Promise.all(toDelete.map(path => {
        return OS.File.remove(path, { ignoreAbsent: true });
      }));
    }
  },

  // List of currently active submit objects
  _activeSubmissions: []
};
