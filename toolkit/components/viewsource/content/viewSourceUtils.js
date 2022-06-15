// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * To keep the global namespace safe, don't define global variables and
 * functions in this file.
 *
 * This file silently depends on contentAreaUtils.js for getDefaultFileName
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var gViewSourceUtils = {
  mnsIWebBrowserPersist: Ci.nsIWebBrowserPersist,
  mnsIWebProgress: Ci.nsIWebProgress,
  mnsIWebPageDescriptor: Ci.nsIWebPageDescriptor,

  // Get the ViewSource actor for a browsing context.
  getViewSourceActor(aBrowsingContext) {
    return aBrowsingContext.currentWindowGlobal.getActor("ViewSource");
  },

  /**
   * Get the ViewSourcePage actor.
   * @param object An object with `browsingContext` field
   */
  getPageActor({ browsingContext }) {
    return browsingContext.currentWindowGlobal.getActor("ViewSourcePage");
  },

  /**
   * Opens the view source window.
   *
   * @param aArgs (required)
   *        This Object can include the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   */
  async viewSource(aArgs) {
    // Check if external view source is enabled.  If so, try it.  If it fails,
    // fallback to internal view source.
    if (Services.prefs.getBoolPref("view_source.editor.external")) {
      try {
        await this.openInExternalEditor(aArgs);
        return;
      } catch (data) {}
    }
    // Try existing browsers first.
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWin && browserWin.BrowserViewSourceOfDocument) {
      browserWin.BrowserViewSourceOfDocument(aArgs);
      return;
    }
    // No browser window created yet, try to create one.
    let utils = this;
    Services.ww.registerNotification(function onOpen(win, topic) {
      if (
        win.document.documentURI !== "about:blank" ||
        topic !== "domwindowopened"
      ) {
        return;
      }
      Services.ww.unregisterNotification(onOpen);
      win.addEventListener(
        "load",
        () => {
          aArgs.viewSourceBrowser = win.gBrowser.selectedTab.linkedBrowser;
          utils.viewSourceInBrowser(aArgs);
        },
        { once: true }
      );
    });
    window.top.openWebLinkIn("about:blank", "current");
  },

  /**
   * Displays view source in the provided <browser>.  This allows for non-window
   * display methods, such as a tab from Firefox.
   *
   * @param aArgs
   *        An object with the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        viewSourceBrowser (required):
   *          The browser to display the view source in.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   */
  viewSourceInBrowser({
    URL,
    viewSourceBrowser,
    browser,
    outerWindowID,
    lineNumber,
  }) {
    if (!URL) {
      throw new Error("Must supply a URL when opening view source.");
    }

    if (browser) {
      // If we're dealing with a remote browser, then the browser
      // for view source needs to be remote as well.
      if (viewSourceBrowser.remoteType != browser.remoteType) {
        // In this base case, where we are handed a <browser> someone else is
        // managing, we don't know for sure that it's safe to toggle remoteness.
        // For view source in a window, this is overridden to actually do the
        // flip if needed.
        throw new Error("View source browser's remoteness mismatch");
      }
    } else if (outerWindowID) {
      throw new Error("Must supply the browser if passing the outerWindowID");
    }

    let viewSourceActor = this.getViewSourceActor(
      viewSourceBrowser.browsingContext
    );
    viewSourceActor.sendAsyncMessage("ViewSource:LoadSource", {
      URL,
      outerWindowID,
      lineNumber,
    });
  },

  /**
   * Displays view source for a selection from some document in the provided
   * <browser>.  This allows for non-window display methods, such as a tab from
   * Firefox.
   *
   * @param aBrowsingContext:
   *        The child browsing context containing the document to view the source of.
   * @param aGetBrowserFn
   *        A function that will return a browser to open the source in.
   */
  async viewPartialSourceInBrowser(aBrowsingContext, aGetBrowserFn) {
    let sourceActor = this.getViewSourceActor(aBrowsingContext);
    if (sourceActor) {
      let data = await sourceActor.sendQuery("ViewSource:GetSelection", {});

      let targetActor = this.getViewSourceActor(
        aGetBrowserFn().browsingContext
      );
      targetActor.sendAsyncMessage("ViewSource:LoadSourceWithSelection", data);
    }
  },

  buildEditorArgs(aPath, aLineNumber) {
    // Determine the command line arguments to pass to the editor.
    // We currently support a %LINE% placeholder which is set to the passed
    // line number (or to 0 if there's none)
    var editorArgs = [];
    var args = Services.prefs.getCharPref("view_source.editor.args");
    if (args) {
      args = args.replace("%LINE%", aLineNumber || "0");
      // add the arguments to the array (keeping quoted strings intact)
      const argumentRE = /"([^"]+)"|(\S+)/g;
      while (argumentRE.test(args)) {
        editorArgs.push(RegExp.$1 || RegExp.$2);
      }
    }
    editorArgs.push(aPath);
    return editorArgs;
  },

  /**
   * Opens an external editor with the view source content.
   *
   * @param aArgs (required)
   *        This Object can include the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   *
   * @return Promise
   *        The promise will be resolved or rejected based on whether the
   *        external editor attempt was successful.  Either way, the data object
   *        is passed along as well.
   */
  openInExternalEditor(aArgs) {
    return new Promise((resolve, reject) => {
      let data;
      let { URL, browser, lineNumber } = aArgs;
      data = {
        url: URL,
        lineNumber,
        isPrivate: false,
      };
      if (browser) {
        data.doc = {
          characterSet: browser.characterSet,
          contentType: browser.documentContentType,
          title: browser.contentTitle,
          cookieJarSettings: browser.cookieJarSettings,
        };
        data.isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
      }

      try {
        var editor = this.getExternalViewSourceEditor();
        if (!editor) {
          reject(data);
          return;
        }

        // make a uri
        var charset = data.doc ? data.doc.characterSet : null;
        var uri = Services.io.newURI(data.url, charset);
        data.uri = uri;

        var path;
        var contentType = data.doc ? data.doc.contentType : null;
        var cookieJarSettings = data.doc ? data.doc.cookieJarSettings : null;
        if (uri.scheme == "file") {
          // it's a local file; we can open it directly
          path = uri.QueryInterface(Ci.nsIFileURL).file.path;

          var editorArgs = this.buildEditorArgs(path, data.lineNumber);
          editor.runw(false, editorArgs, editorArgs.length);
          resolve(data);
        } else {
          // set up the progress listener with what we know so far
          this.viewSourceProgressListener.contentLoaded = false;
          this.viewSourceProgressListener.editor = editor;
          this.viewSourceProgressListener.resolve = resolve;
          this.viewSourceProgressListener.reject = reject;
          this.viewSourceProgressListener.data = data;

          // without a page descriptor, loadPage has no chance of working. download the file.
          var file = this.getTemporaryFile(uri, data.doc, contentType);
          this.viewSourceProgressListener.file = file;

          var webBrowserPersist = Cc[
            "@mozilla.org/embedding/browser/nsWebBrowserPersist;1"
          ].createInstance(this.mnsIWebBrowserPersist);
          // the default setting is to not decode. we need to decode.
          webBrowserPersist.persistFlags = this.mnsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
          webBrowserPersist.progressListener = this.viewSourceProgressListener;
          let ssm = Services.scriptSecurityManager;
          let principal = ssm.createContentPrincipal(
            data.uri,
            browser.contentPrincipal.originAttributes
          );
          webBrowserPersist.savePrivacyAwareURI(
            uri,
            principal,
            null,
            null,
            cookieJarSettings,
            null,
            null,
            file,
            Ci.nsIContentPolicy.TYPE_SAVEAS_DOWNLOAD,
            data.isPrivate
          );

          let helperService = Cc[
            "@mozilla.org/uriloader/external-helper-app-service;1"
          ].getService(Ci.nsPIExternalAppLauncher);
          if (data.isPrivate) {
            // register the file to be deleted when possible
            helperService.deleteTemporaryPrivateFileWhenPossible(file);
          } else {
            // register the file to be deleted on app exit
            helperService.deleteTemporaryFileOnExit(file);
          }
        }
      } catch (ex) {
        // we failed loading it with the external editor.
        Cu.reportError(ex);
        reject(data);
      }
    });
  },

  // Returns nsIProcess of the external view source editor or null
  getExternalViewSourceEditor() {
    try {
      let viewSourceAppPath = Services.prefs.getComplexValue(
        "view_source.editor.path",
        Ci.nsIFile
      );
      let editor = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess
      );
      editor.init(viewSourceAppPath);

      return editor;
    } catch (ex) {
      Cu.reportError(ex);
    }

    return null;
  },

  viewSourceProgressListener: {
    mnsIWebProgressListener: Ci.nsIWebProgressListener,

    QueryInterface: ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]),

    destroy() {
      this.editor = null;
      this.resolve = null;
      this.reject = null;
      this.data = null;
      this.file = null;
    },

    // This listener is used both for tracking the progress of an HTML parse
    // in one case and for tracking the progress of nsIWebBrowserPersist in
    // another case.
    onStateChange(aProgress, aRequest, aFlag, aStatus) {
      // once it's done loading...
      if (aFlag & this.mnsIWebProgressListener.STATE_STOP && aStatus == 0) {
        // We aren't waiting for the parser. Instead, we are waiting for
        // an nsIWebBrowserPersist.
        this.onContentLoaded();
      }
      return 0;
    },

    onContentLoaded() {
      // The progress listener may call this multiple times, so be sure we only
      // run once.
      if (this.contentLoaded) {
        return;
      }
      try {
        if (!this.file) {
          throw new Error("View-source progress listener should have a file!");
        }

        var editorArgs = gViewSourceUtils.buildEditorArgs(
          this.file.path,
          this.data.lineNumber
        );
        this.editor.runw(false, editorArgs, editorArgs.length);

        this.contentLoaded = true;
        this.resolve(this.data);
      } catch (ex) {
        // we failed loading it with the external editor.
        Cu.reportError(ex);
        this.reject(this.data);
      } finally {
        this.destroy();
      }
    },

    editor: null,
    resolve: null,
    reject: null,
    data: null,
    file: null,
  },

  // returns an nsIFile for the passed document in the system temp directory
  getTemporaryFile(aURI, aDocument, aContentType) {
    // include contentAreaUtils.js in our own context when we first need it
    if (!this._caUtils) {
      this._caUtils = {};
      Services.scriptloader.loadSubScript(
        "chrome://global/content/contentAreaUtils.js",
        this._caUtils
      );
    }

    var fileName = this._caUtils.getDefaultFileName(
      null,
      aURI,
      aDocument,
      null
    );

    const mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
    fileName = mimeService.validateFileNameForSaving(
      fileName,
      aContentType,
      mimeService.VALIDATE_DEFAULT
    );

    var tempFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
    tempFile.append(fileName);
    return tempFile;
  },
};
