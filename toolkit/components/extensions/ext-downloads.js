"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://gre/modules/EventEmitter.jsm");

var {
  ignoreEvent,
  normalizeTime,
  PlatformInfo,
} = ExtensionUtils;

const DOWNLOAD_ITEM_FIELDS = ["id", "url", "referrer", "filename", "incognito",
                              "danger", "mime", "startTime", "endTime",
                              "estimatedEndTime", "state",
                              "paused", "canResume", "error",
                              "bytesReceived", "totalBytes",
                              "fileSize", "exists",
                              "byExtensionId", "byExtensionName"];

// Fields that we generate onChanged events for.
const DOWNLOAD_ITEM_CHANGE_FIELDS = ["endTime", "state", "paused", "canResume",
                                     "error", "exists"];

// From https://fetch.spec.whatwg.org/#forbidden-header-name
const FORBIDDEN_HEADERS = ["ACCEPT-CHARSET", "ACCEPT-ENCODING",
                           "ACCESS-CONTROL-REQUEST-HEADERS", "ACCESS-CONTROL-REQUEST-METHOD",
                           "CONNECTION", "CONTENT-LENGTH", "COOKIE", "COOKIE2", "DATE", "DNT",
                           "EXPECT", "HOST", "KEEP-ALIVE", "ORIGIN", "REFERER", "TE", "TRAILER",
                           "TRANSFER-ENCODING", "UPGRADE", "VIA"];

const FORBIDDEN_PREFIXES = /^PROXY-|^SEC-/i;

class DownloadItem {
  constructor(id, download, extension) {
    this.id = id;
    this.download = download;
    this.extension = extension;
    this.prechange = {};
  }

  get url() { return this.download.source.url; }
  get referrer() { return this.download.source.referrer; }
  get filename() { return this.download.target.path; }
  get incognito() { return this.download.source.isPrivate; }
  get danger() { return "safe"; } // TODO
  get mime() { return this.download.contentType; }
  get startTime() { return this.download.startTime; }
  get endTime() { return null; } // TODO
  get estimatedEndTime() { return null; } // TODO
  get state() {
    if (this.download.succeeded) {
      return "complete";
    }
    if (this.download.canceled) {
      return "interrupted";
    }
    return "in_progress";
  }
  get paused() {
    return this.download.canceled && this.download.hasPartialData && !this.download.error;
  }
  get canResume() {
    return (this.download.stopped || this.download.canceled) &&
      this.download.hasPartialData && !this.download.error;
  }
  get error() {
    if (!this.download.stopped || this.download.succeeded) {
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
  get exists() { return this.download.target.exists; }
  get byExtensionId() { return this.extension ? this.extension.id : undefined; }
  get byExtensionName() { return this.extension ? this.extension.name : undefined; }

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
    if (obj.startTime) {
      obj.startTime = obj.startTime.toISOString();
    }
    return obj;
  }

  // When a change event fires, handlers can look at how an individual
  // field changed by comparing item.fieldname with item.prechange.fieldname.
  // After all handlers have been invoked, this gets called to store the
  // current values of all fields ahead of the next event.
  _change() {
    for (let field of DOWNLOAD_ITEM_CHANGE_FIELDS) {
      this.prechange[field] = this[field];
    }
  }
}


// DownloadMap maps back and forth betwen the numeric identifiers used in
// the downloads WebExtension API and a Download object from the Downloads jsm.
// todo: make id and extension info persistent (bug 1247794)
const DownloadMap = {
  currentId: 0,
  loadPromise: null,

  // Maps numeric id -> DownloadItem
  byId: new Map(),

  // Maps Download object -> DownloadItem
  byDownload: new WeakMap(),

  lazyInit() {
    if (this.loadPromise == null) {
      EventEmitter.decorate(this);
      this.loadPromise = Downloads.getList(Downloads.ALL).then(list => {
        let self = this;
        return list.addView({
          onDownloadAdded(download) {
            const item = self.newFromDownload(download, null);
            self.emit("create", item);
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
              Cu.reportError("Got onDownloadChanged for unknown download object");
            } else {
              // We get the first one of these when the download is started.
              // In this case, don't emit anything, just initialize prechange.
              if (Object.keys(item.prechange).length > 0) {
                self.emit("change", item);
              }
              item._change();
            }
          },
        }).then(() => list.getAll())
          .then(downloads => {
            downloads.forEach(download => {
              this.newFromDownload(download, null);
            });
          })
          .then(() => list);
      });
    }
    return this.loadPromise;
  },

  getDownloadList() {
    return this.lazyInit();
  },

  getAll() {
    return this.lazyInit().then(() => this.byId.values());
  },

  fromId(id) {
    const download = this.byId.get(id);
    if (!download) {
      throw new Error(`Invalid download id ${id}`);
    }
    return download;
  },

  newFromDownload(download, extension) {
    if (this.byDownload.has(download)) {
      return this.byDownload.get(download);
    }

    const id = ++this.currentId;
    let item = new DownloadItem(id, download, extension);
    this.byId.set(id, item);
    this.byDownload.set(download, item);
    return item;
  },

  erase(item) {
    // This will need to get more complicated for bug 1255507 but for now we
    // only work with downloads in the DownloadList from getAll()
    return this.getDownloadList().then(list => {
      list.remove(item.download);
    });
  },
};

// Create a callable function that filters a DownloadItem based on a
// query object of the type passed to search() or erase().
function downloadQuery(query) {
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
    return normalizeTime(arg).getTime();
  }

  const startedBefore = normalizeDownloadTime(query.startedBefore, true);
  const startedAfter = normalizeDownloadTime(query.startedAfter, false);
  // const endedBefore = normalizeDownloadTime(query.endedBefore, true);
  // const endedAfter = normalizeDownloadTime(query.endedAfter, false);

  const totalBytesGreater = query.totalBytesGreater || 0;
  const totalBytesLess = (query.totalBytesLess != null)
        ? query.totalBytesLess : Number.MAX_VALUE;

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
      return input => (value == input);
    }
    return input => false;
  }

  const matchFilename = makeMatch(query.filenameRegex, query.filename, "filename");
  const matchUrl = makeMatch(query.urlRegex, query.url, "url");

  return function(item) {
    const url = item.url.toLowerCase();
    const filename = item.filename.toLowerCase();

    if (!queryTerms.every(term => url.includes(term) || filename.includes(term))) {
      return false;
    }

    if (queryNegativeTerms.some(term => url.includes(term) || filename.includes(term))) {
      return false;
    }

    if (!matchFilename(filename) || !matchUrl(url)) {
      return false;
    }

    if (!item.startTime) {
      if (query.startedBefore != null || query.startedAfter != null) {
        return false;
      }
    } else if (item.startTime > startedBefore || item.startTime < startedAfter) {
      return false;
    }

    // todo endedBefore, endedAfter

    if (item.totalBytes == -1) {
      if (query.totalBytesGreater != null || query.totalBytesLess != null) {
        return false;
      }
    } else if (item.totalBytes <= totalBytesGreater || item.totalBytes >= totalBytesLess) {
      return false;
    }

    // todo: include danger
    const SIMPLE_ITEMS = ["id", "mime", "startTime", "endTime", "state",
                          "paused", "error",
                          "bytesReceived", "totalBytes", "fileSize", "exists"];
    for (let field of SIMPLE_ITEMS) {
      if (query[field] != null && item[field] != query[field]) {
        return false;
      }
    }

    return true;
  };
}

function queryHelper(query) {
  let matchFn;
  try {
    matchFn = downloadQuery(query);
  } catch (err) {
    return Promise.reject({message: err.message});
  }

  let compareFn;
  if (query.orderBy != null) {
    const fields = query.orderBy.map(field => field[0] == "-"
                                     ? {reverse: true, name: field.slice(1)}
                                     : {reverse: false, name: field});

    for (let field of fields) {
      if (!DOWNLOAD_ITEM_FIELDS.includes(field.name)) {
        return Promise.reject({message: `Invalid orderBy field ${field.name}`});
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
}

this.downloads = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      downloads: {
        download(options) {
          let {filename} = options;
          if (filename && PlatformInfo.os === "win") {
            // cross platform javascript code uses "/"
            filename = filename.replace(/\//g, "\\");
          }

          if (filename != null) {
            if (filename.length == 0) {
              return Promise.reject({message: "filename must not be empty"});
            }

            let path = OS.Path.split(filename);
            if (path.absolute) {
              return Promise.reject({message: "filename must not be an absolute path"});
            }

            if (path.components.some(component => component == "..")) {
              return Promise.reject({message: "filename must not contain back-references (..)"});
            }
          }

          if (options.conflictAction == "prompt") {
            // TODO
            return Promise.reject({message: "conflictAction prompt not yet implemented"});
          }

          if (options.headers) {
            for (let {name} of options.headers) {
              if (FORBIDDEN_HEADERS.includes(name.toUpperCase()) || name.match(FORBIDDEN_PREFIXES)) {
                return Promise.reject({message: "Forbidden request header name"});
              }
            }
          }

          // Handle method, headers and body options.
          function adjustChannel(channel) {
            if (channel instanceof Ci.nsIHttpChannel) {
              const method = options.method || "GET";
              channel.requestMethod = method;

              if (options.headers) {
                for (let {name, value} of options.headers) {
                  channel.setRequestHeader(name, value, false);
                }
              }

              if (options.body != null) {
                const stream = Cc["@mozilla.org/io/string-input-stream;1"]
                               .createInstance(Ci.nsIStringInputStream);
                stream.setData(options.body, options.body.length);

                channel.QueryInterface(Ci.nsIUploadChannel2);
                channel.explicitSetUploadStream(stream, null, -1, method, false);
              }
            }
            return Promise.resolve();
          }

          function createTarget(downloadsDir) {
            let target;
            if (filename) {
              target = OS.Path.join(downloadsDir, filename);
            } else {
              let uri = NetUtil.newURI(options.url);

              let remote = "download";
              if (uri instanceof Ci.nsIURL) {
                remote = uri.fileName;
              }
              target = OS.Path.join(downloadsDir, remote);
            }

            // Create any needed subdirectories if required by filename.
            const dir = OS.Path.dirname(target);
            return OS.File.makeDir(dir, {from: downloadsDir}).then(() => {
              return OS.File.exists(target);
            }).then(exists => {
              // This has a race, something else could come along and create
              // the file between this test and them time the download code
              // creates the target file.  But we can't easily fix it without
              // modifying DownloadCore so we live with it for now.
              if (exists) {
                switch (options.conflictAction) {
                  case "uniquify":
                  default:
                    target = DownloadPaths.createNiceUniqueFile(new FileUtils.File(target)).path;
                    break;

                  case "overwrite":
                    break;
                }
              }
            }).then(() => {
              if (!options.saveAs) {
                return Promise.resolve(target);
              }

              // Setup the file picker Save As dialog.
              const picker = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
              const window = Services.wm.getMostRecentWindow("navigator:browser");
              picker.init(window, null, Ci.nsIFilePicker.modeSave);
              picker.displayDirectory = new FileUtils.File(dir);
              picker.appendFilters(Ci.nsIFilePicker.filterAll);
              picker.defaultString = OS.Path.basename(target);

              // Open the dialog and resolve/reject with the result.
              return new Promise((resolve, reject) => {
                picker.open(result => {
                  if (result === Ci.nsIFilePicker.returnCancel) {
                    reject({message: "Download canceled by the user"});
                  } else {
                    resolve(picker.file.path);
                  }
                });
              });
            });
          }

          let download;
          return Downloads.getPreferredDownloadsDirectory()
            .then(downloadsDir => createTarget(downloadsDir))
            .then(target => {
              const source = {
                url: options.url,
              };

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
            }).then(dl => {
              download = dl;
              return DownloadMap.getDownloadList();
            }).then(list => {
              list.add(download);

              // This is necessary to make pause/resume work.
              download.tryToKeepPartialData = true;
              download.start();

              const item = DownloadMap.newFromDownload(download, extension);
              return item.id;
            });
        },

        removeFile(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id);
            } catch (err) {
              return Promise.reject({message: `Invalid download id ${id}`});
            }
            if (item.state !== "complete") {
              return Promise.reject({message: `Cannot remove incomplete download id ${id}`});
            }
            return OS.File.remove(item.filename, {ignoreAbsent: false}).catch((err) => {
              return Promise.reject({message: `Could not remove download id ${item.id} because the file doesn't exist`});
            });
          });
        },

        search(query) {
          return queryHelper(query)
            .then(items => items.map(item => item.serialize()));
        },

        pause(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id);
            } catch (err) {
              return Promise.reject({message: `Invalid download id ${id}`});
            }
            if (item.state != "in_progress") {
              return Promise.reject({message: `Download ${id} cannot be paused since it is in state ${item.state}`});
            }

            return item.download.cancel();
          });
        },

        resume(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id);
            } catch (err) {
              return Promise.reject({message: `Invalid download id ${id}`});
            }
            if (!item.canResume) {
              return Promise.reject({message: `Download ${id} cannot be resumed`});
            }

            return item.download.start();
          });
        },

        cancel(id) {
          return DownloadMap.lazyInit().then(() => {
            let item;
            try {
              item = DownloadMap.fromId(id);
            } catch (err) {
              return Promise.reject({message: `Invalid download id ${id}`});
            }
            if (item.download.succeeded) {
              return Promise.reject({message: `Download ${id} is already complete`});
            }
            return item.download.finalize(true);
          });
        },

        showDefaultFolder() {
          Downloads.getPreferredDownloadsDirectory().then(dir => {
            let dirobj = new FileUtils.File(dir);
            if (dirobj.isDirectory()) {
              dirobj.launch();
            } else {
              throw new Error(`Download directory ${dirobj.path} is not actually a directory`);
            }
          }).catch(Cu.reportError);
        },

        erase(query) {
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
          return DownloadMap.lazyInit().then(() => {
            let download = DownloadMap.fromId(downloadId).download;
            if (download.succeeded) {
              return download.launch();
            }
            return Promise.reject({message: "Download has not completed."});
          }).catch((error) => {
            return Promise.reject({message: error.message});
          });
        },

        show(downloadId) {
          return DownloadMap.lazyInit().then(() => {
            let download = DownloadMap.fromId(downloadId);
            return download.download.showContainingDirectory();
          }).then(() => {
            return true;
          }).catch(error => {
            return Promise.reject({message: error.message});
          });
        },

        getFileIcon(downloadId, options) {
          return DownloadMap.lazyInit().then(() => {
            let size = options && options.size ? options.size : 32;
            let download = DownloadMap.fromId(downloadId).download;
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
              let chromeWebNav = Services.appShell.createWindowlessBrowser(true);
              chromeWebNav
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIDocShell)
                .createAboutBlankContentViewer(Services.scriptSecurityManager.getSystemPrincipal());

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

              handleError = (error) => {
                Cu.reportError(error);
                cleanup();
                reject(new Error("An unexpected error occurred"));
              };

              img.addEventListener("load", handleLoad);
              img.addEventListener("error", handleError);
              img.src = `moz-icon:${pathPrefix}${path}?size=${size}`;
            });
          }).catch((error) => {
            return Promise.reject({message: error.message});
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

        onChanged: new SingletonEventManager(context, "downloads.onChanged", fire => {
          const handler = (what, item) => {
            let changes = {};
            const noundef = val => (val === undefined) ? null : val;
            DOWNLOAD_ITEM_CHANGE_FIELDS.forEach(fld => {
              if (item[fld] != item.prechange[fld]) {
                changes[fld] = {
                  previous: noundef(item.prechange[fld]),
                  current: noundef(item[fld]),
                };
              }
            });
            if (Object.keys(changes).length > 0) {
              changes.id = item.id;
              fire.async(changes);
            }
          };

          let registerPromise = DownloadMap.getDownloadList().then(() => {
            DownloadMap.on("change", handler);
          });
          return () => {
            registerPromise.then(() => {
              DownloadMap.off("change", handler);
            });
          };
        }).api(),

        onCreated: new SingletonEventManager(context, "downloads.onCreated", fire => {
          const handler = (what, item) => {
            fire.async(item.serialize());
          };
          let registerPromise = DownloadMap.getDownloadList().then(() => {
            DownloadMap.on("create", handler);
          });
          return () => {
            registerPromise.then(() => {
              DownloadMap.off("create", handler);
            });
          };
        }).api(),

        onErased: new SingletonEventManager(context, "downloads.onErased", fire => {
          const handler = (what, item) => {
            fire.async(item.id);
          };
          let registerPromise = DownloadMap.getDownloadList().then(() => {
            DownloadMap.on("erase", handler);
          });
          return () => {
            registerPromise.then(() => {
              DownloadMap.off("erase", handler);
            });
          };
        }).api(),

        onDeterminingFilename: ignoreEvent(context, "downloads.onDeterminingFilename"),
      },
    };
  }
};
