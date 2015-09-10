// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * To keep the global namespace safe, don't define global variables and
 * functions in this file.
 *
 * This file silently depends on contentAreaUtils.js for
 * getDefaultFileName, getNormalizedLeafName and getDefaultExtension
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ViewSourceBrowser",
  "resource://gre/modules/ViewSourceBrowser.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Deprecated",
  "resource://gre/modules/Deprecated.jsm");

var gViewSourceUtils = {

  mnsIWebBrowserPersist: Components.interfaces.nsIWebBrowserPersist,
  mnsIWebProgress: Components.interfaces.nsIWebProgress,
  mnsIWebPageDescriptor: Components.interfaces.nsIWebPageDescriptor,

  /**
   * Opens the view source window.
   *
   * @param aArgsOrURL (required)
   *        This is either an Object containing parameters, or a string
   *        URL for the page we want to view the source of. In the latter
   *        case we will be paying attention to the other parameters, as
   *        we will be supporting the old API for this method.
   *        If aArgsOrURL is an Object, the other parameters will be ignored.
   *        aArgsOrURL as an Object can include the following properties:
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
   * @param aPageDescriptor (deprecated, optional)
   *        Accepted for compatibility reasons, but is otherwise ignored.
   * @param aDocument (deprecated, optional)
   *        The content document we would like to view the source of. This
   *        function will throw if aDocument is a CPOW.
   * @param aLineNumber (deprecated, optional)
   *        The line number to focus on once the source is loaded.
   */
  viewSource: function(aArgsOrURL, aPageDescriptor, aDocument, aLineNumber)
  {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    if (prefs.getBoolPref("view_source.editor.external")) {
      this.openInExternalEditor(aArgsOrURL, aPageDescriptor, aDocument, aLineNumber);
    } else {
      this._openInInternalViewer(aArgsOrURL, aPageDescriptor, aDocument, aLineNumber);
    }
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
  viewSourceInBrowser: function(aArgs) {
    let viewSourceBrowser = new ViewSourceBrowser(aArgs.viewSourceBrowser);
    viewSourceBrowser.loadViewSource(aArgs);
  },

  /**
   * Displays view source for a selection from some document in the provided
   * <browser>.  This allows for non-window display methods, such as a tab from
   * Firefox.
   *
   * @param aViewSourceInBrowser
   *        The browser containing the page to view the source of.
   * @param aTarget
   *        Set to the target node for MathML. Null for other types of elements.
   * @param aGetBrowserFn
   *        If set, a function that will return a browser to open the source in.
   *        If null, or this function returns null, opens the source in a new window.
   */
  viewPartialSourceInBrowser: function(aViewSourceInBrowser, aTarget, aGetBrowserFn) {
    let mm = aViewSourceInBrowser.messageManager;
    mm.addMessageListener("ViewSource:GetSelectionDone", function gotSelection(message) {
      mm.removeMessageListener("ViewSource:GetSelectionDone", gotSelection);

      if (!message.data)
        return;

      let browserToOpenIn = aGetBrowserFn ? aGetBrowserFn() : null;
      if (browserToOpenIn) {
        let viewSourceBrowser = new ViewSourceBrowser(browserToOpenIn);
        viewSourceBrowser.loadViewSourceFromSelection(message.data.uri, message.data.drawSelection,
                                                      message.data.baseURI);
      }
      else {
        let docUrl = null;
        window.openDialog("chrome://global/content/viewPartialSource.xul",
                          "_blank", "scrollbars,resizable,chrome,dialog=no",
                          {
                            URI: message.data.uri,
                            drawSelection: message.data.drawSelection,
                            baseURI: message.data.baseURI,
                            partial: true,
                          });
      }
    });

    mm.sendAsyncMessage("ViewSource:GetSelection", { }, { target: aTarget });
  },

  // Opens the interval view source viewer
  _openInInternalViewer: function(aArgsOrURL, aPageDescriptor, aDocument, aLineNumber)
  {
    // try to open a view-source window while inheriting the charset (if any)
    var charset = null;
    var isForcedCharset = false;
    if (aDocument) {
      if (Components.utils.isCrossProcessWrapper(aDocument)) {
        throw new Error("View Source cannot accept a CPOW as a document.");
      }

      charset = "charset=" + aDocument.characterSet;
      try {
        isForcedCharset =
          aDocument.defaultView
                   .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIDOMWindowUtils)
                   .docCharsetIsForced;
      } catch (ex) {
      }
    }
    openDialog("chrome://global/content/viewSource.xul",
               "_blank",
               "all,dialog=no",
               aArgsOrURL, charset, aPageDescriptor, aLineNumber, isForcedCharset);
  },

  buildEditorArgs: function(aPath, aLineNumber) {
    // Determine the command line arguments to pass to the editor.
    // We currently support a %LINE% placeholder which is set to the passed
    // line number (or to 0 if there's none)
    var editorArgs = [];
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch);
    var args = prefs.getCharPref("view_source.editor.args");
    if (args) {
      args = args.replace("%LINE%", aLineNumber || "0");
      // add the arguments to the array (keeping quoted strings intact)
      const argumentRE = /"([^"]+)"|(\S+)/g;
      while (argumentRE.test(args))
        editorArgs.push(RegExp.$1 || RegExp.$2);
    }
    editorArgs.push(aPath);
    return editorArgs;
  },

  /**
   * Opens an external editor with the view source content.
   *
   * @param aArgsOrURL (required)
   *        This is either an Object containing parameters, or a string
   *        URL for the page we want to view the source of. In the latter
   *        case we will be paying attention to the other parameters, as
   *        we will be supporting the old API for this method.
   *        If aArgsOrURL is an Object, the other parameters will be ignored.
   *        aArgsOrURL as an Object can include the following properties:
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
   * @param aPageDescriptor (deprecated, optional)
   *        Accepted for compatibility reasons, but is otherwise ignored.
   * @param aDocument (deprecated, optional)
   *        The content document we would like to view the source of. This
   *        function will throw if aDocument is a CPOW.
   * @param aLineNumber (deprecated, optional)
   *        The line number to focus on once the source is loaded.
   * @param aCallBack
   *        A function accepting two arguments:
   *          * result (true = success)
   *          * data object
   *        The function defaults to opening an internal viewer if external
   *        viewing fails.
   */
  openInExternalEditor: function(aArgsOrURL, aPageDescriptor, aDocument,
                                 aLineNumber, aCallBack) {
    let data;
    if (typeof aArgsOrURL == "string") {
      Deprecated.warning("The arguments you're passing to " +
                         "openInExternalEditor are using an out-of-date API.",
                         "https://developer.mozilla.org/en-US/Add-ons/" +
                         "Code_snippets/View_Source_for_XUL_Applications");
      data = {
        url: aArgsOrURL,
        pageDescriptor: aPageDescriptor,
        doc: aDocument,
        lineNumber: aLineNumber
      };
    } else {
      let { URL, outerWindowID, lineNumber } = aArgsOrURL;
      data = {
        url: URL,
        lineNumber
      };
    }

    try {
      var editor = this.getExternalViewSourceEditor();
      if (!editor) {
        this.handleCallBack(aCallBack, false, data);
        return;
      }

      // make a uri
      var ios = Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
      var charset = aDocument ? aDocument.characterSet : null;
      var uri = ios.newURI(data.url, charset, null);
      data.uri = uri;

      var path;
      var contentType = aDocument ? aDocument.contentType : null;
      if (uri.scheme == "file") {
        // it's a local file; we can open it directly
        path = uri.QueryInterface(Components.interfaces.nsIFileURL).file.path;

        var editorArgs = this.buildEditorArgs(path, data.lineNumber);
        editor.runw(false, editorArgs, editorArgs.length);
        this.handleCallBack(aCallBack, true, data);
      } else {
        // set up the progress listener with what we know so far
        this.viewSourceProgressListener.contentLoaded = false;
        this.viewSourceProgressListener.editor = editor;
        this.viewSourceProgressListener.callBack = aCallBack;
        this.viewSourceProgressListener.data = data;
        if (!aPageDescriptor) {
          // without a page descriptor, loadPage has no chance of working. download the file.
          var file = this.getTemporaryFile(uri, aDocument, contentType);
          this.viewSourceProgressListener.file = file;

          let fromPrivateWindow = false;
          if (aDocument) {
            try {
              fromPrivateWindow =
                aDocument.defaultView
                         .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation)
                         .QueryInterface(Components.interfaces.nsILoadContext)
                         .usePrivateBrowsing;
            } catch (e) {
            }
          }

          var webBrowserPersist = Components
                                  .classes["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                                  .createInstance(this.mnsIWebBrowserPersist);
          // the default setting is to not decode. we need to decode.
          webBrowserPersist.persistFlags = this.mnsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
          webBrowserPersist.progressListener = this.viewSourceProgressListener;
          let referrerPolicy = Components.interfaces.nsIHttpChannel.REFERRER_POLICY_NO_REFERRER;
          webBrowserPersist.savePrivacyAwareURI(uri, null, null, referrerPolicy, null, null, file, fromPrivateWindow);

          let helperService = Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"]
                                        .getService(Components.interfaces.nsPIExternalAppLauncher);
          if (fromPrivateWindow) {
            // register the file to be deleted when possible
            helperService.deleteTemporaryPrivateFileWhenPossible(file);
          } else {
            // register the file to be deleted on app exit
            helperService.deleteTemporaryFileOnExit(file);
          }
        } else {
          // we'll use nsIWebPageDescriptor to get the source because it may
          // not have to refetch the file from the server
          // XXXbz this is so broken...  This code doesn't set up this docshell
          // at all correctly; if somehow the view-source stuff managed to
          // execute script we'd be in big trouble here, I suspect.
          var webShell = Components.classes["@mozilla.org/docshell;1"].createInstance();
          webShell.QueryInterface(Components.interfaces.nsIBaseWindow).create();
          this.viewSourceProgressListener.webShell = webShell;
          var progress = webShell.QueryInterface(this.mnsIWebProgress);
          progress.addProgressListener(this.viewSourceProgressListener,
                                       this.mnsIWebProgress.NOTIFY_STATE_DOCUMENT);
          var pageLoader = webShell.QueryInterface(this.mnsIWebPageDescriptor);
          pageLoader.loadPage(aPageDescriptor, this.mnsIWebPageDescriptor.DISPLAY_AS_SOURCE);
        }
      }
    } catch (ex) {
      // we failed loading it with the external editor.
      Components.utils.reportError(ex);
      this.handleCallBack(aCallBack, false, data);
      return;
    }
  },

  // Default callback - opens the internal viewer if the external editor failed
  internalViewerFallback: function(result, data)
  {
    if (!result) {
      this._openInInternalViewer(data.url, data.pageDescriptor, data.doc, data.lineNumber);
    }
  },

  // Calls the callback, keeping in mind undefined or null values.
  handleCallBack: function(aCallBack, result, data)
  {
    // ifcallback is undefined, default to the internal viewer
    if (aCallBack === undefined) {
      this.internalViewerFallback(result, data);
    } else if (aCallBack) {
      aCallBack(result, data);
    }
  },

  // Returns nsIProcess of the external view source editor or null
  getExternalViewSourceEditor: function()
  {
    try {
      let viewSourceAppPath =
          Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefBranch)
                    .getComplexValue("view_source.editor.path",
                                     Components.interfaces.nsIFile);
      let editor = Components.classes['@mozilla.org/process/util;1']
                             .createInstance(Components.interfaces.nsIProcess);
      editor.init(viewSourceAppPath);

      return editor;
    }
    catch (ex) {
      Components.utils.reportError(ex);
    }

    return null;
  },

  viewSourceProgressListener: {

    mnsIWebProgressListener: Components.interfaces.nsIWebProgressListener,

    QueryInterface: function(aIID) {
     if (aIID.equals(this.mnsIWebProgressListener) ||
         aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
         aIID.equals(Components.interfaces.nsISupports))
       return this;
     throw Components.results.NS_NOINTERFACE;
    },

    destroy: function() {
      if (this.webShell) {
        this.webShell.QueryInterface(Components.interfaces.nsIBaseWindow).destroy();
      }
      this.webShell = null;
      this.editor = null;
      this.callBack = null;
      this.data = null;
      this.file = null;
    },

    // This listener is used both for tracking the progress of an HTML parse
    // in one case and for tracking the progress of nsIWebBrowserPersist in
    // another case.
    onStateChange: function(aProgress, aRequest, aFlag, aStatus) {
      // once it's done loading...
      if ((aFlag & this.mnsIWebProgressListener.STATE_STOP) && aStatus == 0) {
        if (!this.webShell) {
          // We aren't waiting for the parser. Instead, we are waiting for
          // an nsIWebBrowserPersist.
          this.onContentLoaded();
          return 0;
        }
        var webNavigation = this.webShell.QueryInterface(Components.interfaces.nsIWebNavigation);
        if (webNavigation.document.readyState == "complete") {
          // This branch is probably never taken. Including it for completeness.
          this.onContentLoaded();
        } else {
          webNavigation.document.addEventListener("DOMContentLoaded",
                                                  this.onContentLoaded.bind(this));
        }
      }
      return 0;
    },

    onContentLoaded: function() {
      // The progress listener may call this multiple times, so be sure we only
      // run once.
      if (this.contentLoaded) {
        return;
      }
      try {
        if (!this.file) {
          // it's not saved to file yet, it's in the webshell

          // get a temporary filename using the attributes from the data object that
          // openInExternalEditor gave us
          this.file = gViewSourceUtils.getTemporaryFile(this.data.uri, this.data.doc,
                                                        this.data.doc.contentType);

          // we have to convert from the source charset.
          var webNavigation = this.webShell.QueryInterface(Components.interfaces.nsIWebNavigation);
          var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                                   .createInstance(Components.interfaces.nsIFileOutputStream);
          foStream.init(this.file, 0x02 | 0x08 | 0x20, -1, 0); // write | create | truncate
          var coStream = Components.classes["@mozilla.org/intl/converter-output-stream;1"]
                                   .createInstance(Components.interfaces.nsIConverterOutputStream);
          coStream.init(foStream, this.data.doc.characterSet, 0, null);

          // write the source to the file
          coStream.writeString(webNavigation.document.body.textContent);

          // clean up
          coStream.close();
          foStream.close();

          let fromPrivateWindow =
            this.data.doc.defaultView
                         .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIWebNavigation)
                         .QueryInterface(Components.interfaces.nsILoadContext)
                         .usePrivateBrowsing;

          let helperService = Components.classes["@mozilla.org/uriloader/external-helper-app-service;1"]
                              .getService(Components.interfaces.nsPIExternalAppLauncher);
          if (fromPrivateWindow) {
            // register the file to be deleted when possible
            helperService.deleteTemporaryPrivateFileWhenPossible(this.file);
          } else {
            // register the file to be deleted on app exit
            helperService.deleteTemporaryFileOnExit(this.file);
          }
        }

        var editorArgs = gViewSourceUtils.buildEditorArgs(this.file.path,
                                                          this.data.lineNumber);
        this.editor.runw(false, editorArgs, editorArgs.length);

        this.contentLoaded = true;
        gViewSourceUtils.handleCallBack(this.callBack, true, this.data);
      } catch (ex) {
        // we failed loading it with the external editor.
        Components.utils.reportError(ex);
        gViewSourceUtils.handleCallBack(this.callBack, false, this.data);
      } finally {
        this.destroy();
      }
    },

    onLocationChange: function() {return 0;},
    onProgressChange: function() {return 0;},
    onStatusChange: function() {return 0;},
    onSecurityChange: function() {return 0;},

    webShell: null,
    editor: null,
    callBack: null,
    data: null,
    file: null
  },

  // returns an nsIFile for the passed document in the system temp directory
  getTemporaryFile: function(aURI, aDocument, aContentType) {
    // include contentAreaUtils.js in our own context when we first need it
    if (!this._caUtils) {
      var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                                   .getService(Components.interfaces.mozIJSSubScriptLoader);
      this._caUtils = {};
      scriptLoader.loadSubScript("chrome://global/content/contentAreaUtils.js", this._caUtils);
    }

    var fileLocator = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties);
    var tempFile = fileLocator.get("TmpD", Components.interfaces.nsIFile);
    var fileName = this._caUtils.getDefaultFileName(null, aURI, aDocument, aContentType);
    var extension = this._caUtils.getDefaultExtension(fileName, aURI, aContentType);
    var leafName = this._caUtils.getNormalizedLeafName(fileName, extension);
    tempFile.append(leafName);
    return tempFile;
  }
}
