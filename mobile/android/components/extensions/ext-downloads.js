/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "DownloadPaths",
  "resource://gre/modules/DownloadPaths.jsm"
);

var { ignoreEvent } = ExtensionCommon;

const REQUEST_DOWNLOAD_MESSAGE = "GeckoView:WebExtension:Download";

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

const State = {
  IN_PROGRESS: "in_progress",
  INTERRUPTED: "interrupted",
  COMPLETE: "complete",
};

// TODO Bug 1247794: make id and extension info persistent
class DownloadItem {
  constructor(downloadInfo, options, extension) {
    this.id = downloadInfo.id;
    this.url = options.url;
    this.referrer = downloadInfo.referrer || "";
    this.filename = options.filename || "";
    this.incognito = options.incognito;
    this.danger = downloadInfo.danger || "safe"; // todo; not implemented in toolkit either
    this.mime = downloadInfo.mime || "";
    this.startTime = Date.now().toString();
    this.state = State.IN_PROGRESS;
    this.paused = false;
    this.canResume = false;
    this.bytesReceived = 0;
    this.totalBytes = -1;
    this.fileSize = -1;
    this.exists = downloadInfo.exists || false;
    this.byExtensionId = extension?.id;
    this.byExtensionName = extension?.name;
  }
}

this.downloads = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;
    return {
      downloads: {
        download(options) {
          // the validation checks should be kept in sync with the toolkit implementation
          const { filename } = options;
          if (filename != null) {
            if (!filename.length) {
              return Promise.reject({ message: "filename must not be empty" });
            }

            const path = OS.Path.split(filename);
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
              path.components.some(component => {
                const sanitized = DownloadPaths.sanitize(component, {
                  compressWhitespaces: false,
                });
                return component != sanitized;
              })
            ) {
              return Promise.reject({
                message: "filename must not contain illegal characters",
              });
            }
          }

          if (options.incognito && !context.privateBrowsingAllowed) {
            return Promise.reject({
              message: "Private browsing access not allowed",
            });
          }

          if (options.headers) {
            for (const { name } of options.headers) {
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

          return EventDispatcher.instance
            .sendRequestForResult({
              type: REQUEST_DOWNLOAD_MESSAGE,
              options,
              extensionId: extension.id,
            })
            .then(value => {
              const downloadItem = new DownloadItem(
                { id: value },
                options,
                extension
              );
              return downloadItem.id;
            });
        },

        removeFile(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        search(query) {
          throw new ExtensionError("Not implemented");
        },

        pause(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        resume(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        cancel(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        showDefaultFolder() {
          throw new ExtensionError("Not implemented");
        },

        erase(query) {
          throw new ExtensionError("Not implemented");
        },

        open(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        show(downloadId) {
          throw new ExtensionError("Not implemented");
        },

        getFileIcon(downloadId, options) {
          throw new ExtensionError("Not implemented");
        },

        onChanged: ignoreEvent(context, "downloads.onChanged"),

        onCreated: ignoreEvent(context, "downloads.onCreated"),

        onErased: ignoreEvent(context, "downloads.onErased"),

        onDeterminingFilename: ignoreEvent(
          context,
          "downloads.onDeterminingFilename"
        ),
      },
    };
  }
};
