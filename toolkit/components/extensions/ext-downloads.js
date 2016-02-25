"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
const {
  ignoreEvent,
} = ExtensionUtils;

let currentId = 0;

extensions.registerSchemaAPI("downloads", "downloads", (extension, context) => {
  return {
    downloads: {
      download(options) {
        if (options.filename != null) {
          if (options.filename.length == 0) {
            return Promise.reject({message: "filename must not be empty"});
          }

          let path = OS.Path.split(options.filename);
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

        function createTarget(downloadsDir) {
          // TODO
          // if (options.saveAs) { }

          let target;
          if (options.filename) {
            target = OS.Path.join(downloadsDir, options.filename);
          } else {
            let uri = NetUtil.newURI(options.url).QueryInterface(Ci.nsIURL);
            target = OS.Path.join(downloadsDir, uri.fileName);
          }

          // This has a race, something else could come along and create
          // the file between this test and them time the download code
          // creates the target file.  But we can't easily fix it without
          // modifying DownloadCore so we live with it for now.
          return OS.File.exists(target).then(exists => {
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
            return target;
          });
        }

        let download;
        return Downloads.getPreferredDownloadsDirectory()
          .then(downloadsDir => createTarget(downloadsDir))
          .then(target => Downloads.createDownload({
            source: options.url,
            target: target,
          })).then(dl => {
            download = dl;
            return Downloads.getList(Downloads.ALL);
          }).then(list => {
            list.add(download);

            // This is necessary to make pause/resume work.
            download.tryToKeepPartialData = true;
            download.start();

            // Without other chrome.downloads methods, we can't actually
            // do anything with the id so just return a dummy value for now.
            return currentId++;
          });
      },

      // When we do open(), check for additional downloads.open permission.
      // i.e.:
      // open(downloadId) {
      //   if (!extension.hasPermission("downloads.open")) {
      //     throw new context.cloneScope.Error("Permission denied because 'downloads.open' permission is missing.");
      //   }
      //   ...
      // }
      // likewise for setShelfEnabled() and the "download.shelf" permission

      onCreated: ignoreEvent(context, "downloads.onCreated"),
      onErased: ignoreEvent(context, "downloads.onErased"),
      onChanged: ignoreEvent(context, "downloads.onChanged"),
      onDeterminingFilename: ignoreEvent(context, "downloads.onDeterminingFilename"),
    },
  };
});
