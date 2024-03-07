/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

const SUCCESS = "success";
const FAILED = "failed";
const SUBMITTING = "submitting";

const UUID_REGEX =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
const SUBMISSION_REGEX =
  /^bp-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

// TODO: this is still synchronous; need an async INI parser to make it async
function parseINIStrings(path) {
  let file = new FileUtils.File(path);
  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].getService(
    Ci.nsIINIParserFactory
  );
  let parser = factory.createINIParser(file);
  let obj = {};
  for (let key of parser.getKeys("Strings")) {
    obj[key] = parser.getString("Strings", key);
  }
  return obj;
}

// Since we're basically re-implementing (with async) part of the crashreporter
// client here, we'll just steal the strings we need from crashreporter.ini
async function getL10nStrings() {
  let path = PathUtils.join(
    Services.dirsvc.get("GreD", Ci.nsIFile).path,
    "crashreporter.ini"
  );
  let pathExists = await IOUtils.exists(path);

  if (!pathExists) {
    // we if we're on a mac
    let parentDir = PathUtils.parent(path);
    path = PathUtils.join(
      parentDir,
      "MacOS",
      "crashreporter.app",
      "Contents",
      "Resources",
      "crashreporter.ini"
    );

    let pathExists = await IOUtils.exists(path);

    if (!pathExists) {
      // This happens on Android where everything is in an APK.
      // Android users can't see the contents of the submitted files
      // anyway, so just hardcode some fallback strings.
      return {
        crashid: "Crash ID: %s",
        reporturl: "You can view details of this crash at %s",
      };
    }
  }

  let crstrings = parseINIStrings(path);
  let strings = {
    crashid: crstrings.CrashID,
    reporturl: crstrings.CrashDetailsURL,
  };

  path = PathUtils.join(
    Services.dirsvc.get("XCurProcD", Ci.nsIFile).path,
    "crashreporter-override.ini"
  );
  pathExists = await IOUtils.exists(path);

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
  let uAppDataPath = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  return PathUtils.join(uAppDataPath, "Crash Reports", name);
}

async function writeFileAsync(dirName, fileName, data) {
  let dirPath = getDir(dirName);
  let filePath = PathUtils.join(dirPath, fileName);
  await IOUtils.makeDirectory(dirPath, { permissions: 0o700 });
  await IOUtils.writeUTF8(filePath, data);
}

function getPendingMinidump(id) {
  let pendingDir = getDir("pending");

  return [".dmp", ".extra", ".memory.json.gz"].map(suffix => {
    return PathUtils.join(pendingDir, `${id}${suffix}`);
  });
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
  this.extraKeyVals = extraExtraKeyVals;
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

      for (let entry of this.additionalDumps) {
        toDelete.push(entry.dump);
      }

      await Promise.all(
        toDelete.map(path => {
          return IOUtils.remove(path, { ignoreAbsent: true });
        })
      );
    } catch (ex) {
      console.error(ex);
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

  parseResponse: function Submitter_parseResponse(response) {
    let parsedResponse = {};

    for (let line of response.split("\n")) {
      let data = line.split("=");

      if (
        (data.length == 2 &&
          data[0] == "CrashID" &&
          SUBMISSION_REGEX.test(data[1])) ||
        data[0] == "ViewURL"
      ) {
        parsedResponse[data[0]] = data[1];
      }
    }

    return parsedResponse;
  },

  submitForm: function Submitter_submitForm() {
    if (!("ServerURL" in this.extraKeyVals)) {
      return false;
    }
    let serverURL = this.extraKeyVals.ServerURL;
    delete this.extraKeyVals.ServerURL;

    // Override the submission URL from the environment
    let envOverride = Services.env.get("MOZ_CRASHREPORTER_URL");
    if (envOverride != "") {
      serverURL = envOverride;
    }

    let xhr = new XMLHttpRequest();
    xhr.open("POST", serverURL, true);

    let formData = new FormData();

    // tell the server not to throttle this if requested
    this.extraKeyVals.Throttleable = this.noThrottle ? "0" : "1";

    // add the data
    let payload = Object.assign({}, this.extraKeyVals);
    let json = new Blob([JSON.stringify(payload)], {
      type: "application/json",
    });
    formData.append("extra", json);

    // add the minidumps
    let promises = [
      File.createFromFileName(this.dump, {
        type: "application/octet-stream",
      }).then(file => {
        formData.append("upload_file_minidump", file);
      }),
    ];

    if (this.memory) {
      promises.push(
        File.createFromFileName(this.memory, {
          type: "application/gzip",
        }).then(file => {
          formData.append("memory_report", file);
        })
      );
    }

    if (this.additionalDumps.length) {
      let names = [];
      for (let i of this.additionalDumps) {
        names.push(i.name);
        promises.push(
          File.createFromFileName(i.dump, {
            type: "application/octet-stream",
          }).then(file => {
            formData.append("upload_file_minidump_" + i.name, file);
          })
        );
      }
    }

    let manager = Services.crashmanager;
    let submissionID = manager.generateSubmissionID();

    xhr.addEventListener("readystatechange", evt => {
      if (xhr.readyState == 4) {
        let ret =
          xhr.status === 200 ? this.parseResponse(xhr.responseText) : {};
        let submitted = !!ret.CrashID;
        let p = Promise.resolve();

        if (this.recordSubmission) {
          let result = submitted
            ? manager.SUBMISSION_RESULT_OK
            : manager.SUBMISSION_RESULT_FAILED;
          p = manager.addSubmissionResult(
            this.id,
            submissionID,
            new Date(),
            result
          );
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
      p = p.then(() => {
        return manager.addSubmissionAttempt(id, submissionID, new Date());
      });
    }
    p.then(() => {
      xhr.send(formData);
    });
    return true;
  },

  notifyStatus: function Submitter_notify(status, ret) {
    let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
    propBag.setPropertyAsAString("minidumpID", this.id);
    if (status == SUCCESS) {
      propBag.setPropertyAsAString("serverCrashID", ret.CrashID);
    }

    let extraKeyValsBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag2
    );
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

  readAnnotations: async function Submitter_readAnnotations(extra) {
    // These annotations are used only by the crash reporter client and should
    // not be submitted to Socorro.
    const strippedAnnotations = [
      "StackTraces",
      "TelemetryClientId",
      "TelemetrySessionId",
      "TelemetryServerURL",
    ];
    let extraKeyVals = await IOUtils.readJSON(extra);

    this.extraKeyVals = { ...extraKeyVals, ...this.extraKeyVals };
    strippedAnnotations.forEach(key => delete this.extraKeyVals[key]);
  },

  submit: async function Submitter_submit() {
    if (this.recordSubmission) {
      await Services.crashmanager.ensureCrashIsPresent(this.id);
    }

    let [dump, extra, memory] = getPendingMinidump(this.id);
    let [dumpExists, extraExists, memoryExists] = await Promise.all([
      IOUtils.exists(dump),
      IOUtils.exists(extra),
      IOUtils.exists(memory),
    ]);

    if (!dumpExists || !extraExists) {
      this.notifyStatus(FAILED);
      this.cleanup();
      return this.submitStatusPromise;
    }

    this.dump = dump;
    this.extra = extra;
    this.memory = memoryExists ? memory : null;
    await this.readAnnotations(extra);

    let additionalDumps = [];

    if ("additional_minidumps" in this.extraKeyVals) {
      let dumpsExistsPromises = [];
      let names = this.extraKeyVals.additional_minidumps.split(",");

      for (let name of names) {
        let [dump /* , extra, memory */] = getPendingMinidump(
          this.id + "-" + name
        );

        dumpsExistsPromises.push(IOUtils.exists(dump));
        additionalDumps.push({ name, dump });
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
  },
};

// ===================================
// External API goes here
export var CrashSubmit = {
  // A set of strings representing how a user subnmitted a given crash
  SUBMITTED_FROM_AUTO: "Auto",
  SUBMITTED_FROM_INFOBAR: "Infobar",
  SUBMITTED_FROM_ABOUT_CRASHES: "AboutCrashes",
  SUBMITTED_FROM_CRASH_TAB: "CrashedTab",

  /**
   * Submit the crash report named id.dmp from the "pending" directory.
   *
   * @param id
   *        Filename (minus .dmp extension) of the minidump to submit.
   * @param submittedFrom
   *        One of the SUBMITTED_FROM_* constants representing how the
   *        user submitted this crash.
   * @param params
   *        An object containing any of the following optional parameters:
   *        - recordSubmission
   *          If true, a submission event is recorded in CrashManager.
   *        - noThrottle
   *          If true, this crash report should be submitted with
   *          the Throttleable annotation set to "0" indicating that
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
  submit: function CrashSubmit_submit(id, submittedFrom, params) {
    params = params || {};
    let recordSubmission = false;
    let noThrottle = false;
    let extraExtraKeyVals = {};

    if ("recordSubmission" in params) {
      recordSubmission = params.recordSubmission;
    }

    if ("noThrottle" in params) {
      noThrottle = params.noThrottle;
    }

    if ("extraExtraKeyVals" in params) {
      extraExtraKeyVals = params.extraExtraKeyVals;
    }

    extraExtraKeyVals.SubmittedFrom = submittedFrom;

    let submitter = new Submitter(
      id,
      recordSubmission,
      noThrottle,
      extraExtraKeyVals
    );
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
    await Promise.all(
      getPendingMinidump(id).map(path => {
        return IOUtils.remove(path);
      })
    );
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
    const ignorePath = `${dump}.ignore`;
    await IOUtils.writeUTF8(ignorePath, "", { mode: "create" });
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
    let pendingDir = getDir("pending");

    if (!(await IOUtils.exists(pendingDir))) {
      return ids;
    }

    let children;
    try {
      children = await IOUtils.getChildren(pendingDir);
    } catch (ex) {
      console.error(ex);
      throw ex;
    }

    try {
      const entries = Object.create(null);
      const ignored = Object.create(null);

      for (const child of children) {
        const info = await IOUtils.stat(child);

        if (info.type !== "directory") {
          const name = PathUtils.filename(child);
          const matches = name.match(/(.+)\.dmp$/);
          if (matches) {
            const id = matches[1];

            if (UUID_REGEX.test(id)) {
              entries[id] = info;
            }
          } else {
            // maybe it's a .ignore file
            const matchesIgnore = name.match(/(.+)\.dmp.ignore$/);
            if (matchesIgnore) {
              const id = matchesIgnore[1];

              if (UUID_REGEX.test(id)) {
                ignored[id] = true;
              }
            }
          }
        }
      }

      for (const [id, info] of Object.entries(entries)) {
        const accessDate = new Date(info.lastAccessed);
        if (!(id in ignored) && accessDate > minFileDate) {
          ids.push(id);
        }
      }
    } catch (ex) {
      console.error(ex);
      throw ex;
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

    let dirEntries = [];
    let pendingDir = getDir("pending");

    let children;
    try {
      children = await IOUtils.getChildren(pendingDir);
    } catch (ex) {
      if (DOMException.isInstance(ex) && ex.name === "NotFoundError") {
        return [];
      }

      throw ex;
    }

    for (const path of children) {
      let infoPromise;
      try {
        infoPromise = IOUtils.stat(path);
      } catch (ex) {
        console.error(ex);
        throw ex;
      }

      const name = PathUtils.filename(path);

      if (name.match(/(.+)\.extra$/)) {
        dirEntries.push({
          name,
          path,
          infoPromise,
        });
      }
    }

    dirEntries.sort(async (a, b) => {
      let dateA = (await a.infoPromise).lastModified;
      let dateB = (await b.infoPromise).lastModified;

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
          let pathComponents = PathUtils.split(extra.path);
          pathComponents[pathComponents.length - 1] = matches[1];
          let path = PathUtils.join(...pathComponents);

          toDelete.push(extra.path, `${path}.dmp`, `${path}.memory.json.gz`);
        }
      }

      await Promise.all(
        toDelete.map(path => {
          return IOUtils.remove(path, { ignoreAbsent: true });
        })
      );
    }
  },

  // List of currently active submit objects
  _activeSubmissions: [],
};
