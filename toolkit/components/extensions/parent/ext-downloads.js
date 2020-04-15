/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadPaths",
  "resource://gre/modules/DownloadPaths.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);

var { EventEmitter, ignoreEvent } = ExtensionCommon;

const DOWNLOAD_ITEM_FIELDS = [
  "id",
  "url",
  "referrer",
  "filename",
  "incognito",
  "danger",
  "mime",
  "startTime",
  "endTime",
  "estimatedEndTime",
  "state",
  "paused",
  "canResume",
  "error",
  "bytesReceived",
  "totalBytes",
  "fileSize",
  "exists",
  "byExtensionId",
  "byExtensionName",
];

const DOWNLOAD_DATE_FIELDS = ["startTime", "endTime", "estimatedEndTime"];

// Fields that we generate onChanged events for.
const DOWNLOAD_ITEM_CHANGE_FIELDS = [
  "endTime",
  "state",
  "paused",
  "canResume",
  "error",
  "exists",
];

// From https://fetch.spec.whatwg.org/#forbidden-header-name
// Since bug 1367626 we allow extensions to set REFERER.
const FORBIDDEN_HEADERS = [
  "ACCEPT-CHARSET",
  "ACCEPT-ENCODING",
  "ACCESS-CONTROL-REQUEST-HEADERS",
  "ACCESS-CONTROL-REQUEST-METHOD",
  "CONNECTION",
  "CONTENT-LENGTH",
  "COOKIE",
  "COOKIE2",
  "DATE",
  "DNT",
  "EXPECT",
  "HOST",
  "KEEP-ALIVE",
  "ORIGIN",
  "TE",
  "TRAILER",
  "TRANSFER-ENCODING",
  "UPGRADE",
  "VIA",
];

const FORBIDDEN_PREFIXES = /^PROXY-|^SEC-/i;

const PROMPTLESS_DOWNLOAD_PREF = "browser.download.useDownloadDir";

class DownloadItem {
  constructor(id, download, extension) {
    this.id = id;
    this.download = download;
    this.extension = extension;
    this.prechange = {};
    this._error = null;
  }

  get url() {
    return this.download.source.url;
  }
  get referrer() {
    const uri = this.download.source.referrerInfo
      ? this.download.source.referrerInfo.originalReferrer
      : null;

    return uri && uri.spec;
  }
  get filename() {
    return this.download.target.path;
  }
  get incognito() {
    return this.download.source.isPrivate;
  }
  get danger() {
    return "safe";
  } // TODO
  get mime() {
    return this.download.contentType;
  }
  get startTime() {
    return this.download.startTime;
  }
  get endTime() {
    return null;
  } // TODO
  get estimatedEndTime() {
    // Based on the code in summarizeDownloads() in DownloadsCommon.jsm
    if (this.download.hasProgress && this.download.speed > 0) {
      let sizeLeft = this.download.totalBytes - this.download.currentBytes;
      let timeLeftInSeconds = sizeLeft / this.download.speed;
      return new Date(Date.now() + timeLeftInSeconds * 1000);
    }
  }
  get state() {
    if (this.download.succeeded) {
      return "complete";
    }
    if (this.download.canceled || this.error) {
      return "interrupted";
    }
    return "in_progress";
  }
  get paused() {
    return (
      this.download.canceled &&
      this.download.hasPartialData &&
      !this.download.error
    );
  }
  get canResume() {
    return (
      (this.download.stopped || this.download.canceled) &&
      this.download.hasPartialData &&
      !this.download.error
    );
  }
  get error() {
    if (this._error) {
      return this._error;
    }
    if (
      !this.download.startTime ||
      !this.download.stopped ||
      this.download.succeeded
    ) {
      return null;
    }
    // TODO store this instead of calculating it

    if (this.download.error) {
      if (this.download.error.becauseSourceFailed) {
        return "NETWORK_FAILED"; // TODO
      }
      if (this.download.error.becauseTargetFailed) {
        return "FILE_FAILED"; // TODO
      }
      return "CRASH";
    }
    return "USER_CANCELED";
  }
  set error(value) {
    this._error = value && value.toString();
  }
  get bytesReceived() {
    return this.download.currentBytes;
  }
  get totalBytes() {
    return this.download.hasProgress ? this.download.totalBytes : -1;
  }
  get fileSize() {
    // todo: this is supposed to be post-compression
    return this.download.succeeded ? this.download.target.size : -1;
  }
  get exists() {
    return this.download.target.exists;
  }
  get byExtensionId() {
    return this.extension ? this.extension.id : undefined;
  }
  get byExtensionName() {
    return this.extension ? this.extension.name : undefined;
  }

  /**
   * Create a cloneable version of this object by pulling all the
   * fields into simple properties (instead of getters).
   *
   * @returns {object} A DownloadItem with flat properties,
   *                   suitable for cloning.
   */
  serialize() {
    let obj = {};
    for (let field of DOWNLOAD_ITEM_FIELDS) {
      obj[field] = this[field];
    }
    for (let field of DOWNLOAD_DATE_FIELDS) {
      if (obj[field]) {
        obj[field] = obj[field].toISOString();
      }
    }
    return obj;
  }

  // When a change event fires, handlers can look at how an individual
  // field changed by comparing item.fieldname with item.prechange.fieldname.
  // After all handlers have been invoked, this gets called to store the
  // current values of all fields ahead of the next event.
  _storePrechange() {
    for (let field of DOWNLOAD_ITEM_CHANGE_FIELDS) {
      this.prechange[field] = this[field];
    }
  }
}

// DownloadMap maps back and forth between the numeric identifiers used in
// the downloads WebExtension API and a Download object from the Downloads jsm.
// TODO Bug 1247794: make id and extension info persistent
const DownloadMap = new (class extends EventEmitter {
  constructor() {
    super();

    this.currentId = 0;
    this.loadPromise = null;

    // Maps numeric id -> DownloadItem
    this.byId = new Map();

    // Maps Download object -> DownloadItem
    this.byDownload = new WeakMap();
  }

  lazyInit() {
    if (this.loadPromise == null) {
      this.loadPromise = Downloads.getList(Downloads.ALL).then(list => {
        let self = this;
        return list
          .addView({
            onDownloadAdded(download) {
              const item = self.newFromDownload(download, null);
              self.emit("create", item);
              item._storePrechange();
            },

            onDownloadRemoved(download) {
              const item = self.byDownload.get(download);
              if (item != null) {
                self.emit("erase", item);
                self.byDownload.delete(download);
                self.byId.delete(item.id);
              }
            },

            onDownloadChanged(download) {
              const item = self.byDownload.get(download);
              if (item == null) {
                Cu.reportError(
                  "Got onDownloadChanged for unknown download object"
                );
              } else {
                self.emit("change", item);
                item._storePrechange();
              }
            },
          })
          .then(() => list.getAll())
          .then(downloads => {
            downloads.forEach(download => {
              this.newFromDownload(download, null);
            });
          })
          .then(() => list);
      });
    }
    return this.loadPromise;
  }

  getDownloadList() {
    return this.lazyInit();
  }

  getAll() {
    return this.lazyInit().then(() => this.byId.values());
  }

  fromId(id, privateAllowed = true) {
    const download = this.byId.get(id);
    if (!download || (!privateAllowed && download.incognito)) {
      throw new Error(`Invalid download id ${id}`);
    }
    return download;
  }

  newFromDownload(download, extension) {
    if (this.byDownload.has(download)) {
      return this.byDownload.get(download);
    }

    const id = ++this.currentId;
    let item = new DownloadItem(id, download, extension);
    this.byId.set(id, item);
    this.byDownload.set(download, item);
    return item;
  }

  erase(item) {
    // TODO Bug 1255507: for now we only work with downloads in the DownloadList
    // from getAll()
    return this.getDownloadList().then(list => {
      list.remove(item.download);
    });
  }
})();

// Create a callable function that filters a DownloadItem based on a
// query object of the type passed to search() or erase().
const downloadQuery = query => {
  let queryTerms = [];
  let queryNegativeTerms = [];
  if (query.query != null) {
    for (let term of query.query) {
      if (term[0] == "-") {
        queryNegativeTerms.push(term.slice(1).toLowerCase());
      } else {
        queryTerms.push(term.toLowerCase());
      }
    }
  }

  function normalizeDownloadTime(arg, before) {
    if (arg == null) {
      return before ? Number.MAX_VALUE : 0;
    }
    return ExtensionCommon.normalizeTime(arg).getTime();
  }

  const startedBefore = normalizeDownloadTime(query.startedBefore, true);
  const startedAfter = normalizeDownloadTime(query.startedAfter, false);
  // const endedBefore = normalizeDownloadTime(query.endedBefore, true);
  // const endedAfter = normalizeDownloadTime(query.endedAfter, false);

  const totalBytesGreater =
    query.totalBytesGreater !== null ? query.totalBytesGreater : -1;
  const totalBytesLess =
    query.totalBytesLess !== null ? query.totalBytesLess : Number.MAX_VALUE;

  // Handle options for which we can have a regular expression and/or
  // an explicit value to match.
  function makeMatch(regex, value, field) {
    if (value == null && regex == null) {
      return input => true;
    }

    let re;
    try {
      re = new RegExp(regex || "", "i");
    } catch (err) {
      throw new Error(`Invalid ${field}Regex: ${err.message}`);
    }
    if (value == null) {
      return input => re.test(input);
    }

    value = value.toLowerCase();
    if (re.test(value)) {
      return input => value == input;
    }
    return input => false;
  }

  const matchFilename = makeMatch(
    query.filenameRegex,
    query.filename,
    "filename"
  );
  const matchUrl = makeMatch(query.urlRegex, query.url, "url");

  return function(item) {
    const url = item.url.toLowerCase();
    const filename = item.filename.toLowerCase();

    if (
      !queryTerms.every(term => url.includes(term) || filename.includes(term))
    ) {
      return false;
    }

    if (
      queryNegativeTerms.some(
        term => url.includes(term) || filename.includes(term)
      )
    ) {
      return false;
    }

    if (!matchFilename(filename) || !matchUrl(url)) {
      return false;
    }

    if (!item.startTime) {
      if (query.startedBefore != null || query.startedAfter != null) {
        return false;
      }
    } else if (
      item.startTime > startedBefore ||
      item.startTime < startedAfter
    ) {
      return false;
    }

    // todo endedBefore, endedAfter

    if (item.totalBytes == -1) {
      if (query.totalBytesGreater !== null || query.totalBytesLess !== null) {
        return false;
      }
    } else if (
      item.totalBytes <= totalBytesGreater ||
      item.totalBytes >= totalBytesLess
    ) {
      return false;
    }

    // todo: include danger
    const SIMPLE_ITEMS = [
      "id",
      "mime",
      "startTime",
      "endTime",
      "state",
      "paused",
      "error",
      "incognito",
      "bytesReceived",
      "totalBytes",
      "fileSize",
      "exists",
    ];
    for (let field of SIMPLE_ITEMS) {
      if (query[field] != null && item[field] != query[field]) {
        return false;
      }
    }

    return true;
  };
};

const queryHelper = query => {
  let matchFn;
  try {
    matchFn = downloadQuery(query);
  } catch (err) {
    return Promise.reject({ message: err.message });
  }

  let compareFn;
  if (query.orderBy != null) {
    const fields = query.orderBy.map(field =>
      field[0] == "-"
        ? { reverse: true, name: field.slice(1) }
        : { reverse: false, name: field }
    );

    for (let field of fields) {
      if (!DOWNLOAD_ITEM_FIELDS.includes(field.name)) {
        return Promise.reject({
          message: `Invalid orderBy field ${field.name}`,
        });
      }
    }

    compareFn = (dl1, dl2) => {
      for (let field of fields) {
        const val1 = dl1[field.name];
        const val2 = dl2[field.name];

        if (val1 < val2) {
          return field.reverse ? 1 : -1;
        } else if (val1 > val2) {
          return field.reverse ? -1 : 1;
        }
      }
      return 0;
    };
  }

  return DownloadMap.getAll().then(downloads => {
    if (compareFn) {
      downloads = Array.from(downloads);
      downloads.sort(compareFn);
    }
    let results = [];
    for (let download of downloads) {
      if (query.limit && results.length >= query.limit) {
        break;
      }
      if (matchFn(download)) {
        results.push(download);
      }
    }
    return results;
  });
};

function downloadEventManagerAPI(context, name, event, listener) {
  let register = fire => {
    const handler = (what, item) => {
      if (context.privateBrowsingAllowed || !item.incognito) {
        listener(fire, what, item);
      }
    };
    let registerPromise = DownloadMap.getDownloadList().then(() => {
      DownloadMap.on(event, handler);
    });
    return () => {
      registerPromise.then(() => {
        DownloadMap.off(event, handler);
      });
    };
  };

  return new EventManager({ context, name, register }).api();
}

this.downloads = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      downloads: {
        download(options) {
          let { filename } = options;
          if (filename && AppConstants.platform === "win") {
            // cross platform javascript code uses "/"
            filename = filename.replace(/\//g, "\\");
          }

          if (filename != null) {
            if (!filename.length) {
              return Promise.reject({ message: "filename must not be empty" });
            }

            let path = OS.Path.split(filename);
            if (path.absolute) {
              return Promise.reject({
                message: "filename must not be an absolute path",
              });
            }

            if (path.components.some(component => component == "..")) {
              return Promise.reject({
                message: "filename must not contain back-references (..)",
              });
            }

            if (
              path.components.some(
                component => component != DownloadPaths.sanitize(component)
              )
            ) {
              return Promise.reject({
                message: "filename must not contain illegal characters",
              });
            }
          }

          if (options.incognito && !context.privateBrowsingAllowed) {
            return Promise.reject({
              message: "private browsing access not allowed",
            });
          }

          if (options.conflictAction == "prompt") {
            // TODO
            return Promise.reject({
              message: "conflictAction prompt not yet implemented",
            });
          }

          if (options.headers) {
            for (let { name } of options.headers) {
              if (
                FORBIDDEN_HEADERS.includes(name.toUpperCase()) ||
                name.match(FORBIDDEN_PREFIXES)
              ) {
                return Promise.reject({
                  message: "Forbidden request header name",
                });
              }
            }
          }

          // Handle method, headers and body options.
          function adjustChannel(channel) {
            if (channel instanceof Ci.nsIHttpChannel) {
              const method = options.method || "GET";
              channel.requestMethod = method;

              if (options.headers) {
                for (let { name, value } of options.headers) {
                  channel.setRequestHeader(name, value, false);
                }
              }

              if (options.body != null) {
                const stream = Cc[
                  "@mozilla.org/io/string-input-stream;1"
                ].createInstance(Ci.nsIStringInputStream);
                stream.setData(options.body, options.body.length);

                channel.QueryInterface(Ci.nsIUploadChannel2);
                channel.explicitSetUploadStream(
                  stream,
                  null,
                  -1,
                  method,
                  false
                );
              }
            }
            return Promise.resolve();
          }

          function allowHttpStatus(download, status) {
            const item = DownloadMap.byDownload.get(download);
            if (item === null) {
              return true;
            }

            let error = null;
            switch (status) {
              case 204: // No Content
              case 205: // Reset Content
              case 404: // Not Found
                error = "SERVER_BAD_CONTENT";
                break;

              case 403: // Forbidden
                error = "SERVER_FORBIDDEN";
                break;

              case 402: // Unauthorized
              case 407: // Proxy authentication required
                error = "SERVER_UNAUTHORIZED";
                break;

              default:
                if (status >= 400) {
                  error = "SERVER_FAILED";
                }
                break;
            }

            if (error) {
              item.error = error;
              return false;
            }

            // No error, ergo allow the request.
            return true;
          }

          async function createTarget(downloadsDir) {
            if (!filename) {
              let uri = Services.io.newURI(options.url);
              if (uri instanceof Ci.nsIURL) {
                filename = DownloadPaths.sanitize(
                  Services.textToSubURI.unEscapeURIForUI(uri.fileName)
                );
              }
            }

            let target = OS.Path.join(downloadsDir, filename || "download");

            let saveAs;
            if (options.saveAs !== null) {
              saveAs = options.saveAs;
            } else {
              // If options.saveAs was not specified, only show the file chooser
              // if |browser.download.useDownloadDir == false|. That is to say,
              // only show the file chooser if Firefox normally shows it when
              // a file is downloaded.
              saveAs = !Services.prefs.getBoolPref(
                PROMPTLESS_DOWNLOAD_PREF,
                true
              );
            }

            // Create any needed subdirectories if required by filename.
            const dir = OS.Path.dirname(target);
            await OS.File.makeDir(dir, { from: downloadsDir });

            if (await OS.File.exists(target)) {
              // This has a race, something else could come along and create
              // the file between this test and them time the download code
              // creates the target file.  But we can't easily fix it without
              // modifying DownloadCore so we live with it for now.
              switch (options.conflictAction) {
                case "uniquify":
                default:
                  target = DownloadPaths.createNiceUniqueFile(
                    new FileUtils.File(target)
                  ).path;
                  if (saveAs) {
                    // createNiceUniqueFile actually creates the file, which
                    // is premature if we need to show a SaveAs dialog.
                    await OS.File.remove(target);
                  }
                  break;

                case "overwrite":
                  break;
              }
            }

            if (!saveAs || AppConstants.platform === "android") {
              return target;
            }

            if (!("windowTracker" in global)) {
              return target;
            }

            // Use windowTracker to find a window, rather than Services.wm,
            // so that this doesn't break where navigator:browser isn't the
            // main window (e.g. Thunderbird).
            const window = global.windowTracker.getTopWindow().window;
            const basename = OS.Path.basename(target);
            const ext = basename.match(/\.([^.]+)$/);

            // Setup the file picker Save As dialog.
            const picker = Cc["@mozilla.org/filepicker;1"].createInstance(
              Ci.nsIFilePicker
            );
            picker.init(window, null, Ci.nsIFilePicker.modeSave);
            picker.displayDirectory = new FileUtils.File(dir);
            picker.appendFilters(Ci.nsIFilePicker.filterAll);
            picker.defaultString = basename;

            // Configure a default file extension, used as fallback on Windows.
            picker.defaultExtension = ext && ext[1];

            // Open the dialog and resolve/reject with the result.
            return new Promise((resolve, reject) => {
              picker.open(result => {
                if (result === Ci.nsIFilePicker.returnCancel) {
                  reject({ message: "Download canceled by the user" });
                } else {
                  resolve(picker.file.path);
                }
              });
            });
          }

          let download;
          return Downloads.getPreferredDownloadsDirectory()
            .then(downloadsDir => createTarget(downloadsDir))
            .then(target => {
              const source = {
                url: options.url,
                isPrivate: options.incognito,
              };

              // Unless the API user explicitly wants errors ignored,
              // set the allowHttpStatus callback, which will instruct
              // DownloadCore to cancel downloads on HTTP errors.
              if (!options.allowHttpErrors) {
                source.allowHttpStatus = allowHttpStatus;
              }

              if (options.method || options.headers || options.body) {
                source.adjustChannel = adjustChannel;
              }

              return Downloads.createDownload({
                source,
                target: {
                  path: target,
                  partFilePath: target + ".part",
                },
              });
            })
            .then(dl => {
              download = dl;
              return DownloadMap.getDownloadList();
            })
            .then(list => {
              const item = DownloadMap.newFromDownload(download, extension);
              list.add(download);

              // This is necessary to make pause/resume work.
              download.tryToKeepPartialData = true;

              // Do not handle errors.
              // Extensions will use listeners to be informed about errors.
              // Just ignore any errors from |start()| to avoid spamming the
              // error console.
              download.start().catch(() => {});

              return item.id;
            });
        },

        removeFile(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id, context.privateBrowsingAllowed);
            } catch (err) {
              return Promise.reject({ message: `Invalid download id ${id}` });
            }
            if (item.state !== "complete") {
              return Promise.reject({
                message: `Cannot remove incomplete download id ${id}`,
              });
            }
            return OS.File.remove(item.filename, { ignoreAbsent: false }).catch(
              err => {
                return Promise.reject({
                  message: `Could not remove download id ${item.id} because the file doesn't exist`,
                });
              }
            );
          });
        },

        search(query) {
          if (!context.privateBrowsingAllowed) {
            query.incognito = false;
          }
          return queryHelper(query).then(items =>
            items.map(item => item.serialize())
          );
        },

        pause(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id, context.privateBrowsingAllowed);
            } catch (err) {
              return Promise.reject({ message: `Invalid download id ${id}` });
            }
            if (item.state != "in_progress") {
              return Promise.reject({
                message: `Download ${id} cannot be paused since it is in state ${item.state}`,
              });
            }

            return item.download.cancel();
          });
        },

        resume(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id, context.privateBrowsingAllowed);
            } catch (err) {
              return Promise.reject({ message: `Invalid download id ${id}` });
            }
            if (!item.canResume) {
              return Promise.reject({
                message: `Download ${id} cannot be resumed`,
              });
            }

            item.error = null;
            return item.download.start();
          });
        },

        cancel(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id, context.privateBrowsingAllowed);
            } catch (err) {
              return Promise.reject({ message: `Invalid download id ${id}` });
            }
            if (item.download.succeeded) {
              return Promise.reject({
                message: `Download ${id} is already complete`,
              });
            }
            return item.download.finalize(true);
          });
        },

        showDefaultFolder() {
          Downloads.getPreferredDownloadsDirectory()
            .then(dir => {
              let dirobj = new FileUtils.File(dir);
              if (dirobj.isDirectory()) {
                dirobj.launch();
              } else {
                throw new Error(
                  `Download directory ${dirobj.path} is not actually a directory`
                );
              }
            })
            .catch(Cu.reportError);
        },

        erase(query) {
          if (!context.privateBrowsingAllowed) {
            query.incognito = false;
          }
          return queryHelper(query).then(items => {
            let results = [];
            let promises = [];
            for (let item of items) {
              promises.push(DownloadMap.erase(item));
              results.push(item.id);
            }
            return Promise.all(promises).then(() => results);
          });
        },

        open(downloadId) {
          return DownloadMap.lazyInit()
            .then(() => {
              let download = DownloadMap.fromId(
                downloadId,
                context.privateBrowsingAllowed
              ).download;
              if (download.succeeded) {
                return download.launch();
              }
              return Promise.reject({ message: "Download has not completed." });
            })
            .catch(error => {
              return Promise.reject({ message: error.message });
            });
        },

        show(downloadId) {
          return DownloadMap.lazyInit()
            .then(() => {
              let download = DownloadMap.fromId(
                downloadId,
                context.privateBrowsingAllowed
              );
              return download.download.showContainingDirectory();
            })
            .then(() => {
              return true;
            })
            .catch(error => {
              return Promise.reject({ message: error.message });
            });
        },

        getFileIcon(downloadId, options) {
          return DownloadMap.lazyInit()
            .then(() => {
              let size = options && options.size ? options.size : 32;
              let download = DownloadMap.fromId(
                downloadId,
                context.privateBrowsingAllowed
              ).download;
              let pathPrefix = "";
              let path;

              if (download.succeeded) {
                let file = FileUtils.File(download.target.path);
                path = Services.io.newFileURI(file).spec;
              } else {
                path = OS.Path.basename(download.target.path);
                pathPrefix = "//";
              }

              return new Promise((resolve, reject) => {
                let chromeWebNav = Services.appShell.createWindowlessBrowser(
                  true
                );
                let system = Services.scriptSecurityManager.getSystemPrincipal();
                chromeWebNav.docShell.createAboutBlankContentViewer(
                  system,
                  system
                );

                let img = chromeWebNav.document.createElement("img");
                img.width = size;
                img.height = size;

                let handleLoad;
                let handleError;
                const cleanup = () => {
                  img.removeEventListener("load", handleLoad);
                  img.removeEventListener("error", handleError);
                  chromeWebNav.close();
                  chromeWebNav = null;
                };

                handleLoad = () => {
                  let canvas = chromeWebNav.document.createElement("canvas");
                  canvas.width = size;
                  canvas.height = size;
                  let context = canvas.getContext("2d");
                  context.drawImage(img, 0, 0, size, size);
                  let dataURL = canvas.toDataURL("image/png");
                  cleanup();
                  resolve(dataURL);
                };

                handleError = error => {
                  Cu.reportError(error);
                  cleanup();
                  reject(new Error("An unexpected error occurred"));
                };

                img.addEventListener("load", handleLoad);
                img.addEventListener("error", handleError);
                img.src = `moz-icon:${pathPrefix}${path}?size=${size}`;
              });
            })
            .catch(error => {
              return Promise.reject({ message: error.message });
            });
        },

        // When we do setShelfEnabled(), check for additional "downloads.shelf" permission.
        // i.e.:
        // setShelfEnabled(enabled) {
        //   if (!extension.hasPermission("downloads.shelf")) {
        //     throw new context.cloneScope.Error("Permission denied because 'downloads.shelf' permission is missing.");
        //   }
        //   ...
        // }

        onChanged: downloadEventManagerAPI(
          context,
          "downloads.onChanged",
          "change",
          (fire, what, item) => {
            let changes = {};
            const noundef = val => (val === undefined ? null : val);
            DOWNLOAD_ITEM_CHANGE_FIELDS.forEach(fld => {
              if (item[fld] != item.prechange[fld]) {
                changes[fld] = {
                  previous: noundef(item.prechange[fld]),
                  current: noundef(item[fld]),
                };
              }
            });
            if (Object.keys(changes).length) {
              changes.id = item.id;
              fire.async(changes);
            }
          }
        ),

        onCreated: downloadEventManagerAPI(
          context,
          "downloads.onCreated",
          "create",
          (fire, what, item) => {
            fire.async(item.serialize());
          }
        ),

        onErased: downloadEventManagerAPI(
          context,
          "downloads.onErased",
          "erase",
          (fire, what, item) => {
            fire.async(item.id);
          }
        ),

        onDeterminingFilename: ignoreEvent(
          context,
          "downloads.onDeterminingFilename"
        ),
      },
    };
  }
};
