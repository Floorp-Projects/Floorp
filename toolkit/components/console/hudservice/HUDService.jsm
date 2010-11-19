/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools (HeadsUpDisplay) Console Code
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Dahl <ddahl@mozilla.com> (original author)
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Johnathan Nightingale <jnightingale@mozilla.com>
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Julian Viereck <jviereck@mozilla.com>
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const CONSOLEAPI_CLASS_ID = "{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["HUDService", "ConsoleUtils"];

XPCOMUtils.defineLazyServiceGetter(this, "scriptError",
                                   "@mozilla.org/scripterror;1",
                                   "nsIScriptError");

XPCOMUtils.defineLazyServiceGetter(this, "activityDistributor",
                                   "@mozilla.org/network/http-activity-distributor;1",
                                   "nsIHttpActivityDistributor");

XPCOMUtils.defineLazyServiceGetter(this, "mimeService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");

XPCOMUtils.defineLazyGetter(this, "NetUtil", function () {
  var obj = {};
  Cu.import("resource://gre/modules/NetUtil.jsm", obj);
  return obj.NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "PropertyPanel", function () {
  var obj = {};
  try {
    Cu.import("resource://gre/modules/PropertyPanel.jsm", obj);
  } catch (err) {
    Cu.reportError(err);
  }
  return obj.PropertyPanel;
});

XPCOMUtils.defineLazyGetter(this, "namesAndValuesOf", function () {
  var obj = {};
  Cu.import("resource://gre/modules/PropertyPanel.jsm", obj);
  return obj.namesAndValuesOf;
});

function LogFactory(aMessagePrefix)
{
  function log(aMessage) {
    var _msg = aMessagePrefix + " " + aMessage + "\n";
    dump(_msg);
  }
  return log;
}

let log = LogFactory("*** HUDService:");

const HUD_STRINGS_URI = "chrome://global/locale/headsUpDisplay.properties";

XPCOMUtils.defineLazyGetter(this, "stringBundle", function () {
  return Services.strings.createBundle(HUD_STRINGS_URI);
});

// The amount of time in milliseconds that must pass between messages to
// trigger the display of a new group.
const NEW_GROUP_DELAY = 5000;

// The amount of time in milliseconds that we wait before performing a live
// search.
const SEARCH_DELAY = 200;

// The number of lines that are displayed in the console output by default.
// The user can change this number by adjusting the hidden
// "devtools.hud.loglimit" preference.
const DEFAULT_LOG_LIMIT = 200;

// Possible directions that can be passed to HUDService.animate().
const ANIMATE_OUT = 0;
const ANIMATE_IN = 1;

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

// The maximum number of bytes a Network ResponseListener can hold.
const RESPONSE_BODY_LIMIT = 1048576; // 1 MB

const ERRORS = { LOG_MESSAGE_MISSING_ARGS:
                 "Missing arguments: aMessage, aConsoleNode and aMessageNode are required.",
                 CANNOT_GET_HUD: "Cannot getHeads Up Display with provided ID",
                 MISSING_ARGS: "Missing arguments",
                 LOG_OUTPUT_FAILED: "Log Failure: Could not append messageNode to outputNode",
};

/**
 * Implements the nsIStreamListener and nsIRequestObserver interface. Used
 * within the HS_httpObserverFactory function to get the response body of
 * requests.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @param object aHttpActivity
 *        HttpActivity object associated with this request (see
 *        HS_httpObserverFactory). As the response is done, the response header,
 *        body and status is stored on aHttpActivity.
 */
function ResponseListener(aHttpActivity) {
  this.receivedData = "";
  this.httpActivity = aHttpActivity;
}

ResponseListener.prototype =
{
  /**
   * The original listener for this request.
   */
  originalListener: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * Sets the httpActivity object's response header if it isn't set already.
   *
   * @param nsIRequest aRequest
   */
  setResponseHeader: function RL_setResponseHeader(aRequest)
  {
    let httpActivity = this.httpActivity;
    // Check if the header isn't set yet.
    if (!httpActivity.response.header) {
      if (aRequest instanceof Ci.nsIHttpChannel) {
      httpActivity.response.header = {};
        try {
        aRequest.visitResponseHeaders({
          visitHeader: function(aName, aValue) {
            httpActivity.response.header[aName] = aValue;
          }
        });
      }
        // Accessing the response header can throw an NS_ERROR_NOT_AVAILABLE
        // exception. Catch it and stop it to make it not show up in the.
        // This can happen if the response is not finished yet and the user
        // reloades the page.
        catch (ex) {
          delete httpActivity.response.header;
        }
      }
    }
  },

  /**
   * See documention at
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * Grabs a copy of the original data and passes it on to the original listener.
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsIInputStream aInputStream
   * @param unsigned long aOffset
   * @param unsigned long aCount
   */
  onDataAvailable: function RL_onDataAvailable(aRequest, aContext, aInputStream,
                                                aOffset, aCount)
  {
    this.setResponseHeader(aRequest);

    let StorageStream = Components.Constructor("@mozilla.org/storagestream;1",
                                                "nsIStorageStream",
                                                "init");
    let BinaryOutputStream = Components.Constructor("@mozilla.org/binaryoutputstream;1",
                                                      "nsIBinaryOutputStream",
                                                      "setOutputStream");

    storageStream = new StorageStream(8192, aCount, null);
    binaryOutputStream = new BinaryOutputStream(storageStream.getOutputStream(0));

    let data = NetUtil.readInputStreamToString(aInputStream, aCount);

    if (HUDService.saveRequestAndResponseBodies &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData += data;
    }

    binaryOutputStream.writeBytes(data, aCount);

    let newInputStream = storageStream.newInputStream(0);
    try {
    this.originalListener.onDataAvailable(aRequest, aContext,
        newInputStream, aOffset, aCount);
    }
    catch(ex) {
      aRequest.cancel(ex);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   */
  onStartRequest: function RL_onStartRequest(aRequest, aContext)
  {
    try {
    this.originalListener.onStartRequest(aRequest, aContext);
    }
    catch(ex) {
      aRequest.cancel(ex);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * If aRequest is an nsIHttpChannel then the response header is stored on the
   * httpActivity object. Also, the response body is set on the httpActivity
   * object (if the user has turned on response content logging) and the
   * HUDService.lastFinishedRequestCallback is called if there is one.
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsresult aStatusCode
   */
  onStopRequest: function RL_onStopRequest(aRequest, aContext, aStatusCode)
  {
    try {
    this.originalListener.onStopRequest(aRequest, aContext, aStatusCode);
    }
    catch (ex) { }

    this.setResponseHeader(aRequest);

    if (HUDService.saveRequestAndResponseBodies) {
      this.httpActivity.response.body = this.receivedData;
    }
    else {
      this.httpActivity.response.bodyDiscarded = true;
    }

    if (HUDService.lastFinishedRequestCallback) {
      HUDService.lastFinishedRequestCallback(this.httpActivity);
    }

    // Call update on all panels.
    this.httpActivity.panels.forEach(function(weakRef) {
      let panel = weakRef.get();
      if (panel) {
        panel.update();
      }
    });
    this.httpActivity.response.isDone = true;
    this.httpActivity.response.listener = null;
    this.httpActivity = null;
    this.receivedData = "";
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIStreamListener,
    Ci.nsISupports
  ])
}

/**
 * Helper object for networking stuff.
 *
 * All of the following functions have been taken from the Firebug source. They
 * have been modified to match the Firefox coding rules.
 */

// FIREBUG CODE BEGIN.

/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2007, Parakey Inc.
 * All rights reserved.
 *
 * Redistribution and use of this software in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 *
 * * Neither the name of Parakey Inc. nor the names of its
 *   contributors may be used to endorse or promote products
 *   derived from this software without specific prior
 *   written permission of Parakey Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Creator:
 *  Joe Hewitt
 * Contributors
 *  John J. Barton (IBM Almaden)
 *  Jan Odvarko (Mozilla Corp.)
 *  Max Stepanov (Aptana Inc.)
 *  Rob Campbell (Mozilla Corp.)
 *  Hans Hillen (Paciello Group, Mozilla)
 *  Curtis Bartley (Mozilla Corp.)
 *  Mike Collins (IBM Almaden)
 *  Kevin Decker
 *  Mike Ratcliffe (Comartis AG)
 *  Hernan Rodríguez Colmeiro
 *  Austin Andrews
 *  Christoph Dorn
 *  Steven Roussey (AppCenter Inc, Network54)
 */
var NetworkHelper =
{
  /**
   * Converts aText with a given aCharset to unicode.
   *
   * @param string aText
   *        Text to convert.
   * @param string aCharset
   *        Charset to convert the text to.
   * @returns string
   *          Converted text.
   */
  convertToUnicode: function NH_convertToUnicode(aText, aCharset)
  {
    let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
               createInstance(Ci.nsIScriptableUnicodeConverter);
    conv.charset = aCharset || "UTF-8";
    return conv.ConvertToUnicode(aText);
  },

  /**
   * Reads all available bytes from aStream and converts them to aCharset.
   *
   * @param nsIInputStream aStream
   * @param string aCharset
   * @returns string
   *          UTF-16 encoded string based on the content of aStream and aCharset.
   */
  readAndConvertFromStream: function NH_readAndConvertFromStream(aStream, aCharset)
  {
    let text = null;
    try {
      text = NetUtil.readInputStreamToString(aStream, aStream.available())
      return this.convertToUnicode(text, aCharset);
    }
    catch (err) {
      return text;
    }
  },

   /**
   * Reads the posted text from aRequest.
   *
   * @param nsIHttpChannel aRequest
   * @param nsIDOMNode aBrowser
   * @returns string or null
   *          Returns the posted string if it was possible to read from aRequest
   *          otherwise null.
   */
  readPostTextFromRequest: function NH_readPostTextFromRequest(aRequest, aBrowser)
  {
    if (aRequest instanceof Ci.nsIUploadChannel) {
      let iStream = aRequest.uploadStream;

      let isSeekableStream = false;
      if (iStream instanceof Ci.nsISeekableStream) {
        isSeekableStream = true;
      }

      let prevOffset;
      if (isSeekableStream) {
        prevOffset = iStream.tell();
        iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
      }

      // Read data from the stream.
      let charset = aBrowser.contentWindow.document.characterSet;
      let text = this.readAndConvertFromStream(iStream, charset);

      // Seek locks the file, so seek to the beginning only if necko hasn't
      // read it yet, since necko doesn't seek to 0 before reading (at lest
      // not till 459384 is fixed).
      if (isSeekableStream && prevOffset == 0) {
        iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
      }
      return text;
    }
    return null;
  },

  /**
   * Reads the posted text from the page's cache.
   *
   * @param nsIDOMNode aBrowser
   * @returns string or null
   *          Returns the posted string if it was possible to read from aBrowser
   *          otherwise null.
   */
  readPostTextFromPage: function NH_readPostTextFromPage(aBrowser)
  {
    let webNav = aBrowser.webNavigation;
    if (webNav instanceof Ci.nsIWebPageDescriptor) {
      let descriptor = webNav.currentDescriptor;

      if (descriptor instanceof Ci.nsISHEntry && descriptor.postData &&
          descriptor instanceof Ci.nsISeekableStream) {
        descriptor.seek(NS_SEEK_SET, 0);

        let charset = browser.contentWindow.document.characterSet;
        return this.readAndConvertFromStream(descriptor, charset);
      }
    }
    return null;
  },

  /**
   * Gets the nsIDOMWindow that is associated with aRequest.
   *
   * @param nsIHttpChannel aRequest
   * @returns nsIDOMWindow or null
   */
  getWindowForRequest: function NH_getWindowForRequest(aRequest)
  {
    let loadContext = this.getRequestLoadContext(aRequest);
    if (loadContext) {
      return loadContext.associatedWindow;
    }
    return null;
  },

  /**
   * Gets the nsILoadContext that is associated with aRequest.
   *
   * @param nsIHttpChannel aRequest
   * @returns nsILoadContext or null
   */
  getRequestLoadContext: function NH_getRequestLoadContext(aRequest)
  {
    if (aRequest && aRequest.notificationCallbacks) {
      try {
        return aRequest.notificationCallbacks.getInterface(Ci.nsILoadContext);
      } catch (ex) { }
    }

    if (aRequest && aRequest.loadGroup
                 && aRequest.loadGroup.notificationCallbacks) {
      try {
        return aRequest.loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
      } catch (ex) { }
    }

    return null;
  },

  /**
   * Loads the content of aUrl from the cache.
   *
   * @param string aUrl
   *        URL to load the cached content for.
   * @param string aCharset
   *        Assumed charset of the cached content. Used if there is no charset
   *        on the channel directly.
   * @param function aCallback
   *        Callback that is called with the loaded cached content if available
   *        or null if something failed while getting the cached content.
   */
  loadFromCache: function NH_loadFromCache(aUrl, aCharset, aCallback)
  {
    let channel = NetUtil.newChannel(aUrl);

    // Ensure that we only read from the cache and not the server.
    channel.loadFlags = Ci.nsIRequest.LOAD_FROM_CACHE |
      Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
      Ci.nsICachingChannel.LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

    NetUtil.asyncFetch(channel, function (aInputStream, aStatusCode, aRequest) {
      if (!Components.isSuccessCode(aStatusCode)) {
        aCallback(null);
        return;
      }

      // Try to get the encoding from the channel. If there is none, then use
      // the passed assumed aCharset.
      let aChannel = aRequest.QueryInterface(Ci.nsIChannel);
      let contentCharset = aChannel.contentCharset || aCharset;

      // Read the content of the stream using contentCharset as encoding.
      aCallback(NetworkHelper.readAndConvertFromStream(aInputStream,
                                                       contentCharset));
    });
  },

  // This is a list of all the mime category maps jviereck could find in the
  // firebug code base.
  mimeCategoryMap: {
    "text/plain": "txt",
    "text/html": "html",
    "text/xml": "xml",
    "text/xsl": "txt",
    "text/xul": "txt",
    "text/css": "css",
    "text/sgml": "txt",
    "text/rtf": "txt",
    "text/x-setext": "txt",
    "text/richtext": "txt",
    "text/javascript": "js",
    "text/jscript": "txt",
    "text/tab-separated-values": "txt",
    "text/rdf": "txt",
    "text/xif": "txt",
    "text/ecmascript": "js",
    "text/vnd.curl": "txt",
    "text/x-json": "json",
    "text/x-js": "txt",
    "text/js": "txt",
    "text/vbscript": "txt",
    "view-source": "txt",
    "view-fragment": "txt",
    "application/xml": "xml",
    "application/xhtml+xml": "xml",
    "application/atom+xml": "xml",
    "application/rss+xml": "xml",
    "application/vnd.mozilla.maybe.feed": "xml",
    "application/vnd.mozilla.xul+xml": "xml",
    "application/javascript": "js",
    "application/x-javascript": "js",
    "application/x-httpd-php": "txt",
    "application/rdf+xml": "xml",
    "application/ecmascript": "js",
    "application/http-index-format": "txt",
    "application/json": "json",
    "application/x-js": "txt",
    "multipart/mixed": "txt",
    "multipart/x-mixed-replace": "txt",
    "image/svg+xml": "svg",
    "application/octet-stream": "bin",
    "image/jpeg": "image",
    "image/jpg": "image",
    "image/gif": "image",
    "image/png": "image",
    "image/bmp": "image",
    "application/x-shockwave-flash": "flash",
    "video/x-flv": "flash",
    "audio/mpeg3": "media",
    "audio/x-mpeg-3": "media",
    "video/mpeg": "media",
    "video/x-mpeg": "media",
    "audio/ogg": "media",
    "application/ogg": "media",
    "application/x-ogg": "media",
    "application/x-midi": "media",
    "audio/midi": "media",
    "audio/x-mid": "media",
    "audio/x-midi": "media",
    "music/crescendo": "media",
    "audio/wav": "media",
    "audio/x-wav": "media",
    "text/json": "json",
    "application/x-json": "json",
    "application/json-rpc": "json"
  }
}

// FIREBUG CODE END.

///////////////////////////////////////////////////////////////////////////
//// Helper for creating the network panel.

/**
 * Creates a DOMNode and sets all the attributes of aAttributes on the created
 * element.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 *
 * @returns nsIDOMNode
 */
function createElement(aDocument, aTag, aAttributes)
{
  let node = aDocument.createElement(aTag);
  for (var attr in aAttributes) {
    node.setAttribute(attr, aAttributes[attr]);
  }
  return node;
}

/**
 * Creates a new DOMNode and appends it to aParent.
 *
 * @param nsIDOMNode aParent
 *        A parent node to append the created element.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 *
 * @returns nsIDOMNode
 */
function createAndAppendElement(aParent, aTag, aAttributes)
{
  let node = createElement(aParent.ownerDocument, aTag, aAttributes);
  aParent.appendChild(node);
  return node;
}

/**
 * Convenience function to unwrap a wrapped object.
 *
 * @param aObject the object to unwrap
 */

function unwrap(aObject)
{
  return XPCNativeWrapper.unwrap(aObject);
}

///////////////////////////////////////////////////////////////////////////
//// NetworkPanel

/**
 * Creates a new NetworkPanel.
 *
 * @param nsIDOMNode aParent
 *        Parent node to append the created panel to.
 * @param object aHttpActivity
 *        HttpActivity to display in the panel.
 */
function NetworkPanel(aParent, aHttpActivity)
{
  let doc = aParent.ownerDocument;
  this.httpActivity = aHttpActivity;

  // Create the underlaying panel
  this.panel = createElement(doc, "panel", {
    label: HUDService.getStr("NetworkPanel.label"),
    titlebar: "normal",
    noautofocus: "true",
    noautohide: "true",
    close: "true"
  });

  // Create the iframe that displays the NetworkPanel XHTML.
  this.iframe = createAndAppendElement(this.panel, "iframe", {
    src: "chrome://browser/content/NetworkPanel.xhtml",
    type: "content",
    flex: "1"
  });

  let self = this;

  // Destroy the panel when it's closed.
  this.panel.addEventListener("popuphidden", function onPopupHide() {
    self.panel.removeEventListener("popuphidden", onPopupHide, false);
    self.panel.parentNode.removeChild(self.panel);
    self.panel = null;
    self.iframe = null;
    self.document = null;
    self.httpActivity = null;

    if (self.linkNode) {
      self.linkNode._panelOpen = false;
      self.linkNode = null;
    }
  }, false);

  // Set the document object and update the content once the panel is loaded.
  this.panel.addEventListener("load", function onLoad() {
    self.panel.removeEventListener("load", onLoad, true);
    self.document = self.iframe.contentWindow.document;
    self.update();
  }, true);

  // Create the footer.
  let footer = createElement(doc, "hbox", { align: "end" });
  createAndAppendElement(footer, "spacer", { flex: 1 });

  createAndAppendElement(footer, "resizer", { dir: "bottomend" });
  this.panel.appendChild(footer);

  aParent.appendChild(this.panel);
}

NetworkPanel.prototype =
{
  /**
   * Callback is called once the NetworkPanel is processed completly. Used by
   * unit tests.
   */
  isDoneCallback: null,

  /**
   * The current state of the output.
   */
  _state: 0,

  /**
   * State variables.
   */
  _INIT: 0,
  _DISPLAYED_REQUEST_HEADER: 1,
  _DISPLAYED_REQUEST_BODY: 2,
  _DISPLAYED_RESPONSE_HEADER: 3,
  _TRANSITION_CLOSED: 4,

  _fromDataRegExp: /Content-Type\:\s*application\/x-www-form-urlencoded/,

  /**
   * Small helper function that is nearly equal to  HUDService.getFormatStr
   * except that it prefixes aName with "NetworkPanel.".
   *
   * @param string aName
   *        The name of an i10n string to format. This string is prefixed with
   *        "NetworkPanel." before calling the HUDService.getFormatStr function.
   * @param array aArray
   *        Values used as placeholder for the i10n string.
   * @returns string
   *          The i10n formated string.
   */
  _format: function NP_format(aName, aArray)
  {
    return HUDService.getFormatStr("NetworkPanel." + aName, aArray);
  },

  /**
   * Returns the content type of the response body. This is based on the
   * response.header["Content-Type"] info. If this value is not available, then
   * the content type is tried to be estimated by the url file ending.
   *
   * @returns string or null
   *          Content type or null if no content type could be figured out.
   */
  get _contentType()
  {
    let response = this.httpActivity.response;
    let contentTypeValue = null;

    if (response.header && response.header["Content-Type"]) {
      let types = response.header["Content-Type"].split(/,|;/);
      for (let i = 0; i < types.length; i++) {
        let type = NetworkHelper.mimeCategoryMap[types[i]];
        if (type) {
          return types[i];
        }
      }
    }

    // Try to get the content type from the request file extension.
    let uri = NetUtil.newURI(this.httpActivity.url);
    let mimeType = null;
    if ((uri instanceof Ci.nsIURL) && uri.fileExtension) {
      try {
        mimeType = mimeService.getTypeFromExtension(uri.fileExtension);
      } catch(e) {
        // Added to prevent failures on OS X 64. No Flash?
        Cu.reportError(e);
        // Return empty string to pass unittests.
        return "";
      }
    }
    return mimeType;
  },

  /**
   *
   * @returns boolean
   *          True if the response is an image, false otherwise.
   */
  get _responseIsImage()
  {
    return NetworkHelper.mimeCategoryMap[this._contentType] == "image";
  },

  /**
   *
   * @returns boolean
   *          True if the response body contains text, false otherwise.
   */
  get _isResponseBodyTextData()
  {
    let contentType = this._contentType;

    if (!contentType)
      return false;

    if (contentType.indexOf("text/") == 0) {
      return true;
    }

    switch (NetworkHelper.mimeCategoryMap[contentType]) {
      case "txt":
      case "js":
      case "json":
      case "css":
      case "html":
      case "svg":
      case "xml":
        return true;

      default:
        return false;
    }
  },

  /**
   *
   * @returns boolean
   *          Returns true if the server responded that the request is already
   *          in the browser's cache, false otherwise.
   */
  get _isResponseCached()
  {
    return this.httpActivity.response.status.indexOf("304") != -1;
  },

  /**
   *
   * @returns boolean
   *          Returns true if the posted body contains form data.
   */
  get _isRequestBodyFormData()
  {
    let requestBody = this.httpActivity.request.body;
    return this._fromDataRegExp.test(requestBody);
  },

  /**
   * Appends the node with id=aId by the text aValue.
   *
   * @param string aId
   * @param string aValue
   * @returns void
   */
  _appendTextNode: function NP_appendTextNode(aId, aValue)
  {
    let textNode = this.document.createTextNode(aValue);
    this.document.getElementById(aId).appendChild(textNode);
  },

  /**
   * Generates some HTML to display the key-value pair of the aList data. The
   * generated HTML is added to node with id=aParentId.
   *
   * @param string aParentId
   *        Id of the parent node to append the list to.
   * @oaram object aList
   *        Object that holds the key-value information to display in aParentId.
   * @param boolean aIgnoreCookie
   *        If true, the key-value named "Cookie" is not added to the list.
   * @returns void
   */
  _appendList: function NP_appendList(aParentId, aList, aIgnoreCookie)
  {
    let parent = this.document.getElementById(aParentId);
    let doc = this.document;

    let sortedList = {};
    Object.keys(aList).sort().forEach(function(aKey) {
      sortedList[aKey] = aList[aKey];
    });

    for (let key in sortedList) {
      if (aIgnoreCookie && key == "Cookie") {
        continue;
      }

      /**
       * The following code creates the HTML:
       *
       * <span class="property-name">${line}:</span>
       * <span class="property-value">${aList[line]}</span><br>
       *
       * and adds it to parent.
       */
      let textNode = doc.createTextNode(key + ":");
      let span = doc.createElement("span");
      span.setAttribute("class", "property-name");
      span.appendChild(textNode);
      parent.appendChild(span);

      textNode = doc.createTextNode(sortedList[key]);
      span = doc.createElement("span");
      span.setAttribute("class", "property-value");
      span.appendChild(textNode);
      parent.appendChild(span);

      parent.appendChild(doc.createElement("br"));
    }
  },

  /**
   * Displays the node with id=aId.
   *
   * @param string aId
   * @returns void
   */
  _displayNode: function NP_displayNode(aId)
  {
    this.document.getElementById(aId).style.display = "block";
  },

  /**
   * Sets the request URL, request method, the timing information when the
   * request started and the request header content on the NetworkPanel.
   * If the request header contains cookie data, a list of sent cookies is
   * generated and a special sent cookie section is displayed + the cookie list
   * added to it.
   *
   * @returns void
   */
  _displayRequestHeader: function NP_displayRequestHeader()
  {
    let timing = this.httpActivity.timing;
    let request = this.httpActivity.request;

    this._appendTextNode("headUrl", this.httpActivity.url);
    this._appendTextNode("headMethod", this.httpActivity.method);

    this._appendTextNode("requestHeadersInfo",
      ConsoleUtils.timestampString(timing.REQUEST_HEADER/1000));

    this._appendList("requestHeadersContent", request.header, true);

    if ("Cookie" in request.header) {
      this._displayNode("requestCookie");

      let cookies = request.header.Cookie.split(";");
      let cookieList = {};
      let cookieListSorted = {};
      cookies.forEach(function(cookie) {
        let name, value;
        [name, value] = cookie.trim().split("=");
        cookieList[name] = value;
      });
      this._appendList("requestCookieContent", cookieList);
    }
  },

  /**
   * Displays the request body section of the NetworkPanel and set the request
   * body content on the NetworkPanel.
   *
   * @returns void
   */
  _displayRequestBody: function NP_displayRequestBody() {
    this._displayNode("requestBody");
    this._appendTextNode("requestBodyContent", this.httpActivity.request.body);
  },

  /*
   * Displays the `sent form data` section. Parses the request header for the
   * submitted form data displays it inside of the `sent form data` section.
   *
   * @returns void
   */
  _displayRequestForm: function NP_processRequestForm() {
    let requestBodyLines = this.httpActivity.request.body.split("\n");
    let formData = requestBodyLines[requestBodyLines.length - 1].
                      replace(/\+/g, " ").split("&");

    function unescapeText(aText)
    {
      try {
        return decodeURIComponent(aText);
      }
      catch (ex) {
        return decodeURIComponent(unescape(aText));
      }
    }

    let formDataObj = {};
    for (let i = 0; i < formData.length; i++) {
      let data = formData[i];
      let idx = data.indexOf("=");
      let key = data.substring(0, idx);
      let value = data.substring(idx + 1);
      formDataObj[unescapeText(key)] = unescapeText(value);
    }

    this._appendList("requestFormDataContent", formDataObj);
    this._displayNode("requestFormData");
  },

  /**
   * Displays the response section of the NetworkPanel, sets the response status,
   * the duration between the start of the request and the receiving of the
   * response header as well as the response header content on the the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseHeader: function NP_displayResponseHeader()
  {
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;

    this._appendTextNode("headStatus", response.status);

    let deltaDuration =
      Math.round((timing.RESPONSE_HEADER - timing.REQUEST_HEADER) / 1000);
    this._appendTextNode("responseHeadersInfo",
      this._format("durationMS", [deltaDuration]));

    this._displayNode("responseContainer");
    this._appendList("responseHeadersContent", response.header);
  },

  /**
   * Displays the respones image section, sets the source of the image displayed
   * in the image response section to the request URL and the duration between
   * the receiving of the response header and the end of the request. Once the
   * image is loaded, the size of the requested image is set.
   *
   * @returns void
   */
  _displayResponseImage: function NP_displayResponseImage()
  {
    let self = this;
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;
    let cached = "";

    if (this._isResponseCached) {
      cached = "Cached";
    }

    let imageNode = this.document.getElementById("responseImage" + cached +"Node");
    imageNode.setAttribute("src", this.httpActivity.url);

    // This function is called to set the imageInfo.
    function setImageInfo() {
      let deltaDuration =
        Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
      self._appendTextNode("responseImage" + cached + "Info",
        self._format("imageSizeDeltaDurationMS", [
          imageNode.width, imageNode.height, deltaDuration
        ]
      ));
    }

    // Check if the image is already loaded.
    if (imageNode.width != 0) {
      setImageInfo();
    }
    else {
      // Image is not loaded yet therefore add a load event.
      imageNode.addEventListener("load", function imageNodeLoad() {
        imageNode.removeEventListener("load", imageNodeLoad, false);
        setImageInfo();
      }, false);
    }

    this._displayNode("responseImage" + cached);
  },

  /**
   * Displays the response body section, sets the the duration between
   * the receiving of the response header and the end of the request as well as
   * the content of the response body on the NetworkPanel.
   *
   * @param [optional] string aCachedContent
   *        Cached content for this request. If this argument is set, the
   *        responseBodyCached section is displayed.
   * @returns void
   */
  _displayResponseBody: function NP_displayResponseBody(aCachedContent)
  {
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;
    let cached =  "";
    if (aCachedContent) {
      cached = "Cached";
    }

    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseBody" + cached + "Info",
      this._format("durationMS", [deltaDuration]));

    this._displayNode("responseBody" + cached);
    this._appendTextNode("responseBody" + cached + "Content",
                            aCachedContent || response.body);
  },

  /**
   * Displays the `Unknown Content-Type hint` and sets the duration between the
   * receiving of the response header on the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseBodyUnknownType: function NP_displayResponseBodyUnknownType()
  {
    let timing = this.httpActivity.timing;

    this._displayNode("responseBodyUnknownType");
    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseBodyUnknownTypeInfo",
      this._format("durationMS", [deltaDuration]));

    this._appendTextNode("responseBodyUnknownTypeContent",
      this._format("responseBodyUnableToDisplay.content", [this._contentType]));
  },

  /**
   * Displays the `no response body` section and sets the the duration between
   * the receiving of the response header and the end of the request.
   *
   * @returns void
   */
  _displayNoResponseBody: function NP_displayNoResponseBody()
  {
    let timing = this.httpActivity.timing;

    this._displayNode("responseNoBody");
    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseNoBodyInfo",
      this._format("durationMS", [deltaDuration]));
  },

  /*
   * Calls the isDoneCallback function if one is specified.
   */
  _callIsDone: function() {
    if (this.isDoneCallback) {
      this.isDoneCallback();
    }
  },

  /**
   * Updates the content of the NetworkPanel's iframe.
   *
   * @returns void
   */
  update: function NP_update()
  {
    /**
     * After the iframe's contentWindow is ready, the document object is set.
     * If the document object isn't set yet, then the page is loaded and nothing
     * can be updated.
     */
    if (!this.document) {
      return;
    }

    let timing = this.httpActivity.timing;
    let request = this.httpActivity.request;
    let response = this.httpActivity.response;

    switch (this._state) {
      case this._INIT:
        this._displayRequestHeader();
        this._state = this._DISPLAYED_REQUEST_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_HEADER:
        // Process the request body if there is one.
        if (!request.bodyDiscarded && request.body) {
          // Check if we send some form data. If so, display the form data special.
          if (this._isRequestBodyFormData) {
            this._displayRequestForm();
          }
          else {
            this._displayRequestBody();
          }
          this._state = this._DISPLAYED_REQUEST_BODY;
        }
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_BODY:
        // There is always a response header. Therefore we can skip here if
        // we don't have a response header yet and don't have to try updating
        // anything else in the NetworkPanel.
        if (!response.header) {
          break
        }
        this._displayResponseHeader();
        this._state = this._DISPLAYED_RESPONSE_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_RESPONSE_HEADER:
        // Check if the transition is done.
        if (timing.TRANSACTION_CLOSE && response.isDone) {
          if (response.bodyDiscarded) {
            this._callIsDone();
          }
          else if (this._responseIsImage) {
            this._displayResponseImage();
            this._callIsDone();
          }
          else if (!this._isResponseBodyTextData) {
            this._displayResponseBodyUnknownType();
            this._callIsDone();
          }
          else if (response.body) {
            this._displayResponseBody();
            this._callIsDone();
          }
          else if (this._isResponseCached) {
            let self = this;
            NetworkHelper.loadFromCache(this.httpActivity.url,
                                        this.httpActivity.charset,
                                        function(aContent) {
              // If some content could be loaded from the cache, then display
              // the body.
              if (aContent) {
                self._displayResponseBody(aContent);
                self._callIsDone();
              }
              // Otherwise, show the "There is no response body" hint.
              else {
                self._displayNoResponseBody();
                self._callIsDone();
              }
            });
          }
          else {
            this._displayNoResponseBody();
            this._callIsDone();
          }
          this._state = this._TRANSITION_CLOSED;
        }
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//// Private utility functions for the HUD service

/**
 * Destroys lines of output if more lines than the allowed log limit are
 * present.
 *
 * @param nsIDOMNode aConsoleNode
 *        The DOM node that holds the output of the console.
 * @returns void
 */
function pruneConsoleOutputIfNecessary(aConsoleNode)
{
  let logLimit;
  try {
    let prefBranch = Services.prefs.getBranch("devtools.hud.");
    logLimit = prefBranch.getIntPref("loglimit");
  } catch (e) {
    logLimit = DEFAULT_LOG_LIMIT;
  }

  let messageNodes = aConsoleNode.querySelectorAll(".hud-msg-node");
  for (let i = 0; i < messageNodes.length - logLimit; i++) {
    let messageNode = messageNodes[i];
    let groupNode = messageNode.parentNode;
    if (!groupNode.classList.contains("hud-group")) {
      throw new Error("pruneConsoleOutputIfNecessary: message node not in a " +
                      "HUD group");
    }

    groupNode.removeChild(messageNode);

    // If there are no more children, then remove the group itself.
    if (!groupNode.querySelector(".hud-msg-node")) {
      groupNode.parentNode.removeChild(groupNode);
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  // TODO: provide mixins for FENNEC: bug 568621
  if (appName() == "FIREFOX") {
    var mixins = new FirefoxApplicationHooks();
  }
  else {
    throw new Error("Unsupported Application");
  }

  this.mixins = mixins;
  this.storage = new ConsoleStorage();
  this.defaultFilterPrefs = this.storage.defaultDisplayPrefs;
  this.defaultGlobalConsolePrefs = this.storage.defaultGlobalConsolePrefs;

  // These methods access the "this" object, but they're registered as
  // event listeners. So we hammer in the "this" binding.
  this.onTabClose = this.onTabClose.bind(this);
  this.onWindowUnload = this.onWindowUnload.bind(this);

  // begin observing HTTP traffic
  this.startHTTPObservation();
};

HUD_SERVICE.prototype =
{
  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HS_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns (format) string
   */
  getFormatStr: function HS_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * getter for UI commands to be used by the frontend
   *
   * @returns object
   */
  get consoleUI() {
    return HeadsUpDisplayUICommands;
  },

  /**
   * Collection of HUDIds that map to the tabs/windows/contexts
   * that a HeadsUpDisplay can be activated for.
   */
  activatedContexts: [],

  /**
   * Registry of HeadsUpDisplay DOM node ids
   */
  _headsUpDisplays: {},

  /**
   * Mapping of HUDIds to URIspecs
   */
  displayRegistry: {},

  /**
   * Mapping of HUDIds to contentWindows.
   */
  windowRegistry: {},

  /**
   * Mapping of URISpecs to HUDIds
   */
  uriRegistry: {},

  /**
   * The sequencer is a generator (after initialization) that returns unique
   * integers
   */
  sequencer: null,

  /**
   * Each HeadsUpDisplay has a set of filter preferences
   */
  filterPrefs: {},

  /**
   * Gets the ID of the outer window of this DOM window
   *
   * @param nsIDOMWindow aWindow
   * @returns integer
   */
  getWindowId: function HS_getWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },

  /**
   * Gets the top level content window that has an outer window with
   * the given ID or returns null if no such content window exists
   *
   * @param integer aId
   * @returns nsIDOMWindow
   */
  getWindowByWindowId: function HS_getWindowByWindowId(aId)
  {
    let someWindow = Services.wm.getMostRecentWindow(null);
    let windowUtils = someWindow.getInterface(Ci.nsIDOMWindowUtils);
    return windowUtils.getOuterWindowWithId(aId);
  },

  /**
   * Whether to save the bodies of network requests and responses. Disabled by
   * default to save memory.
   */
  saveRequestAndResponseBodies: false,

  /**
   * Tell the HUDService that a HeadsUpDisplay can be activated
   * for the window or context that has 'aContextDOMId' node id
   *
   * @param string aContextDOMId
   * @return void
   */
  registerActiveContext: function HS_registerActiveContext(aContextDOMId)
  {
    this.activatedContexts.push(aContextDOMId);
  },

  /**
   * Firefox-specific current tab getter
   *
   * @returns nsIDOMWindow
   */
  currentContext: function HS_currentContext() {
    return this.mixins.getCurrentContext();
  },

  /**
   * Tell the HUDService that a HeadsUpDisplay should be deactivated
   *
   * @param string aContextDOMId
   * @return void
   */
  unregisterActiveContext: function HS_deregisterActiveContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    var idx = this.activatedContexts.indexOf(domId);
    if (idx > -1) {
      this.activatedContexts.splice(idx, 1);
    }
  },

  /**
   * Tells callers that a HeadsUpDisplay can be activated for the context
   *
   * @param string aContextDOMId
   * @return boolean
   */
  canActivateContext: function HS_canActivateContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    for (var idx in this.activatedContexts) {
      if (this.activatedContexts[idx] == domId){
        return true;
      }
    }
    return false;
  },

  /**
   * Activate a HeadsUpDisplay for the given tab context.
   *
   * @param Element aContext the tab element.
   * @returns void
   */
  activateHUDForContext: function HS_activateHUDForContext(aContext)
  {
    var window = aContext.linkedBrowser.contentWindow;
    var id = aContext.linkedBrowser.parentNode.parentNode.getAttribute("id");
    this.registerActiveContext(id);
    HUDService.windowInitializer(window);
  },

  /**
   * Deactivate a HeadsUpDisplay for the given tab context.
   *
   * @param nsIDOMWindow aContext
   * @returns void
   */
  deactivateHUDForContext: function HS_deactivateHUDForContext(aContext)
  {
    let window = aContext.linkedBrowser.contentWindow;
    let nBox = aContext.ownerDocument.defaultView.
      getNotificationBox(window);
    let hudId = "hud_" + nBox.id;
    let displayNode = nBox.querySelector("#" + hudId);

    if (hudId in this.displayRegistry && displayNode) {
    this.unregisterActiveContext(hudId);
      this.unregisterDisplay(displayNode);
    window.focus();
    }
  },

  /**
   * Clear the specified HeadsUpDisplay
   *
   * @param string|nsIDOMNode aHUD
   *        Either the ID of a HUD or the DOM node corresponding to an outer
   *        HUD box.
   * @returns void
   */
  clearDisplay: function HS_clearDisplay(aHUD)
  {
    if (typeof(aHUD) === "string") {
      aHUD = this.getOutputNodeById(aHUD);
    }

    var outputNode = aHUD.querySelector(".hud-output-node");

    while (outputNode.firstChild) {
      outputNode.removeChild(outputNode.firstChild);
    }

    outputNode.lastTimestamp = 0;
  },

  /**
   * get a unique ID from the sequence generator
   *
   * @returns integer
   */
  sequenceId: function HS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer(-1);
    }
    return this.sequencer.next();
  },

  /**
   * get the default filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getDefaultFilterPrefs: function HS_getDefaultFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the current filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getFilterPrefs: function HS_getFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @returns boolean
   */
  getFilterState: function HS_getFilterState(aHUDId, aToggleType)
  {
    if (!aHUDId) {
      return false;
    }
    try {
      var bool = this.filterPrefs[aHUDId][aToggleType];
      return bool;
    }
    catch (ex) {
      return false;
    }
  },

  /**
   * set the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @param boolean aState
   * @returns void
   */
  setFilterState: function HS_setFilterState(aHUDId, aToggleType, aState)
  {
    this.filterPrefs[aHUDId][aToggleType] = aState;
    this.adjustVisibilityForMessageType(aHUDId, aToggleType, aState);
  },

  /**
   * Temporarily lifts the subtree rooted at the given node out of the DOM for
   * the duration of the supplied callback. This allows DOM mutations performed
   * inside the callback to avoid triggering reflows.
   *
   * @param nsIDOMNode aNode
   *        The node to remove from the tree.
   * @param function aCallback
   *        The callback, which should take no parameters. The return value of
   *        the callback, if any, is ignored.
   * @returns void
   */
  liftNode: function(aNode, aCallback) {
    let parentNode = aNode.parentNode;
    let siblingNode = aNode.nextSibling;
    parentNode.removeChild(aNode);
    aCallback();
    parentNode.insertBefore(aNode, siblingNode);
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the message type filter named by @aMessageType.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aMessageType
   *        The message type being filtered ("network", "css", etc.)
   * @param boolean aState
   *        True if the filter named by @aMessageType is being turned on; false
   *        otherwise.
   * @returns void
   */
  adjustVisibilityForMessageType:
  function HS_adjustVisibilityForMessageType(aHUDId, aMessageType, aState)
  {
    let displayNode = this.getOutputNodeById(aHUDId);
    let outputNode = displayNode.querySelector(".hud-output-node");
    let doc = outputNode.ownerDocument;

    this.liftNode(outputNode, function() {
      let xpath = ".//*[contains(@class, 'hud-msg-node') and " +
        "contains(@class, 'hud-" + aMessageType + "')]";
      let result = doc.evaluate(xpath, outputNode, null,
        Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
      for (let i = 0; i < result.snapshotLength; i++) {
        if (aState) {
          result.snapshotItem(i).classList.remove("hud-filtered-by-type");
        } else {
          result.snapshotItem(i).classList.add("hud-filtered-by-type");
        }
      }
    });
  },

  /**
   * Returns the source code of the XPath contains() function necessary to
   * match the given query string.
   *
   * @param string The query string to convert.
   * @returns string
   */
  buildXPathFunctionForString: function HS_buildXPathFunctionForString(aStr)
  {
    let words = aStr.split(/\s+/), results = [];
    for (let i = 0; i < words.length; i++) {
      let word = words[i];
      if (word === "") {
        continue;
      }

      let result;
      if (word.indexOf('"') === -1) {
        result = '"' + word + '"';
      }
      else if (word.indexOf("'") === -1) {
        result = "'" + word + "'";
      }
      else {
        result = 'concat("' + word.replace(/"/g, "\", '\"', \"") + '")';
      }

      results.push("contains(translate(., 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz'), " + result.toLowerCase() + ")");
    }

    return (results.length === 0) ? "true()" : results.join(" and ");
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the search string.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aSearchString
   *        The new search string.
   * @returns void
   */
  adjustVisibilityOnSearchStringChange:
  function HS_adjustVisibilityOnSearchStringChange(aHUDId, aSearchString)
  {
    let fn = this.buildXPathFunctionForString(aSearchString);
    let displayNode = this.getOutputNodeById(aHUDId);
    let outputNode = displayNode.querySelector(".hud-output-node");
    let doc = outputNode.ownerDocument;
    this.liftNode(outputNode, function() {
      let xpath = './/*[contains(@class, "hud-msg-node") and ' +
        'not(contains(@class, "hud-filtered-by-string")) and not(' + fn + ')]';
      let result = doc.evaluate(xpath, outputNode, null,
        Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
      for (let i = 0; i < result.snapshotLength; i++) {
        result.snapshotItem(i).classList.add("hud-filtered-by-string");
      }

      xpath = './/*[contains(@class, "hud-msg-node") and contains(@class, ' +
        '"hud-filtered-by-string") and ' + fn + ']';
      result = doc.evaluate(xpath, outputNode, null,
        Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
      for (let i = 0; i < result.snapshotLength; i++) {
        result.snapshotItem(i).classList.remove("hud-filtered-by-string");
      }
    });
  },

  /**
   * Makes a newly-inserted node invisible if the user has filtered it out.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param nsIDOMNode aNewNode
   *        The newly-inserted console message.
   * @returns void
   */
  adjustVisibilityForNewlyInsertedNode:
  function HS_adjustVisibilityForNewlyInsertedNode(aHUDId, aNewNode) {
    // Filter on the search string.
    let searchString = this.getFilterStringByHUDId(aHUDId);
    let xpath = ".[" + this.buildXPathFunctionForString(searchString) + "]";
    let doc = aNewNode.ownerDocument;
    let result = doc.evaluate(xpath, aNewNode, null,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
    if (result.snapshotLength === 0) {
      // The string filter didn't match, so the node is filtered.
      aNewNode.classList.add("hud-filtered-by-string");
    }

    // Filter by the message type.
    let classes = aNewNode.classList;
    let msgType = null;
    for (let i = 0; i < classes.length; i++) {
      let klass = classes.item(i);
      if (klass !== "hud-msg-node" && klass.indexOf("hud-") === 0) {
        msgType = klass.substring(4);   // Strip off "hud-".
        break;
      }
    }
    if (msgType !== null && !this.getFilterState(aHUDId, msgType)) {
      // The node is filtered by type.
      aNewNode.classList.add("hud-filtered-by-type");
    }
  },

  /**
   * Keeps a reference for each HeadsUpDisplay that is created
   */
  hudReferences: {},

  /**
   * Register a reference of each HeadsUpDisplay that is created
   */
  registerHUDReference:
  function HS_registerHUDReference(aHUD)
  {
    this.hudReferences[aHUD.hudId] = aHUD;
  },

  /**
   * Deletes a HeadsUpDisplay object from memory
   *
   * @param string aHUDId
   * @returns void
   */
  deleteHeadsUpDisplay: function HS_deleteHeadsUpDisplay(aHUDId)
  {
    delete this.hudReferences[aHUDId];
  },

  /**
   * Register a new Heads Up Display
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  registerDisplay: function HS_registerDisplay(aHUDId, aContentWindow)
  {
    // register a display DOM node Id and HUD uriSpec with the service

    if (!aHUDId || !aContentWindow){
      throw new Error(ERRORS.MISSING_ARGS);
    }
    var URISpec = aContentWindow.document.location.href;
    this.filterPrefs[aHUDId] = this.defaultFilterPrefs;
    this.displayRegistry[aHUDId] = URISpec;

    this._headsUpDisplays[aHUDId] = { id: aHUDId };

    this.registerActiveContext(aHUDId);
    // init storage objects:
    this.storage.createDisplay(aHUDId);

    var huds = this.uriRegistry[URISpec];
    var foundHUDId = false;

    if (huds) {
      var len = huds.length;
      for (var i = 0; i < len; i++) {
        if (huds[i] == aHUDId) {
          foundHUDId = true;
          break;
        }
      }
      if (!foundHUDId) {
        this.uriRegistry[URISpec].push(aHUDId);
      }
    }
    else {
      this.uriRegistry[URISpec] = [aHUDId];
    }

    var windows = this.windowRegistry[aHUDId];
    if (!windows) {
      this.windowRegistry[aHUDId] = [aContentWindow];
    }
    else {
      windows.push(aContentWindow);
    }
  },

  /**
   * When a display is being destroyed, unregister it first
   *
   * @param string|nsIDOMNode aHUD
   *        Either the ID of a HUD or the DOM node corresponding to an outer
   *        HUD box.
   * @returns void
   */
  unregisterDisplay: function HS_unregisterDisplay(aHUD)
  {
    // Remove children from the output. If the output is not cleared, there can
    // be leaks as some nodes has node.onclick = function; set and GC can't
    // remove the nodes then.
    HUDService.clearDisplay(aHUD);

    var id, outputNode;
    if (typeof(aHUD) === "string") {
      id = aHUD;
      outputNode = this.mixins.getOutputNodeById(aHUD);
    }
    else {
      id = aHUD.getAttribute("id");
      outputNode = aHUD;
    }

    // remove HUD DOM node and
    // remove display references from local registries get the outputNode
    var parent = outputNode.parentNode;
    var splitters = parent.querySelectorAll("splitter");
    var len = splitters.length;
    for (var i = 0; i < len; i++) {
      if (splitters[i].getAttribute("class") == "hud-splitter") {
        splitters[i].parentNode.removeChild(splitters[i]);
        break;
      }
    }
    // remove the DOM Nodes
    parent.removeChild(outputNode);

    // remove our record of the DOM Nodes from the registry
    delete this._headsUpDisplays[id];
    // remove the HeadsUpDisplay object from memory
    this.deleteHeadsUpDisplay(id);
    // remove the related storage object
    this.storage.removeDisplay(id);
    // remove the related window objects
    delete this.windowRegistry[id];

    let displays = this.displays();

    var uri  = this.displayRegistry[id];
    var specHudArr = this.uriRegistry[uri];

    for (var i = 0; i < specHudArr.length; i++) {
      if (specHudArr[i] == id) {
        specHudArr.splice(i, 1);
      }
    }
    delete displays[id];
    delete this.displayRegistry[id];
    delete this.uriRegistry[uri];
  },

  /**
   * Shutdown all HeadsUpDisplays on xpcom-shutdown
   *
   * @returns void
   */
  shutdown: function HS_shutdown()
  {
    for (var displayId in this._headsUpDisplays) {
      this.unregisterDisplay(displayId);
    }
    // delete the storage as it holds onto channels
    delete this.storage;
  },

  /**
   * Returns the hudId that is corresponding to the hud activated for the
   * passed aContentWindow. If there is no matching hudId null is returned.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns string or null
   */
  getHudIdByWindow: function HS_getHudIdByWindow(aContentWindow)
  {
    // Fast path: check the cached window registry.
    for (let hudId in this.windowRegistry) {
      if (this.windowRegistry[hudId] &&
          this.windowRegistry[hudId].indexOf(aContentWindow) != -1) {
        return hudId;
      }
    }

    // As a fallback, do a little pointer chasing to try to find the Web
    // Console. This fallback approach occurs when opening the Console on a
    // page with subframes.
    let [ , tabBrowser, browser ] = ConsoleUtils.getParents(aContentWindow);
    if (!tabBrowser) {
      return null;
    }

    let notificationBox = tabBrowser.getNotificationBox(browser);
    let hudBox = notificationBox.querySelector(".hud-box");
    if (!hudBox) {
      return null;
    }

    // Cache it!
    let hudId = hudBox.id;
    this.windowRegistry[hudId].push(aContentWindow);

    let uri = aContentWindow.document.location.href;
    if (!this.uriRegistry[uri]) {
      this.uriRegistry[uri] = [ hudId ];
    } else {
      this.uriRegistry[uri].push(hudId);
    }

    return hudId;
  },

  /**
   * Gets HUD DOM Node
   * @param string id
   *        The Heads Up Display DOM Id
   * @returns nsIDOMNode
   */
  getHeadsUpDisplay: function HS_getHeadsUpDisplay(aId)
  {
    return this.mixins.getOutputNodeById(aId);
  },

  /**
   * gets the nsIDOMNode outputNode by ID via the gecko app mixins
   *
   * @param string aId
   * @returns nsIDOMNode
   */
  getOutputNodeById: function HS_getOutputNodeById(aId)
  {
    return this.mixins.getOutputNodeById(aId);
  },

  /**
   * Gets an object that contains active DOM Node Ids for all Heads Up Displays
   *
   * @returns object
   */
  displays: function HS_displays() {
    return this._headsUpDisplays;
  },

  /**
   * Gets an array that contains active DOM Node Ids for all HUDs
   * @returns array
   */
  displaysIndex: function HS_displaysIndex()
  {
    var props = [];
    for (var prop in this._headsUpDisplays) {
      props.push(prop);
    }
    return props;
  },

  /**
   * get the current filter string for the HeadsUpDisplay
   *
   * @param string aHUDId
   * @returns string
   */
  getFilterStringByHUDId: function HS_getFilterStringbyHUDId(aHUDId) {
    var hud = this.getHeadsUpDisplay(aHUDId);
    var filterStr = hud.querySelectorAll(".hud-filter-box")[0].value;
    return filterStr;
  },

  /**
   * Update the filter text in the internal tracking object for all
   * filter strings
   *
   * @param nsIDOMNode aTextBoxNode
   * @returns void
   */
  updateFilterText: function HS_updateFiltertext(aTextBoxNode)
  {
    var hudId = aTextBoxNode.getAttribute("hudId");
    this.adjustVisibilityOnSearchStringChange(hudId, aTextBoxNode.value);
  },

  /**
   * Logs a HUD-generated console message
   * @param object aMessage
   *        The message to log, which is a JS object, this is the
   *        "raw" log message
   * @param nsIDOMNode aConsoleNode
   *        The output DOM node to log the messageNode to
   * @param nsIDOMNode aMessageNode
   *        The message DOM Node that will be appended to aConsoleNode
   * @returns void
   */
  logHUDMessage: function HS_logHUDMessage(aMessage,
                                           aConsoleNode,
                                           aMessageNode)
  {
    if (!aMessage) {
      throw new Error(ERRORS.MISSING_ARGS);
    }

    let lastGroupNode = this.appendGroupIfNecessary(aConsoleNode,
                                                    aMessage.timestamp);

    lastGroupNode.appendChild(aMessageNode);
    ConsoleUtils.scrollToVisible(aMessageNode);

    // store this message in the storage module:
    this.storage.recordEntry(aMessage.hudId, aMessage);

    pruneConsoleOutputIfNecessary(aConsoleNode);
  },

  /**
   * Logs a message to the Heads Up Display that originates
   * from the window.console API
   *
   * @param aHudId
   * @param string aLevel one of log/info/warn/error
   * @param string aArguments
   */
  logConsoleAPIMessage: function HS_logConsoleAPIMessage(aHudId,
                                                         aLevel,
                                                         aArguments)
  {
    let hud = this.hudReferences[aHudId];
    let messageNode = hud.makeXULNode("label");
    let klass = "hud-msg-node hud-" + aLevel;
    messageNode.setAttribute("class", klass);

    let message = Array.join(aArguments, " ") + "\n";
    let ts = ConsoleUtils.timestamp();
    let timestampedMessage = ConsoleUtils.timestampString(ts) + ": " + message;
    messageNode.appendChild(hud.chromeDocument.createTextNode(timestampedMessage));

    let messageObject = {
      logLevel: aLevel,
      hudId: aHudId,
      message: message,
      timestamp: ts,
      origin: "WebConsole"
    };
    this.logMessage(messageObject, hud.outputNode, messageNode);
  },

  /**
   * Logs a Message.
   * @param aMessage
   *        The message to log, which is a JS object, this is the
   *        "raw" log message
   * @param aConsoleNode
   *        The output DOM node to log the messageNode to
   * @param The message DOM Node that will be appended to aConsoleNode
   * @returns void
   */
  logMessage: function HS_logMessage(aMessage, aConsoleNode, aMessageNode)
  {
    if (!aMessage) {
      throw new Error(ERRORS.MISSING_ARGS);
    }

    switch (aMessage.origin) {
      case "network":
      case "WebConsole":
      case "console-listener":
        this.logHUDMessage(aMessage, aConsoleNode, aMessageNode);
        break;
      default:
        // noop
        break;
    }
  },

  /**
   * Get OutputNode by Id
   *
   * @param string aId
   * @returns nsIDOMNode
   */
  getConsoleOutputNode: function HS_getConsoleOutputNode(aId)
  {
    let displayNode = this.getHeadsUpDisplay(aId);
    return displayNode.querySelector(".hud-output-node");
  },

  /**
   * Inform user that the Web Console API has been replaced by a script
   * in a content page.
   *
   * @param string aHUDId
   * @returns void
   */
  logWarningAboutReplacedAPI:
  function HS_logWarningAboutReplacedAPI(aHUDId)
  {
    let domId = "hud-log-node-" + this.sequenceId();
    let outputNode = this.getConsoleOutputNode(aHUDId);

    let msgFormat = {
      logLevel: "error",
      activityObject: {},
      hudId: aHUDId,
      origin: "console-listener",
      domId: domId,
      message: this.getStr("ConsoleAPIDisabled"),
    };

    let messageObject =
    this.messageFactory(msgFormat, "error", outputNode, msgFormat.activityObject);
    this.logMessage(messageObject.messageObject, outputNode, messageObject.messageNode);
  },

  /**
   * Register a Gecko app's specialized ApplicationHooks object
   *
   * @returns void or throws "UNSUPPORTED APPLICATION" error
   */
  registerApplicationHooks:
  function HS_registerApplications(aAppName, aHooksObject)
  {
    switch(aAppName) {
      case "FIREFOX":
        this.applicationHooks = aHooksObject;
        return;
      default:
        throw new Error("MOZ APPLICATION UNSUPPORTED");
    }
  },

  /**
   * Registry of ApplicationHooks used by specified Gecko Apps
   *
   * @returns Specific Gecko 'ApplicationHooks' Object/Mixin
   */
  applicationHooks: null,

  getChromeWindowFromContentWindow:
  function HS_getChromeWindowFromContentWindow(aContentWindow)
  {
    if (!aContentWindow) {
      throw new Error("Cannot get contentWindow via nsILoadContext");
    }
    var win = aContentWindow.QueryInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
      .QueryInterface(Ci.nsIDocShellTreeItem)
      .rootTreeItem
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindow)
      .QueryInterface(Ci.nsIDOMChromeWindow);
    return win;
  },

  /**
   * Requests that haven't finished yet.
   */
  openRequests: {},

  /**
   * Assign a function to this property to listen for finished httpRequests.
   * Used by unit tests.
   */
  lastFinishedRequestCallback: null,

  /**
   * Opens a NetworkPanel.
   *
   * @param nsIDOMNode aNode
   *        DOMNode to display the panel next to.
   * @param object aHttpActivity
   *        httpActivity object. The data of this object is displayed in the
   *        NetworkPanel.
   * @returns NetworkPanel
   */
  openNetworkPanel: function HS_openNetworkPanel(aNode, aHttpActivity)
  {
    let doc = aNode.ownerDocument;
    let parent = doc.getElementById("mainPopupSet");
    let netPanel = new NetworkPanel(parent, aHttpActivity);
    netPanel.linkNode = aNode;

    let panel = netPanel.panel;
    panel.openPopup(aNode, "after_pointer", 0, 0, false, false);
    panel.sizeTo(350, 400);
    aHttpActivity.panels.push(Cu.getWeakReference(netPanel));
    return netPanel;
  },

  /**
   * Begin observing HTTP traffic that we care about,
   * namely traffic that originates inside any context that a Heads Up Display
   * is active for.
   */
  startHTTPObservation: function HS_httpObserverFactory()
  {
    // creates an observer for http traffic
    var self = this;
    var httpObserver = {
      observeActivity :
      function HS_SHO_observeActivity(aChannel,
                                      aActivityType,
                                      aActivitySubtype,
                                      aTimestamp,
                                      aExtraSizeData,
                                      aExtraStringData)
      {
        if (aActivityType ==
              activityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION ||
            aActivityType ==
              activityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {

          aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

          let transCodes = this.httpTransactionCodes;
          let hudId;

          if (aActivitySubtype ==
              activityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER ) {
            // Try to get the source window of the request.
            let win = NetworkHelper.getWindowForRequest(aChannel);
            if (!win) {
              return;
            }

            // Try to get the hudId that is associated to the window.
            hudId = self.getHudIdByWindow(win);
            if (!hudId) {
              return;
            }

            // The httpActivity object will hold all information concerning
            // this request and later response.

            let httpActivity = {
              id: self.sequenceId(),
              hudId: hudId,
              url: aChannel.URI.spec,
              method: aChannel.requestMethod,
              channel: aChannel,
              charset: win.document.characterSet,

              panels: [],
              request: {
                header: { }
              },
              response: {
                header: null
              },
              timing: {
                "REQUEST_HEADER": aTimestamp
              }
            };

            // Add a new output entry.
            let loggedNode = self.logActivity("network", hudId, httpActivity);

            // In some cases loggedNode can be undefined (e.g. if an image was
            // requested). Don't continue in such a case.
            if (!loggedNode) {
              return;
            }

            // Add listener for the response body.
            let newListener = new ResponseListener(httpActivity);
            aChannel.QueryInterface(Ci.nsITraceableChannel);
            newListener.originalListener = aChannel.setNewListener(newListener);
            httpActivity.response.listener = newListener;

            // Copy the request header data.
            aChannel.visitRequestHeaders({
              visitHeader: function(aName, aValue) {
                httpActivity.request.header[aName] = aValue;
              }
            });

            // Store the loggedNode and the httpActivity object for later reuse.
            httpActivity.messageObject = loggedNode;
            self.openRequests[httpActivity.id] = httpActivity;

            // Make the network span clickable.
            let linkNode = loggedNode.messageNode;
            linkNode.setAttribute("aria-haspopup", "true");
            linkNode.addEventListener("mousedown", function(aEvent) {
              this._startX = aEvent.clientX;
              this._startY = aEvent.clientY;
            }, false);

            linkNode.addEventListener("click", function(aEvent) {
              if (aEvent.detail != 1 || aEvent.button != 0 ||
                  (this._startX != aEvent.clientX &&
                   this._startY != aEvent.clientY)) {
                return;
              }

              if (!this._panelOpen) {
                self.openNetworkPanel(this, httpActivity);
                this._panelOpen = true;
              }
            }, false);
          }
          else {
            // Iterate over all currently ongoing requests. If aChannel can't
            // be found within them, then exit this function.
            let httpActivity = null;
            for each (var item in self.openRequests) {
              if (item.channel !== aChannel) {
                continue;
              }
              httpActivity = item;
              break;
            }

            if (!httpActivity) {
              return;
            }

            let msgObject, updatePanel = false;
            let data, textNode;
            // Store the time information for this activity subtype.
            httpActivity.timing[transCodes[aActivitySubtype]] = aTimestamp;

            switch (aActivitySubtype) {
              case activityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT:
                if (!self.saveRequestAndResponseBodies) {
                  httpActivity.request.bodyDiscarded = true;
                  break;
                }

                let gBrowser = HUDService.currentContext().gBrowser;

                let sentBody = NetworkHelper.readPostTextFromRequest(
                                aChannel, gBrowser);
                if (!sentBody) {
                  // If the request URL is the same as the current page url, then
                  // we can try to get the posted text from the page directly.
                  // This check is necessary as otherwise the
                  //   NetworkHelper.readPostTextFromPage
                  // function is called for image requests as well but these
                  // are not web pages and as such don't store the posted text
                  // in the cache of the webpage.
                  if (httpActivity.url == gBrowser.contentWindow.location.href) {
                    sentBody = NetworkHelper.readPostTextFromPage(gBrowser);
                  }
                  if (!sentBody) {
                    sentBody = "";
                  }
                }
                httpActivity.request.body = sentBody;
                break;

              case activityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER:
                msgObject = httpActivity.messageObject;

                // aExtraStringData contains the response header. The first line
                // contains the response status (e.g. HTTP/1.1 200 OK).
                //
                // Note: The response header is not saved here. Calling the
                //       aChannel.visitResponseHeaders at this point sometimes
                //       causes an NS_ERROR_NOT_AVAILABLE exception. Therefore,
                //       the response header and response body is stored on the
                //       httpActivity object within the RL_onStopRequest function.
                httpActivity.response.status =
                  aExtraStringData.split(/\r\n|\n|\r/)[0];

                // Remove the textNode from the messageNode and add a new one
                // that contains the respond http status.
                textNode = msgObject.messageNode.firstChild;
                textNode.parentNode.removeChild(textNode);

                data = [ httpActivity.url,
                         httpActivity.response.status ];

                msgObject.messageNode.appendChild(
                  msgObject.textFactory(
                    msgObject.prefix +
                    self.getFormatStr("networkUrlWithStatus", data) + "\n"));

                break;

              case activityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE:
                msgObject = httpActivity.messageObject;


                let timing = httpActivity.timing;
                let requestDuration =
                  Math.round((timing.RESPONSE_COMPLETE -
                                timing.REQUEST_HEADER) / 1000);

                // Remove the textNode from the messageNode and add a new one
                // that contains the request duration.
                textNode = msgObject.messageNode.firstChild;
                textNode.parentNode.removeChild(textNode);

                data = [ httpActivity.url,
                         httpActivity.response.status,
                         requestDuration ];

                msgObject.messageNode.appendChild(
                  msgObject.textFactory(
                    msgObject.prefix +
                    self.getFormatStr("networkUrlWithStatusAndDuration",
                                      data) + "\n"));

                delete self.openRequests[item.id];
                updatePanel = true;
                break;
            }

            if (updatePanel) {
              httpActivity.panels.forEach(function(weakRef) {
                let panel = weakRef.get();
                if (panel) {
                  panel.update();
                }
              });
            }
          }
        }
      },

      httpTransactionCodes: {
        0x5001: "REQUEST_HEADER",
        0x5002: "REQUEST_BODY_SENT",
        0x5003: "RESPONSE_START",
        0x5004: "RESPONSE_HEADER",
        0x5005: "RESPONSE_COMPLETE",
        0x5006: "TRANSACTION_CLOSE",

        0x804b0003: "STATUS_RESOLVING",
        0x804b0007: "STATUS_CONNECTING_TO",
        0x804b0004: "STATUS_CONNECTED_TO",
        0x804b0005: "STATUS_SENDING_TO",
        0x804b000a: "STATUS_WAITING_FOR",
        0x804b0006: "STATUS_RECEIVING_FROM"
      }
    };

    activityDistributor.addObserver(httpObserver);
  },

  /**
   * Logs network activity.
   *
   * @param string aType
   *        The severity of the message.
   * @param object aActivityObject
   *        The activity to log.
   * @returns void
   */
  logNetActivity: function HS_logNetActivity(aType, aActivityObject)
  {
    var outputNode, hudId;
    try {
      hudId = aActivityObject.hudId;
      outputNode = this.getHeadsUpDisplay(hudId).
                                  querySelector(".hud-output-node");

      // get an id to attach to the dom node for lookup of node
      // when updating the log entry with additional http transactions
      var domId = "hud-log-node-" + this.sequenceId();

      var message = { logLevel: aType,
                      activityObj: aActivityObject,
                      hudId: hudId,
                      origin: "network",
                      domId: domId,
                    };
      var msgType = this.getStr("typeNetwork");
      var msg = msgType + " " +
        aActivityObject.method +
        " " +
        aActivityObject.url;
      message.message = msg;

      var messageObject =
        this.messageFactory(message, aType, outputNode, aActivityObject);

      var timestampedMessage = messageObject.timestampedMessage;
      var urlIdx = timestampedMessage.indexOf(aActivityObject.url);
      messageObject.prefix = timestampedMessage.substring(0, urlIdx);

      messageObject.messageNode.classList.add("hud-clickable");
      messageObject.messageNode.setAttribute("crop", "end");

      this.logMessage(messageObject.messageObject, outputNode, messageObject.messageNode);
      return messageObject;
    }
    catch (ex) {
      Cu.reportError(ex);
    }
  },

  /**
   * Logs console listener activity.
   *
   * @param string aHUDId
   *        The ID of the HUD to which to send the message.
   * @param object aActivityObject
   *        The message to log.
   * @returns void
   */
  logConsoleActivity: function HS_logConsoleActivity(aHUDId, aActivityObject)
  {
    var _msgLogLevel = this.scriptMsgLogLevel[aActivityObject.flags];
    var msgLogLevel = this.getStr(_msgLogLevel);

    var logLevel = "warn";

    if (aActivityObject.flags in this.scriptErrorFlags) {
      logLevel = this.scriptErrorFlags[aActivityObject.flags];
    }

    // in this case, the "activity object" is the
    // nsIScriptError or nsIConsoleMessage
    var message = {
      activity: aActivityObject,
      origin: "console-listener",
      hudId: aHUDId,
    };

    var lineColSubs = [aActivityObject.lineNumber,
                       aActivityObject.columnNumber];
    var lineCol = this.getFormatStr("errLineCol", lineColSubs);

    var errFileSubs = [aActivityObject.sourceName];
    var errFile = this.getFormatStr("errFile", errFileSubs);

    var msgCategory = this.getStr("msgCategory");

    message.logLevel = logLevel;
    message.level = logLevel;

    message.message = msgLogLevel + " " +
                      aActivityObject.errorMessage + " " +
                      errFile + " " +
                      lineCol + " " +
                      msgCategory + " " + aActivityObject.category;

    let outputNode = this.hudReferences[aHUDId].outputNode;

    var messageObject =
    this.messageFactory(message, message.level, outputNode, aActivityObject);

    this.logMessage(messageObject.messageObject, outputNode, messageObject.messageNode);
  },

  /**
   * Calls logNetActivity() or logConsoleActivity() as appropriate to log the
   * given message to the appropriate console.
   *
   * @param string aType
   *        The type of message; one of "network" or "console-listener".
   * @param string aHUDId
   *        The ID of the console to which to send the message.
   * @param object (or nsIScriptError) aActivityObj
   *        The message to send.
   * @returns void
   */
  logActivity: function HS_logActivity(aType, aHUDId, aActivityObject)
  {
    if (aType == "network") {
      return this.logNetActivity(aType, aActivityObject);
    }
    else if (aType == "console-listener") {
      this.logConsoleActivity(aHUDId, aActivityObject);
    }
  },

  /**
   * Builds and appends a group to the console if enough time has passed since
   * the last message.
   *
   * @param nsIDOMNode aConsoleNode
   *        The DOM node that holds the output of the console (NB: not the HUD
   *        node itself).
   * @param number aTimestamp
   *        The timestamp of the newest message in milliseconds.
   * @returns nsIDOMNode
   *          The group into which the next message should be written.
   */
  appendGroupIfNecessary:
  function HS_appendGroupIfNecessary(aConsoleNode, aTimestamp)
  {
    let hudBox = aConsoleNode;
    while (hudBox && !hudBox.classList.contains("hud-box")) {
      hudBox = hudBox.parentNode;
    }

    let lastTimestamp = hudBox.lastTimestamp;
    let delta = aTimestamp - lastTimestamp;
    hudBox.lastTimestamp = aTimestamp;
    if (delta < NEW_GROUP_DELAY) {
      // No new group needed. Return the most recently-added group, if there is
      // one.
      let lastGroupNode = aConsoleNode.querySelector(".hud-group:last-child");
      if (lastGroupNode != null) {
        return lastGroupNode;
      }
    }

    let chromeDocument = aConsoleNode.ownerDocument;
    let groupNode = chromeDocument.createElement("vbox");
    groupNode.setAttribute("class", "hud-group");

    aConsoleNode.appendChild(groupNode);
    return groupNode;
  },

  /**
   * Wrapper method that generates a LogMessage object
   *
   * @param object aMessage
   * @param string aLevel
   * @param nsIDOMNode aOutputNode
   * @param object aActivityObject
   * @returns
   */
  messageFactory:
  function messageFactory(aMessage, aLevel, aOutputNode, aActivityObject)
  {
    // generate a LogMessage object
    return new LogMessage(aMessage, aLevel, aOutputNode,  aActivityObject);
  },

  /**
   * Initialize the JSTerm object to create a JS Workspace by attaching the UI
   * into the given parent node, using the mixin.
   *
   * @param nsIDOMWindow aContext the context used for evaluating user input
   * @param nsIDOMNode aParentNode where to attach the JSTerm
   * @param object aConsole
   *        Console object used within the JSTerm instance to report errors
   *        and log data (by calling console.error(), console.log(), etc).
   */
  initializeJSTerm: function HS_initializeJSTerm(aContext, aParentNode, aConsole)
  {
    // create Initial JS Workspace:
    var context = Cu.getWeakReference(aContext);

    // Attach the UI into the target parent node using the mixin.
    var firefoxMixin = new JSTermFirefoxMixin(context, aParentNode);
    var jsTerm = new JSTerm(context, aParentNode, firefoxMixin, aConsole);

    // TODO: injection of additional functionality needs re-thinking/api
    // see bug 559748
  },

  /**
   * Passed a HUDId, the corresponding window is returned
   *
   * @param string aHUDId
   * @returns nsIDOMWindow
   */
  getContentWindowFromHUDId: function HS_getContentWindowFromHUDId(aHUDId)
  {
    var hud = this.getHeadsUpDisplay(aHUDId);
    var nodes = hud.parentNode.childNodes;

    for (var i = 0; i < nodes.length; i++) {
      var node = nodes[i];

      if (node.localName == "stack" &&
          node.firstChild &&
          node.firstChild.contentWindow) {
        return node.firstChild.contentWindow;
      }
    }
    throw new Error("HS_getContentWindowFromHUD: Cannot get contentWindow");
  },

  /**
   * Creates a generator that always returns a unique number for use in the
   * indexes
   *
   * @returns Generator
   */
  createSequencer: function HS_createSequencer(aInt)
  {
    function sequencer(aInt)
    {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(aInt);
  },

  scriptErrorFlags: {
    0: "error",
    1: "warn",
    2: "exception",
    4: "strict"
  },

  /**
   * replacement strings (L10N)
   */
  scriptMsgLogLevel: {
    0: "typeError",
    1: "typeWarning",
    2: "typeException",
    4: "typeStrict",
  },

  /**
   * Closes the Console, if any, that resides on the given tab.
   *
   * @param nsIDOMNode aTab
   *        The tab on which to close the console.
   * @returns void
   */
  closeConsoleOnTab: function HS_closeConsoleOnTab(aTab)
  {
    let xulDocument = aTab.ownerDocument;
    let xulWindow = xulDocument.defaultView;
    let gBrowser = xulWindow.gBrowser;
    let linkedBrowser = aTab.linkedBrowser;
    let notificationBox = gBrowser.getNotificationBox(linkedBrowser);
    let hudId = "hud_" + notificationBox.getAttribute("id");
    let outputNode = xulDocument.getElementById(hudId);
    if (outputNode != null) {
      this.unregisterDisplay(outputNode);
    }
  },

  /**
   * onTabClose event handler function
   *
   * @param aEvent
   * @returns void
   */
  onTabClose: function HS_onTabClose(aEvent)
  {
    this.closeConsoleOnTab(aEvent.target);
  },

  /**
   * Called whenever a browser window closes. Cleans up any consoles still
   * around.
   *
   * @param nsIDOMEvent aEvent
   *        The dispatched event.
   * @returns void
   */
  onWindowUnload: function HS_onWindowUnload(aEvent)
  {
    let gBrowser = aEvent.target.defaultView.gBrowser;
    let tabContainer = gBrowser.tabContainer;

    let tab = tabContainer.firstChild;
    while (tab != null) {
      this.closeConsoleOnTab(tab);
      tab = tab.nextSibling;
    }
  },

  /**
   * windowInitializer - checks what Gecko app is running and inits the HUD
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  windowInitializer: function HS_WindowInitalizer(aContentWindow)
  {
    var xulWindow = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell)
                      .chromeEventHandler.ownerDocument.defaultView;

    let xulWindow = unwrap(xulWindow);

    let docElem = xulWindow.document.documentElement;
    if (!docElem || docElem.getAttribute("windowtype") != "navigator:browser" ||
        !xulWindow.gBrowser) {
      // Do not do anything unless we have a browser window.
      // This may be a view-source window or other type of non-browser window.
      return;
    }

    xulWindow.addEventListener("unload", this.onWindowUnload, false);

    let gBrowser = xulWindow.gBrowser;

    let container = gBrowser.tabContainer;
    container.addEventListener("TabClose", this.onTabClose, false);

    let _browser = gBrowser.
      getBrowserForDocument(aContentWindow.top.document);
    let nBox = gBrowser.getNotificationBox(_browser);
    let nBoxId = nBox.getAttribute("id");
    let hudId = "hud_" + nBoxId;

    if (!this.canActivateContext(hudId)) {
      return;
    }

    this.registerDisplay(hudId, aContentWindow);

    let hudNode;
    let childNodes = nBox.childNodes;

    for (let i = 0; i < childNodes.length; i++) {
      let id = childNodes[i].getAttribute("id");
      // `id` is a string with the format "hud_<number>".
      if (id.split("_")[0] == "hud") {
        hudNode = childNodes[i];
        break;
      }
    }

    let hud;
    // If there is no HUD for this tab create a new one.
    if (!hudNode) {
      // get nBox object and call new HUD
      let config = { parentNode: nBox,
                     contentWindow: aContentWindow
                   };

      hud = new HeadsUpDisplay(config);

      HUDService.registerHUDReference(hud);
    }
    else {
      hud = this.hudReferences[hudId];
      if (aContentWindow == aContentWindow.top) {
        // TODO: name change?? doesn't actually re-attach the console
        hud.reattachConsole(aContentWindow);
      }
    }

    // need to detect that the console component has been paved over
    // TODO: change how we detect our console: bug 612405
    let consoleObject = aContentWindow.wrappedJSObject.console;
    if (consoleObject && consoleObject.classID != CONSOLEAPI_CLASS_ID) {
      this.logWarningAboutReplacedAPI(hudId);
    }

    // register the controller to handle "select all" properly
    this.createController(xulWindow);
  },

  /**
   * Adds the command controller to the XUL window if it's not already present.
   *
   * @param nsIDOMWindow aWindow
   *        The browser XUL window.
   * @returns void
   */
  createController: function HUD_createController(aWindow)
  {
    if (aWindow.commandController == null) {
      aWindow.commandController = new CommandController(aWindow);
      aWindow.controllers.insertControllerAt(0, aWindow.commandController);
    }
  },

  /**
   * Animates the Console appropriately.
   *
   * @param string aHUDId The ID of the console.
   * @param string aDirection Whether to animate the console appearing
   *        (ANIMATE_IN) or disappearing (ANIMATE_OUT).
   * @param function aCallback An optional callback, which will be called with
   *        the "transitionend" event passed as a parameter once the animation
   *        finishes.
   */
  animate: function HS_animate(aHUDId, aDirection, aCallback)
  {
    let hudBox = this.getOutputNodeById(aHUDId);
    if (!hudBox.classList.contains("animated") && aCallback) {
      aCallback();
      return;
    }

    switch (aDirection) {
      case ANIMATE_OUT:
        hudBox.style.height = 0;
        break;
      case ANIMATE_IN:
        var contentWindow = hudBox.ownerDocument.defaultView;
        hudBox.style.height = Math.ceil(contentWindow.innerHeight / 3) + "px";
        break;
    }

    if (aCallback) {
      hudBox.addEventListener("transitionend", aCallback, false);
    }
  },

  /**
   * Disables all animation for a console, for unit testing. After this call,
   * the console will instantly take on a reasonable height, and the close
   * animation will not occur.
   *
   * @param string aHUDId The ID of the console.
   */
  disableAnimation: function HS_disableAnimation(aHUDId)
  {
    let hudBox = this.getOutputNodeById(aHUDId);
    hudBox.classList.remove("animated");
    hudBox.style.height = "300px";
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplay
//////////////////////////////////////////////////////////////////////////

/*
 * HeadsUpDisplay is an interactive console initialized *per tab*  that
 * displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 * */
function HeadsUpDisplay(aConfig)
{
  // sample config: { parentNode: aDOMNode,
  //                  // or
  //                  parentNodeId: "myHUDParent123",
  //
  //                  placement: "appendChild"
  //                  // or
  //                  placement: "insertBefore",
  //                  placementChildNodeIndex: 0,
  //                }

  this.HUDBox = null;

  if (aConfig.parentNode) {
    // TODO: need to replace these DOM calls with internal functions
    // that operate on each application's node structure
    // better yet, we keep these functions in a "bridgeModule" or the HUDService
    // to keep a registry of nodeGetters for each application
    // see bug 568647
    this.parentNode = aConfig.parentNode;
    this.notificationBox = aConfig.parentNode;
    this.chromeDocument = aConfig.parentNode.ownerDocument;
    this.contentWindow = aConfig.contentWindow;
    this.uriSpec = aConfig.contentWindow.location.href;
    this.hudId = "hud_" + aConfig.parentNode.getAttribute("id");
  }
  else {
    // parentNodeId is the node's id where we attach the HUD
    // TODO: is the "navigator:browser" below used in all Gecko Apps?
    // see bug 568647
    let windowEnum = Services.wm.getEnumerator("navigator:browser");
    let parentNode;
    let contentDocument;
    let contentWindow;
    let chromeDocument;

    // TODO: the following  part is still very Firefox specific
    // see bug 568647

    while (windowEnum.hasMoreElements()) {
      let window = windowEnum.getNext();
      try {
        let gBrowser = window.gBrowser;
        let _browsers = gBrowser.browsers;
        let browserLen = _browsers.length;

        for (var i = 0; i < browserLen; i++) {
          var _notificationBox = gBrowser.getNotificationBox(_browsers[i]);
          this.notificationBox = _notificationBox;

          if (_notificationBox.getAttribute("id") == aConfig.parentNodeId) {
            this.parentNodeId = _notificationBox.getAttribute("id");
            this.hudId = "hud_" + this.parentNodeId;

            parentNode = _notificationBox;

            this.contentDocument =
              _notificationBox.childNodes[0].contentDocument;
            this.contentWindow =
              _notificationBox.childNodes[0].contentWindow;
            this.uriSpec = aConfig.contentWindow.location.href;

            this.chromeDocument =
              _notificationBox.ownerDocument;

            break;
          }
        }
      }
      catch (ex) {
        Cu.reportError(ex);
      }

      if (parentNode) {
        break;
      }
    }
    if (!parentNode) {
      throw new Error(this.ERRORS.PARENTNODE_NOT_FOUND);
    }
    this.parentNode = parentNode;
  }

  // create textNode Factory:
  this.textFactory = NodeFactory("text", "xul", this.chromeDocument);

  this.chromeWindow = HUDService.getChromeWindowFromContentWindow(this.contentWindow);

  // create a panel dynamically and attach to the parentNode
  let hudBox = this.createHUD();

  let splitter = this.chromeDocument.createElement("splitter");
  splitter.setAttribute("class", "hud-splitter");

  this.notificationBox.insertBefore(splitter,
                                    this.notificationBox.childNodes[1]);

  this.HUDBox.lastTimestamp = 0;
  // create the JSTerm input element
  try {
    this.createConsoleInput(this.contentWindow, this.consoleWrap, this.outputNode);
    this.HUDBox.querySelectorAll(".jsterm-input-node")[0].focus();
  }
  catch (ex) {
    Cu.reportError(ex);
  }
}

HeadsUpDisplay.prototype = {

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HUD_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @param array aArray
   * @returns string
   */
  getFormatStr: function HUD_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * The JSTerm object that contains the console's inputNode
   *
   */
  jsterm: null,

  /**
   * creates and attaches the console input node
   *
   * @param nsIDOMWindow aWindow
   * @returns void
   */
  createConsoleInput:
  function HUD_createConsoleInput(aWindow, aParentNode, aExistingConsole)
  {
    var context = Cu.getWeakReference(aWindow);

    if (appName() == "FIREFOX") {
      let outputCSSClassOverride = "hud-msg-node";
      let mixin = new JSTermFirefoxMixin(context, aParentNode, aExistingConsole, outputCSSClassOverride);
      this.jsterm = new JSTerm(context, aParentNode, mixin, this.console);
    }
    else {
      throw new Error("Unsupported Gecko Application");
    }
  },

  /**
   * Re-attaches a console when the contentWindow is recreated
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  reattachConsole: function HUD_reattachConsole(aContentWindow)
  {
    this.contentWindow = aContentWindow;
    this.contentDocument = this.contentWindow.document;
    this.uriSpec = this.contentWindow.location.href;

    if (!this.jsterm) {
      this.createConsoleInput(this.contentWindow, this.consoleWrap, this.outputNode);
    }
    else {
      this.jsterm.context = Cu.getWeakReference(this.contentWindow);
      this.jsterm.console = this.console;
      this.jsterm.createSandbox();
    }
  },

  /**
   * Shortcut to make XUL nodes
   *
   * @param string aTag
   * @returns nsIDOMNode
   */
  makeXULNode:
  function HUD_makeXULNode(aTag)
  {
    return this.chromeDocument.createElement(aTag);
  },

  /**
   * Build the UI of each HeadsUpDisplay
   *
   * @returns nsIDOMNode
   */
  makeHUDNodes: function HUD_makeHUDNodes()
  {
    let self = this;
    this.HUDBox = this.makeXULNode("vbox");
    this.HUDBox.setAttribute("id", this.hudId);
    this.HUDBox.setAttribute("class", "hud-box animated");
    this.HUDBox.style.height = 0;

    let outerWrap = this.makeXULNode("vbox");
    outerWrap.setAttribute("class", "hud-outer-wrapper");
    outerWrap.setAttribute("flex", "1");

    let consoleCommandSet = this.makeXULNode("commandset");
    outerWrap.appendChild(consoleCommandSet);

    let consoleWrap = this.makeXULNode("vbox");
    this.consoleWrap = consoleWrap;
    consoleWrap.setAttribute("class", "hud-console-wrapper");
    consoleWrap.setAttribute("flex", "1");

    this.outputNode = this.makeXULNode("scrollbox");
    this.outputNode.setAttribute("class", "hud-output-node");
    this.outputNode.setAttribute("flex", "1");
    this.outputNode.setAttribute("orient", "vertical");
    this.outputNode.setAttribute("context", this.hudId + "-output-contextmenu");

    this.outputNode.addEventListener("DOMNodeInserted", function(ev) {
      // DOMNodeInserted is also called when the output node is being *itself*
      // (re)inserted into the DOM (which happens during a search, for
      // example). For this reason, we need to ensure that we only check
      // message nodes.
      let node = ev.target;
      if (node.nodeType === node.ELEMENT_NODE &&
          node.classList.contains("hud-msg-node")) {
        HUDService.adjustVisibilityForNewlyInsertedNode(self.hudId, ev.target);
      }
    }, false);

    this.filterSpacer = this.makeXULNode("spacer");
    this.filterSpacer.setAttribute("flex", "1");

    this.filterBox = this.makeXULNode("textbox");
    this.filterBox.setAttribute("class", "compact hud-filter-box");
    this.filterBox.setAttribute("hudId", this.hudId);
    this.filterBox.setAttribute("placeholder", this.getStr("stringFilter"));
    this.filterBox.setAttribute("type", "search");

    this.setFilterTextBoxEvents();

    this.createConsoleMenu(this.consoleWrap);

    this.filterPrefs = HUDService.getDefaultFilterPrefs(this.hudId);

    let consoleFilterToolbar = this.makeFilterToolbar();
    consoleFilterToolbar.setAttribute("id", "viewGroup");
    this.consoleFilterToolbar = consoleFilterToolbar;
    consoleWrap.appendChild(consoleFilterToolbar);

    consoleWrap.appendChild(this.outputNode);

    outerWrap.appendChild(consoleWrap);

    this.HUDBox.lastTimestamp = 0;

    this.jsTermParentNode = outerWrap;
    this.HUDBox.appendChild(outerWrap);
    return this.HUDBox;
  },


  /**
   * sets the click events for all binary toggle filter buttons
   *
   * @returns void
   */
  setFilterTextBoxEvents: function HUD_setFilterTextBoxEvents()
  {
    var filterBox = this.filterBox;
    function onChange()
    {
      // To improve responsiveness, we let the user finish typing before we
      // perform the search.

      if (this.timer == null) {
        let timerClass = Cc["@mozilla.org/timer;1"];
        this.timer = timerClass.createInstance(Ci.nsITimer);
      } else {
        this.timer.cancel();
      }

      let timerEvent = {
        notify: function setFilterTextBoxEvents_timerEvent_notify() {
          HUDService.updateFilterText(filterBox);
        }
      };

      this.timer.initWithCallback(timerEvent, SEARCH_DELAY,
        Ci.nsITimer.TYPE_ONE_SHOT);
    }

    filterBox.addEventListener("command", onChange, false);
    filterBox.addEventListener("input", onChange, false);
  },

  /**
   * Make the filter toolbar where we can toggle logging filters
   *
   * @returns nsIDOMNode
   */
  makeFilterToolbar: function HUD_makeFilterToolbar()
  {
    let buttons = ["Network", "CSSParser", "Exception", "Error",
                   "Info", "Warn", "Log",];

    const pageButtons = [
      { prefKey: "network", name: "PageNet" },
      { prefKey: "cssparser", name: "PageCSS" },
      { prefKey: "exception", name: "PageJS" }
    ];
    const consoleButtons = [
      { prefKey: "error", name: "ConsoleErrors" },
      { prefKey: "warn", name: "ConsoleWarnings" },
      { prefKey: "info", name: "ConsoleInfo" },
      { prefKey: "log", name: "ConsoleLog" }
    ];

    let toolbar = this.makeXULNode("toolbar");
    toolbar.setAttribute("class", "hud-console-filter-toolbar");
    toolbar.setAttribute("mode", "text");

    let pageCategoryTitle = this.getStr("categoryPage");
    this.addButtonCategory(toolbar, pageCategoryTitle, pageButtons);

    let separator = this.makeXULNode("separator");
    separator.setAttribute("orient", "vertical");
    toolbar.appendChild(separator);

    let consoleCategoryTitle = this.getStr("categoryConsole");
    this.addButtonCategory(toolbar, consoleCategoryTitle, consoleButtons);

    toolbar.appendChild(this.filterSpacer);
    toolbar.appendChild(this.filterBox);
    return toolbar;
  },

  /**
   * Creates the context menu on the console, which contains the "clear
   * console" functionality.
   *
   * @param nsIDOMNode aOutputNode
   *        The console output DOM node.
   * @returns void
   */
  createConsoleMenu: function HUD_createConsoleMenu(aConsoleWrapper) {
    let menuPopup = this.makeXULNode("menupopup");
    let id = this.hudId + "-output-contextmenu";
    menuPopup.setAttribute("id", id);

    let saveBodiesItem = this.makeXULNode("menuitem");
    saveBodiesItem.setAttribute("label", this.getStr("saveBodies.label"));
    saveBodiesItem.setAttribute("accesskey",
                                 this.getStr("saveBodies.accesskey"));
    saveBodiesItem.setAttribute("type", "checkbox");
    saveBodiesItem.setAttribute("buttonType", "saveBodies");
    saveBodiesItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(saveBodiesItem);

    menuPopup.appendChild(this.makeXULNode("menuseparator"));

    let copyItem = this.makeXULNode("menuitem");
    copyItem.setAttribute("label", this.getStr("copyCmd.label"));
    copyItem.setAttribute("accesskey", this.getStr("copyCmd.accesskey"));
    copyItem.setAttribute("key", "key_copy");
    copyItem.setAttribute("command", "cmd_copy");
    menuPopup.appendChild(copyItem);

    let selectAllItem = this.makeXULNode("menuitem");
    selectAllItem.setAttribute("label", this.getStr("selectAllCmd.label"));
    selectAllItem.setAttribute("accesskey",
                               this.getStr("selectAllCmd.accesskey"));
    selectAllItem.setAttribute("hudId", this.hudId);
    selectAllItem.setAttribute("buttonType", "selectAll");
    selectAllItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(selectAllItem);

    menuPopup.appendChild(this.makeXULNode("menuseparator"));

    let clearItem = this.makeXULNode("menuitem");
    clearItem.setAttribute("label", this.getStr("clearConsoleCmd.label"));
    clearItem.setAttribute("accesskey",
                           this.getStr("clearConsoleCmd.accesskey"));
    clearItem.setAttribute("hudId", this.hudId);
    clearItem.setAttribute("buttonType", "clear");
    clearItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(clearItem);

    aConsoleWrapper.appendChild(menuPopup);
    aConsoleWrapper.setAttribute("context", id);
  },

  makeButton: function HUD_makeButton(aName, aPrefKey, aType)
  {
    var self = this;
    let prefKey = aPrefKey;

    let btn;
    if (aType == "checkbox") {
      btn = this.makeXULNode("checkbox");
      btn.setAttribute("type", aType);
    } else {
      btn = this.makeXULNode("toolbarbutton");
    }

    btn.setAttribute("hudId", this.hudId);
    btn.setAttribute("buttonType", prefKey);
    btn.setAttribute("class", "hud-filter-btn");
    let key = "btn" + aName;
    btn.setAttribute("label", this.getStr(key));
    key = "tip" + aName;
    btn.setAttribute("tooltip", this.getStr(key));

    if (aType == "checkbox") {
      btn.setAttribute("checked", this.filterPrefs[prefKey]);
      function toggle(btn) {
        self.consoleFilterCommands.toggle(btn);
      };

      btn.setAttribute("oncommand", "HUDConsoleUI.toggleFilter(this);");
    }
    else {
      var command = "HUDConsoleUI.command(this)";
      btn.setAttribute("oncommand", command);
    }
    return btn;
  },

  /**
   * Appends a category title and a series of buttons to the filter bar.
   *
   * @param nsIDOMNode aToolbar
   *        The DOM node to which to add the category.
   * @param string aTitle
   *        The title for the category.
   * @param Array aButtons
   *        The buttons, specified as objects with "name" and "prefKey"
   *        properties.
   * @returns nsIDOMNode
   */
  addButtonCategory: function(aToolbar, aTitle, aButtons) {
    let lbl = this.makeXULNode("label");
    lbl.setAttribute("class", "hud-filter-cat");
    lbl.setAttribute("value", aTitle);
    aToolbar.appendChild(lbl);

    for (let i = 0; i < aButtons.length; i++) {
      let btn = aButtons[i];
      aToolbar.appendChild(this.makeButton(btn.name, btn.prefKey, "checkbox"));
    }
  },

  createHUD: function HUD_createHUD()
  {
    let self = this;
    if (this.HUDBox) {
      return this.HUDBox;
    }
    else  {
      this.makeHUDNodes();

      let nodes = this.notificationBox.insertBefore(this.HUDBox,
        this.notificationBox.childNodes[0]);

      return this.HUDBox;
    }
  },

  get console() { return this.contentWindow.wrappedJSObject.console; },

  getLogCount: function HUD_getLogCount()
  {
    return this.outputNode.childNodes.length;
  },

  getLogNodes: function HUD_getLogNodes()
  {
    return this.outputNode.childNodes;
  },

  ERRORS: {
    HUD_BOX_DOES_NOT_EXIST: "Heads Up Display does not exist",
    TAB_ID_REQUIRED: "Tab DOM ID is required",
    PARENTNODE_NOT_FOUND: "parentNode element not found"
  }
};


//////////////////////////////////////////////////////////////////////////////
// ConsoleAPIObserver
//////////////////////////////////////////////////////////////////////////////

let ConsoleAPIObserver = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function CAO_init()
  {
    Services.obs.addObserver(this, "quit-application-granted", false);
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  observe: function CAO_observe(aMessage, aTopic, aData)
  {
    if (aTopic == "console-api-log-event") {
      aMessage = aMessage.wrappedJSObject;
      let windowId = parseInt(aData);
      let win = HUDService.getWindowByWindowId(windowId);
      if (!win)
        return;

      // Find the HUD ID for the topmost window
      let hudId = HUDService.getHudIdByWindow(win.top);
      if (!hudId)
        return;

      HUDService.logConsoleAPIMessage(hudId, aMessage.level, aMessage.arguments);
    }
    else if (aTopic == "quit-application-granted") {
      this.shutdown();
    }
  },

  shutdown: function CAO_shutdown()
  {
    Services.obs.removeObserver(this, "console-api-log-event");
  }
};

/**
 * Creates a DOM Node factory for XUL nodes - as well as textNodes
 * @param aFactoryType "xul" or "text"
 * @param ignored This parameter is currently ignored, and will be removed
 * See bug 594304
 * @param aDocument The document, the factory is to generate nodes from
 * @return DOM Node Factory function
 */
function NodeFactory(aFactoryType, ignored, aDocument)
{
  // aDocument is presumed to be a XULDocument
  if (aFactoryType == "text") {
    return function factory(aText)
    {
      return aDocument.createTextNode(aText);
    }
  }
  else if (aFactoryType == "xul") {
    return function factory(aTag)
    {
      return aDocument.createElement(aTag);
    }
  }
  else {
    throw new Error('NodeFactory: Unknown factory type: ' + aFactoryType);
  }
}

//////////////////////////////////////////////////////////////////////////
// JS Completer
//////////////////////////////////////////////////////////////////////////

const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = '{[('.split('');
const CLOSE_BODY = '}])'.split('');
const OPEN_CLOSE_BODY = {
  '{': '}',
  '[': ']',
  '(': ')'
};

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string aStr
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == '\'') {
          state = STATE_QUOTE;
        }
        else if (c == ';') {
          start = i + 1;
        }
        else if (c == ' ') {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == '}') {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quoate state > ' <
      case STATE_QUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
          return;
        }
        else if (c == '\'') {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * scope and inputValue.
 *
 * @param object aScope
 *        Scope to use for the completion.
 *
 * @param string aInputValue
 *        Value that should be completed.
 *
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue)
{
  let obj = unwrap(aScope);

  // Analyse the aInputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(aInputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = aInputValue.substring(beginning.startPos);

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  let properties = completionPart.split('.');
  let matchProp;
  if (properties.length > 1) {
      matchProp = properties[properties.length - 1].trimLeft();
      properties.pop();
      for each (var prop in properties) {
        prop = prop.trim();

        // If obj is undefined or null, then there is no change to run
        // completion on it. Exit here.
        if (typeof obj === "undefined" || obj === null) {
          return null;
        }

        // Check if prop is a getter function on obj. Functions can change other
        // stuff so we can't execute them to get the next object. Stop here.
        if (obj.__lookupGetter__(prop)) {
          return null;
        }
        obj = obj[prop];
      }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If obj is undefined or null, then there is no change to run
  // completion on it. Exit here.
  if (typeof obj === "undefined" || obj === null) {
    return null;
  }

  let matches = [];
  for (var prop in obj) {
    matches.push(prop);
  }

  matches = matches.filter(function(item) {
    return item.indexOf(matchProp) == 0;
  }).sort();

  return {
    matchProp: matchProp,
    matches: matches
  };
}

//////////////////////////////////////////////////////////////////////////
// JSTerm
//////////////////////////////////////////////////////////////////////////

/**
 * JSTermHelper
 *
 * Defines a set of functions ("helper functions") that are available from the
 * WebConsole but not from the webpage.
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 */
function JSTermHelper(aJSTerm)
{
  /**
   * Returns the result of document.getElementById(aId).
   *
   * @param string aId
   *        A string that is passed to window.document.getElementById.
   * @returns nsIDOMNode or null
   */
  aJSTerm.sandbox.$ = function JSTH_$(aId)
  {
    try {
      return aJSTerm._window.document.getElementById(aId);
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Returns the result of document.querySelectorAll(aSelector).
   *
   * @param string aSelector
   *        A string that is passed to window.document.querySelectorAll.
   * @returns array of nsIDOMNode
   */
  aJSTerm.sandbox.$$ = function JSTH_$$(aSelector)
  {
    try {
      return aJSTerm._window.document.querySelectorAll(aSelector);
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Runs a xPath query and returns all matched nodes.
   *
   * @param string aXPath
   *        xPath search query to execute.
   * @param [optional] nsIDOMNode aContext
   *        Context to run the xPath query on. Uses window.document if not set.
   * @returns array of nsIDOMNode
   */
  aJSTerm.sandbox.$x = function JSTH_$x(aXPath, aContext)
  {
    let nodes = [];
    let doc = aJSTerm._window.document;
    let aContext = aContext || doc;

    try {
      let results = doc.evaluate(aXPath, aContext, null,
                                  Ci.nsIDOMXPathResult.ANY_TYPE, null);

      let node;
      while (node = results.iterateNext()) {
        nodes.push(node);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }

    return nodes;
  };

  /**
   * Clears the output of the JSTerm.
   */
  aJSTerm.sandbox.clear = function JSTH_clear()
  {
    aJSTerm.clearOutput();
  };

  /**
   * Returns the result of Object.keys(aObject).
   *
   * @param object aObject
   *        Object to return the property names from.
   * @returns array of string
   */
  aJSTerm.sandbox.keys = function JSTH_keys(aObject)
  {
    try {
      return Object.keys(unwrap(aObject));
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Returns the values of all properties on aObject.
   *
   * @param object aObject
   *        Object to display the values from.
   * @returns array of string
   */
  aJSTerm.sandbox.values = function JSTH_values(aObject)
  {
    let arrValues = [];
    let obj = unwrap(aObject);

    try {
      for (let prop in obj) {
        arrValues.push(obj[prop]);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
    return arrValues;
  };

  /**
   * Inspects the passed aObject. This is done by opening the PropertyPanel.
   *
   * @param object aObject
   *        Object to inspect.
   * @returns void
   */
  aJSTerm.sandbox.inspect = function JSTH_inspect(aObject)
  {
    aJSTerm.openPropertyPanel(null, unwrap(aObject));
  };

  /**
   * Prints aObject to the output.
   *
   * @param object aObject
   *        Object to print to the output.
   * @returns void
   */
  aJSTerm.sandbox.pprint = function JSTH_pprint(aObject)
  {
    if (aObject === null || aObject === undefined || aObject === true || aObject === false) {
      aJSTerm.console.error(HUDService.getStr("helperFuncUnsupportedTypeError"));
      return;
    }
    let output = [];
    let pairs = namesAndValuesOf(unwrap(aObject));

    pairs.forEach(function(pair) {
      output.push("  " + pair.display);
    });

    aJSTerm.writeOutput(output.join("\n"));
  };
}

/**
 * JSTerm
 *
 * JavaScript Terminal: creates input nodes for console code interpretation
 * and 'JS Workspaces'
 */

/**
 * Create a JSTerminal or attach a JSTerm input node to an existing output node,
 * given by the parent node.
 *
 * @param object aContext
 *        Usually nsIDOMWindow, but doesn't have to be
 * @param nsIDOMNode aParentNode where to attach the JSTerm
 * @param object aMixin
 *        Gecko-app (or Jetpack) specific utility object
 * @param object aConsole
 *        Console object to use within the JSTerm.
 */
function JSTerm(aContext, aParentNode, aMixin, aConsole)
{
  // set the context, attach the UI by appending to aParentNode

  this.application = appName();
  this.context = aContext;
  this.parentNode = aParentNode;
  this.mixins = aMixin;
  this.console = aConsole;

  this.xulElementFactory =
    NodeFactory("xul", "xul", aParentNode.ownerDocument);

  this.textFactory = NodeFactory("text", "xul", aParentNode.ownerDocument);

  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  this.historyIndex = 0;
  this.historyPlaceHolder = 0;  // this.history.length;
  this.log = LogFactory("*** JSTerm:");
  this.init();
}

JSTerm.prototype = {

  propertyProvider: JSPropertyProvider,

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  init: function JST_init()
  {
    this.createSandbox();
    this.inputNode = this.mixins.inputNode;
    let eventHandlerKeyDown = this.keyDown();
    this.inputNode.addEventListener('keypress', eventHandlerKeyDown, false);
    let eventHandlerInput = this.inputEventHandler();
    this.inputNode.addEventListener('input', eventHandlerInput, false);
    this.outputNode = this.mixins.outputNode;
    if (this.mixins.cssClassOverride) {
      this.cssClassOverride = this.mixins.cssClassOverride;
    }
  },

  get codeInputString()
  {
    // TODO: filter the input for windows line breaks, conver to unix
    // see bug 572812
    return this.inputNode.value;
  },

  generateUI: function JST_generateUI()
  {
    this.mixins.generateUI();
  },

  attachUI: function JST_attachUI()
  {
    this.mixins.attachUI();
  },

  createSandbox: function JST_setupSandbox()
  {
    // create a JS Sandbox out of this.context
    this.sandbox = new Cu.Sandbox(this._window,
      { sandboxPrototype: this._window, wantXrays: false });
    this.sandbox.console = this.console;
    JSTermHelper(this);
  },

  get _window()
  {
    return this.context.get().QueryInterface(Ci.nsIDOMWindowInternal);
  },

  /**
   * Evaluates a string in the sandbox. The string is currently wrapped by a
   * with(window) { aString } construct, see bug 574033.
   *
   * @param string aString
   *        String to evaluate in the sandbox.
   * @returns something
   *          The result of the evaluation.
   */
  evalInSandbox: function JST_evalInSandbox(aString)
  {
    return Cu.evalInSandbox(aString, this.sandbox, "1.8", "Web Console", 1);
  },


  execute: function JST_execute(aExecuteString)
  {
    // attempt to execute the content of the inputNode
    aExecuteString = aExecuteString || this.inputNode.value;
    if (!aExecuteString) {
      this.writeOutput("no value to execute");
      return;
    }

    this.writeOutput(aExecuteString, true);

    try {
      var result = this.evalInSandbox(aExecuteString);

      if (result || result === false) {
        this.writeOutputJS(aExecuteString, result);
      }
      else if (result === undefined) {
        this.writeOutput("undefined", false);
      }
      else if (result === null) {
        this.writeOutput("null", false);
      }
    }
    catch (ex) {
      this.writeOutput(ex);
    }

    this.history.push(aExecuteString);
    this.historyIndex++;
    this.historyPlaceHolder = this.history.length;
    this.setInputValue("");
  },

  /**
   * Opens a new PropertyPanel. The panel has two buttons: "Update" reexecutes
   * the passed aEvalString and places the result inside of the tree. The other
   * button closes the panel.
   *
   * @param string aEvalString
   *        String that was used to eval the aOutputObject. Used as title
   *        and to update the tree content.
   * @param object aOutputObject
   *        Object to display/inspect inside of the tree.
   * @param nsIDOMNode aAnchor
   *        A node to popup the panel next to (using "after_pointer").
   * @returns object the created and opened propertyPanel.
   */
  openPropertyPanel: function JST_openPropertyPanel(aEvalString, aOutputObject,
                                                    aAnchor)
  {
    let self = this;
    let propPanel;
    // The property panel has two buttons:
    // 1. `Update`: reexecutes the string executed on the command line. The
    //    result will be inspected by this panel.
    // 2. `Close`: destroys the panel.
    let buttons = [];

    // If there is a evalString passed to this function, then add a `Update`
    // button to the panel so that the evalString can be reexecuted to update
    // the content of the panel.
    if (aEvalString !== null) {
      buttons.push({
        label: HUDService.getStr("update.button"),
        accesskey: HUDService.getStr("update.accesskey"),
        oncommand: function () {
          try {
            var result = self.evalInSandbox(aEvalString);

            if (result !== undefined) {
              // TODO: This updates the value of the tree.
              // However, the states of opened nodes is not saved.
              // See bug 586246.
              propPanel.treeView.data = result;
            }
          }
          catch (ex) {
            self.console.error(ex);
          }
        }
      });
    }

    buttons.push({
      label: HUDService.getStr("close.button"),
      accesskey: HUDService.getStr("close.accesskey"),
      class: "jsPropertyPanelCloseButton",
      oncommand: function () {
        propPanel.destroy();
        aAnchor._panelOpen = false;
      }
    });

    let doc = self.parentNode.ownerDocument;
    let parent = doc.getElementById("mainPopupSet");
    let title = (aEvalString
        ? HUDService.getFormatStr("jsPropertyInspectTitle", [aEvalString])
        : HUDService.getStr("jsPropertyTitle"));
    propPanel = new PropertyPanel(parent, doc, title, aOutputObject, buttons);

    let panel = propPanel.panel;
    panel.openPopup(aAnchor, "after_pointer", 0, 0, false, false);
    panel.sizeTo(200, 400);
    return propPanel;
  },

  /**
   * Writes a JS object to the JSTerm outputNode. If the user clicks on the
   * written object, openPropertyPanel is called to open up a panel to inspect
   * the object.
   *
   * @param string aEvalString
   *        String that was evaluated to get the aOutputObject.
   * @param object aOutputObject
   *        Object to be written to the outputNode.
   */
  writeOutputJS: function JST_writeOutputJS(aEvalString, aOutputObject)
  {
    let lastGroupNode = HUDService.appendGroupIfNecessary(this.outputNode,
                                                      Date.now());

    var self = this;
    var node = this.xulElementFactory("label");
    node.setAttribute("class", "jsterm-output-line hud-clickable");
    node.setAttribute("aria-haspopup", "true");
    node.setAttribute("crop", "end");

    node.addEventListener("mousedown", function(aEvent) {
      this._startX = aEvent.clientX;
      this._startY = aEvent.clientY;
    }, false);

    node.addEventListener("click", function(aEvent) {
      if (aEvent.detail != 1 || aEvent.button != 0 ||
          (this._startX != aEvent.clientX &&
           this._startY != aEvent.clientY)) {
        return;
      }

      if (!this._panelOpen) {
        self.openPropertyPanel(aEvalString, aOutputObject, this);
        this._panelOpen = true;
      }
    }, false);

    // TODO: format the aOutputObject and don't just use the
    // aOuputObject.toString() function: [object object] -> Object {prop, ...}
    // See bug 586249.
    let textNode = this.textFactory(aOutputObject + "\n");
    node.appendChild(textNode);

    lastGroupNode.appendChild(node);
    ConsoleUtils.scrollToVisible(node);
    pruneConsoleOutputIfNecessary(this.outputNode);
  },

  /**
   * Writes a message to the HUD that originates from the interactive
   * JavaScript console.
   *
   * @param string aOutputMessage
   *        The message to display.
   * @param boolean aIsInput
   *        True if the message is the user's input, false if the message is
   *        the result of the expression the user typed.
   * @returns void
   */
  writeOutput: function JST_writeOutput(aOutputMessage, aIsInput)
  {
    let lastGroupNode = HUDService.appendGroupIfNecessary(this.outputNode,
                                                          Date.now());

    var node = this.xulElementFactory("label");
    if (aIsInput) {
      node.setAttribute("class", "jsterm-input-line");
      aOutputMessage = "> " + aOutputMessage;
    }
    else {
      node.setAttribute("class", "jsterm-output-line");
    }

    if (this.cssClassOverride) {
      let classes = this.cssClassOverride.split(" ");
      for (let i = 0; i < classes.length; i++) {
        node.classList.add(classes[i]);
      }
    }

    var textNode = this.textFactory(aOutputMessage + "\n");
    node.appendChild(textNode);

    lastGroupNode.appendChild(node);
    ConsoleUtils.scrollToVisible(node);
    pruneConsoleOutputIfNecessary(this.outputNode);
  },

  clearOutput: function JST_clearOutput()
  {
    let outputNode = this.outputNode;

    while (outputNode.firstChild) {
      outputNode.removeChild(outputNode.firstChild);
    }

    outputNode.lastTimestamp = 0;
  },

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput: function JST_resizeInput()
  {
    let inputNode = this.inputNode;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    let scrollHeight = inputNode.inputField.scrollHeight;
    if (scrollHeight > 0) {
      inputNode.style.height = scrollHeight + "px";
    }
  },

  /**
   * Sets the value of the input field (command line), and resizes the field to
   * fit its contents. This method is preferred over setting "inputNode.value"
   * directly, because it correctly resizes the field.
   *
   * @param string aNewValue
   *        The new value to set.
   * @returns void
   */
  setInputValue: function JST_setInputValue(aNewValue)
  {
    this.inputNode.value = aNewValue;
    this.resizeInput();
  },

  inputEventHandler: function JSTF_inputEventHandler()
  {
    var self = this;
    function handleInputEvent(aEvent) {
      self.resizeInput();
    }
    return handleInputEvent;
  },

  keyDown: function JSTF_keyDown(aEvent)
  {
    var self = this;
    function handleKeyDown(aEvent) {
      // ctrl-a
      var setTimeout = aEvent.target.ownerDocument.defaultView.setTimeout;
      var target = aEvent.target;
      var tmp;

      if (aEvent.ctrlKey) {
        switch (aEvent.charCode) {
          case 97:
            // control-a
            tmp = self.codeInputString;
            setTimeout(function() {
              self.setInputValue(tmp);
              self.inputNode.setSelectionRange(0, 0);
            }, 0);
            break;
          case 101:
            // control-e
            tmp = self.codeInputString;
            self.setInputValue("");
            setTimeout(function(){
              self.setInputValue(tmp);
            }, 0);
            break;
          default:
            return;
        }
        return;
      }
      else if (aEvent.shiftKey && aEvent.keyCode == 13) {
        // shift return
        // TODO: expand the inputNode height by one line
        return;
      }
      else {
        switch(aEvent.keyCode) {
          case 13:
            // return
            self.execute();
            aEvent.preventDefault();
            break;
          case 38:
            // up arrow: history previous
            if (self.caretAtStartOfInput()) {
              let updated = self.historyPeruse(HISTORY_BACK);
              if (updated && aEvent.cancelable) {
                self.inputNode.setSelectionRange(0, 0);
                aEvent.preventDefault();
              }
            }
            break;
          case 40:
            // down arrow: history next
            if (self.caretAtEndOfInput()) {
              let updated = self.historyPeruse(HISTORY_FORWARD);
              if (updated && aEvent.cancelable) {
                let inputEnd = self.inputNode.value.length;
                self.inputNode.setSelectionRange(inputEnd, inputEnd);
                aEvent.preventDefault();
              }
            }
            break;
          case 9:
            // tab key
            // If there are more than one possible completion, pressing tab
            // means taking the next completion, shift_tab means taking
            // the previous completion.
            var completionResult;
            if (aEvent.shiftKey) {
              completionResult = self.complete(self.COMPLETE_BACKWARD);
            }
            else {
              completionResult = self.complete(self.COMPLETE_FORWARD);
            }
            if (completionResult) {
              if (aEvent.cancelable) {
              aEvent.preventDefault();
            }
            aEvent.target.focus();
            }
            break;
          case 8:
            // backspace key
          case 46:
            // delete key
            // necessary so that default is not reached.
            break;
          default:
            // all not handled keys
            // Store the current inputNode value. If the value is the same
            // after keyDown event was handled (after 0ms) then the user
            // moved the cursor. If the value changed, then call the complete
            // function to show completion on new value.
            var value = self.inputNode.value;
            setTimeout(function() {
              if (self.inputNode.value !== value) {
                self.complete(self.COMPLETE_HINT_ONLY);
              }
            }, 0);
            break;
        }
        return;
      }
    }
    return handleKeyDown;
  },

  /**
   * Go up/down the history stack of input values.
   *
   * @param number aDirection
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse: function JST_historyPeruse(aDirection)
  {
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (aDirection == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }

      let inputVal = this.history[--this.historyPlaceHolder];
      if (inputVal){
        this.setInputValue(inputVal);
      }
    }
    // Down Arrow key
    else if (aDirection == HISTORY_FORWARD) {
      if (this.historyPlaceHolder == this.history.length - 1) {
        this.historyPlaceHolder ++;
        this.setInputValue("");
      }
      else if (this.historyPlaceHolder >= (this.history.length)) {
        return false;
      }
      else {
        let inputVal = this.history[++this.historyPlaceHolder];
        if (inputVal){
          this.setInputValue(inputVal);
        }
      }
    }
    else {
      throw new Error("Invalid argument 0");
    }

    return true;
  },

  refocus: function JSTF_refocus()
  {
    this.inputNode.blur();
    this.inputNode.focus();
  },

  /**
   * Check if the caret is at the start of the input.
   *
   * @returns boolean
   *          True if the caret is at the start of the input.
   */
  caretAtStartOfInput: function JST_caretAtStartOfInput()
  {
    return this.inputNode.selectionStart == this.inputNode.selectionEnd &&
        this.inputNode.selectionStart == 0;
  },

  /**
   * Check if the caret is at the end of the input.
   *
   * @returns boolean
   *          True if the caret is at the end of the input, or false otherwise.
   */
  caretAtEndOfInput: function JST_caretAtEndOfInput()
  {
    return this.inputNode.selectionStart == this.inputNode.selectionEnd &&
        this.inputNode.selectionStart == this.inputNode.value.length;
  },

  history: [],

  // Stores the data for the last completion.
  lastCompletion: null,

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the inputNode.value is set to this value and the selection is set
   *          from the current cursor position to the end of the completed text.
   *
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete: function JSTF_complete(type)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      return false;
    }
    let selStart = inputNode.selectionStart, selEnd = inputNode.selectionEnd;

    // 'Normalize' the selection so that end is always after start.
    if (selStart > selEnd) {
      let newSelEnd = selStart;
      selStart = selEnd;
      selEnd = newSelEnd;
    }

    // Only complete if the selection is at the end of the input.
    if (selEnd != inputValue.length) {
      this.lastCompletion = null;
      return false;
    }

    // Remove the selected text from the inputValue.
    inputValue = inputValue.substring(0, selStart);

    let matches;
    let matchIndexToUse;
    let matchOffset;
    let completionStr;

    // If there is a saved completion from last time and the used value for
    // completion stayed the same, then use the stored completion.
    if (this.lastCompletion && inputValue == this.lastCompletion.value) {
      matches = this.lastCompletion.matches;
      matchOffset = this.lastCompletion.matchOffset;
      if (type === this.COMPLETE_BACKWARD) {
        this.lastCompletion.index --;
      }
      else if (type === this.COMPLETE_FORWARD) {
        this.lastCompletion.index ++;
      }
      matchIndexToUse = this.lastCompletion.index;
    }
    else {
      // Look up possible completion values.
      let completion = this.propertyProvider(this.sandbox.window, inputValue);
      if (!completion) {
        return false;
      }
      matches = completion.matches;
      matchIndexToUse = 0;
      matchOffset = completion.matchProp.length;
      // Store this match;
      this.lastCompletion = {
        index: 0,
        value: inputValue,
        matches: matches,
        matchOffset: matchOffset
      };
    }

    if (matches.length != 0) {
      // Ensure that the matchIndexToUse is always a valid array index.
      if (matchIndexToUse < 0) {
        matchIndexToUse = matches.length + (matchIndexToUse % matches.length);
        if (matchIndexToUse == matches.length) {
          matchIndexToUse = 0;
        }
      }
      else {
        matchIndexToUse = matchIndexToUse % matches.length;
      }

      completionStr = matches[matchIndexToUse].substring(matchOffset);
      this.setInputValue(inputValue + completionStr);

      selEnd = inputValue.length + completionStr.length;

      // If there is more than one possible completion or the completed part
      // should get displayed only without moving the cursor at the end of the
      // completion.
      if (matches.length > 1 || type === this.COMPLETE_HINT_ONLY) {
        inputNode.setSelectionRange(selStart, selEnd);
      }
      else {
        inputNode.setSelectionRange(selEnd, selEnd);
      }

      return completionStr ? true : false;
    }

    return false;
  }
};

/**
 * JSTermFirefoxMixin
 *
 * JavaScript Terminal Firefox Mixin
 *
 */
function
JSTermFirefoxMixin(aContext,
                   aParentNode,
                   aExistingConsole,
                   aCSSClassOverride)
{
  // aExisting Console is the existing outputNode to use in favor of
  // creating a new outputNode - this is so we can just attach the inputNode to
  // a normal HeadsUpDisplay console output, and re-use code.
  this.cssClassOverride = aCSSClassOverride;
  this.context = aContext;
  this.parentNode = aParentNode;
  this.existingConsoleNode = aExistingConsole;
  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  if (aParentNode.ownerDocument) {
    this.xulElementFactory =
      NodeFactory("xul", "xul", aParentNode.ownerDocument);

    this.textFactory = NodeFactory("text", "xul", aParentNode.ownerDocument);
    this.generateUI();
    this.attachUI();
  }
  else {
    throw new Error("aParentNode should be a DOM node with an ownerDocument property ");
  }
}

JSTermFirefoxMixin.prototype = {
  /**
   * Generates and attaches the UI for an entire JS Workspace or
   * just the input node used under the console output
   *
   * @returns void
   */
  generateUI: function JSTF_generateUI()
  {
    let inputContainer = this.xulElementFactory("hbox");
    inputContainer.setAttribute("class", "jsterm-input-container");

    let inputNode = this.xulElementFactory("textbox");
    inputNode.setAttribute("class", "jsterm-input-node");
    inputNode.setAttribute("flex", "1");
    inputNode.setAttribute("multiline", "true");
    inputNode.setAttribute("rows", "1");
    inputContainer.appendChild(inputNode);

    let closeButton = this.xulElementFactory("button");
    closeButton.setAttribute("class", "jsterm-close-button");
    inputContainer.appendChild(closeButton);
    closeButton.addEventListener("command", HeadsUpDisplayUICommands.toggleHUD,
                                 false);

    if (this.existingConsoleNode == undefined) {
      // create elements
      let term = this.xulElementFactory("vbox");
      term.setAttribute("class", "jsterm-wrapper-node");
      term.setAttribute("flex", "1");

      let outputNode = this.xulElementFactory("vbox");
      outputNode.setAttribute("class", "jsterm-output-node");

      // construction
      term.appendChild(outputNode);
      term.appendChild(inputNode);

      this.outputNode = outputNode;
      this.inputNode = inputNode;
      this.term = term;
    }
    else {
      this.inputNode = inputNode;
      this.term = inputContainer;
      this.outputNode = this.existingConsoleNode;
    }
  },

  get inputValue()
  {
    return this.inputNode.value;
  },

  attachUI: function JSTF_attachUI()
  {
    this.parentNode.appendChild(this.term);
  }
};

/**
 * LogMessage represents a single message logged to the "outputNode" console
 */
function LogMessage(aMessage, aLevel, aOutputNode, aActivityObject)
{
  if (!aOutputNode || !aOutputNode.ownerDocument) {
    throw new Error("aOutputNode is required and should be type nsIDOMNode");
  }
  if (!aMessage.origin) {
    throw new Error("Cannot create and log a message without an origin");
  }
  this.message = aMessage;
  if (aMessage.domId) {
    // domId is optional - we only need it if the logmessage is
    // being asynchronously updated
    this.domId = aMessage.domId;
  }
  this.activityObject = aActivityObject;
  this.outputNode = aOutputNode;
  this.level = aLevel;
  this.origin = aMessage.origin;

  this.xulElementFactory =
  NodeFactory("xul", "xul", aOutputNode.ownerDocument);

  this.textFactory = NodeFactory("text", "xul", aOutputNode.ownerDocument);

  this.createLogNode();
}

LogMessage.prototype = {

  /**
   * create a console log div node
   *
   * @returns nsIDOMNode
   */
  createLogNode: function LM_createLogNode()
  {
    this.messageNode = this.xulElementFactory("label");

    var ts = ConsoleUtils.timestamp();
    this.timestampedMessage = ConsoleUtils.timestampString(ts) + ": " +
      this.message.message;
    var messageTxtNode = this.textFactory(this.timestampedMessage + "\n");

    this.messageNode.appendChild(messageTxtNode);

    this.messageNode.classList.add("hud-msg-node");
    this.messageNode.classList.add("hud-" + this.level);


    if (this.activityObject.category == "CSS Parser") {
      this.messageNode.classList.add("hud-cssparser");
    }

    var self = this;

    var messageObject = {
      logLevel: self.level,
      message: self.message,
      timestamp: ts,
      activity: self.activityObject,
      origin: self.origin,
      hudId: self.message.hudId,
    };

    this.messageObject = messageObject;
  }
};


/**
 * Firefox-specific Application Hooks.
 * Each Gecko-based application will need an object like this in
 * order to use the Heads Up Display
 */
function FirefoxApplicationHooks()
{ }

FirefoxApplicationHooks.prototype = {

  /**
   * Firefox-specific method for getting an array of chrome Window objects
   */
  get chromeWindows()
  {
    var windows = [];
    var enumerator = Services.ww.getWindowEnumerator(null);
    while (enumerator.hasMoreElements()) {
      windows.push(enumerator.getNext());
    }
    return windows;
  },

  /**
   * Firefox-specific method for getting the DOM node (per tab) that message
   * nodes are appended to.
   * @param aId
   *        The DOM node's id.
   */
  getOutputNodeById: function FAH_getOutputNodeById(aId)
  {
    if (!aId) {
      throw new Error("FAH_getOutputNodeById: id is null!!");
    }
    var enumerator = Services.ww.getWindowEnumerator(null);
    while (enumerator.hasMoreElements()) {
      let window = enumerator.getNext();
      let node = window.document.getElementById(aId);
      if (node) {
        return node;
      }
    }
    throw new Error("Cannot get outputNode by id");
  },

  /**
   * gets the current contentWindow (Firefox-specific)
   *
   * @returns nsIDOMWindow
   */
  getCurrentContext: function FAH_getCurrentContext()
  {
    return Services.wm.getMostRecentWindow("navigator:browser");
  }
};

//////////////////////////////////////////////////////////////////////////////
// Utility functions used by multiple callers
//////////////////////////////////////////////////////////////////////////////

/**
 * ConsoleUtils: a collection of globally used functions
 *
 */

ConsoleUtils = {

  /**
   * Generates a millisecond resolution timestamp.
   *
   * @returns integer
   */
  timestamp: function ConsoleUtils_timestamp()
  {
    return Date.now();
  },

  /**
   * Generates a formatted timestamp string for displaying in console messages.
   *
   * @param integer [ms] Optional, allows you to specify the timestamp in
   * milliseconds since the UNIX epoch.
   * @returns string The timestamp formatted for display.
   */
  timestampString: function ConsoleUtils_timestampString(ms)
  {
    var d = new Date(ms ? ms : null);
    let hours = d.getHours(), minutes = d.getMinutes();
    let seconds = d.getSeconds(), milliseconds = d.getMilliseconds();
    let parameters = [ hours, minutes, seconds, milliseconds ];
    return HUDService.getFormatStr("timestampFormat", parameters);
  },

  /**
   * Scrolls a node so that it's visible in its containing XUL "scrollbox"
   * element.
   *
   * @param nsIDOMNode aNode
   *        The node to make visible.
   * @returns void
   */
  scrollToVisible: function ConsoleUtils_scrollToVisible(aNode) {
    let scrollBoxNode = aNode.parentNode;
    while (scrollBoxNode.tagName !== "scrollbox") {
      scrollBoxNode = scrollBoxNode.parentNode;
    }

    let boxObject = scrollBoxNode.boxObject;
    let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    nsIScrollBoxObject.ensureElementIsVisible(aNode);
  },

  /**
   * Given an instance of nsIScriptError, attempts to work out the IDs of HUDs
   * to which the script should be sent.
   *
   * @param nsIScriptError aScriptError
   *        The script error that was received.
   * @returns Array<string>
   */
  getHUDIdsForScriptError:
  function ConsoleUtils_getHUDIdsForScriptError(aScriptError) {
    if (aScriptError instanceof Ci.nsIScriptError2) {
      let windowID = aScriptError.outerWindowID;
      if (windowID) {
        // We just need some arbitrary window here so that we can GI to
        // nsIDOMWindowUtils...
        let someWindow = Services.wm.getMostRecentWindow(null);
        if (someWindow) {
          let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                      .getInterface(Ci.nsIDOMWindowUtils);

          // In the future (post-Electrolysis), getOuterWindowWithId() could
          // return null, because the originating window could have gone away
          // while we were in the process of receiving and/or processing a
          // message. For future-proofing purposes, we do a null check here.
          let content = windowUtils.getOuterWindowWithId(windowID);
          if (content) {
            let hudId = HUDService.getHudIdByWindow(content);
            if (hudId) {
              return [ hudId ];
            }
          }
        }
      }
    }

    // The error had no window ID. As a less precise fallback, see whether we
    // can find some consoles to send to via the URI.
    let hudIds = HUDService.uriRegistry[aScriptError.sourceName];
    return hudIds ? hudIds : [];
  },

  /**
   * Returns the chrome window, the tab browser, and the browser that
   * contain the given content window.
   *
   * NB: This function only works in Firefox.
   *
   * @param nsIDOMWindow aContentWindow
   *        The content window to query. If this parameter is not a content
   *        window, then [ null, null, null ] will be returned.
   */
  getParents: function ConsoleUtils_getParents(aContentWindow) {
    let chromeEventHandler =
      aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIWebNavigation)
                    .QueryInterface(Ci.nsIDocShell)
                    .chromeEventHandler;
    if (!chromeEventHandler) {
      return [ null, null, null ];
    }

    let chromeWindow = chromeEventHandler.ownerDocument.defaultView;
    let gBrowser = XPCNativeWrapper.unwrap(chromeWindow).gBrowser;
    let documentElement = chromeWindow.document.documentElement;
    if (!documentElement || !gBrowser ||
        documentElement.getAttribute("windowtype") !== "navigator:browser") {
      // Not a browser window.
      return [ chromeWindow, null, null ];
    }

    return [ chromeWindow, gBrowser, chromeEventHandler ];
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

HeadsUpDisplayUICommands = {
  toggleHUD: function UIC_toggleHUD() {
    var window = HUDService.currentContext();
    var gBrowser = window.gBrowser;
    var linkedBrowser = gBrowser.selectedTab.linkedBrowser;
    var tabId = gBrowser.getNotificationBox(linkedBrowser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var ownerDocument = gBrowser.selectedTab.ownerDocument;
    var hud = ownerDocument.getElementById(hudId);
    if (hud) {
      HUDService.animate(hudId, ANIMATE_OUT, function() {
        // If the user closes the console while the console is animating away,
        // then these callbacks will queue up, but all the callbacks after the
        // first will have no console to operate on. This test handles this
        // case gracefully.
        if (ownerDocument.getElementById(hudId)) {
          HUDService.deactivateHUDForContext(gBrowser.selectedTab);
        }
      });
    }
    else {
      HUDService.activateHUDForContext(gBrowser.selectedTab);
      HUDService.animate(hudId, ANIMATE_IN);
    }
  },

  toggleFilter: function UIC_toggleFilter(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    var state = HUDService.getFilterState(hudId, filter);
    if (state) {
      HUDService.setFilterState(hudId, filter, false);
      aButton.setAttribute("checked", false);
    }
    else {
      HUDService.setFilterState(hudId, filter, true);
      aButton.setAttribute("checked", true);
    }
  },

  command: function UIC_command(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    switch (filter) {
      case "clear":
        HUDService.clearDisplay(hudId);
        break;
      case "selectAll":
        let outputNode = HUDService.getOutputNodeById(hudId);
        let chromeWindow = outputNode.ownerDocument.defaultView;
        let commandController = chromeWindow.commandController;
        commandController.selectAll(outputNode);
        break;
      case "saveBodies": {
        let checked = aButton.getAttribute("checked") === "true";
        HUDService.saveRequestAndResponseBodies = checked;
        break;
      }
    }
  },

};

//////////////////////////////////////////////////////////////////////////
// ConsoleStorage
//////////////////////////////////////////////////////////////////////////

var prefs = Services.prefs;

const GLOBAL_STORAGE_INDEX_ID = "GLOBAL_CONSOLE";
const PREFS_BRANCH_PREF = "devtools.hud.display.filter";
const PREFS_PREFIX = "devtools.hud.display.filter.";
const PREFS = { network: PREFS_PREFIX + "network",
                cssparser: PREFS_PREFIX + "cssparser",
                exception: PREFS_PREFIX + "exception",
                error: PREFS_PREFIX + "error",
                info: PREFS_PREFIX + "info",
                warn: PREFS_PREFIX + "warn",
                log: PREFS_PREFIX + "log",
                global: PREFS_PREFIX + "global",
              };

function ConsoleStorage()
{
  this.sequencer = null;
  this.consoleDisplays = {};
  // each display will have an index that tracks each ConsoleEntry
  this.displayIndexes = {};
  this.globalStorageIndex = [];
  this.globalDisplay = {};
  this.createDisplay(GLOBAL_STORAGE_INDEX_ID);
  // TODO: need to create a method that truncates the message
  // see bug 570543

  // store an index of display prefs
  this.displayPrefs = {};

  // check prefs for existence, create & load if absent, load them if present
  let filterPrefs;
  let defaultDisplayPrefs;

  try {
    filterPrefs = prefs.getBoolPref(PREFS_BRANCH_PREF);
  }
  catch (ex) {
    filterPrefs = false;
  }

  // TODO: for FINAL release,
  // use the sitePreferencesService to save specific site prefs
  // see bug 570545

  if (filterPrefs) {
    defaultDisplayPrefs = {
      network: (prefs.getBoolPref(PREFS.network) ? true: false),
      cssparser: (prefs.getBoolPref(PREFS.cssparser) ? true: false),
      exception: (prefs.getBoolPref(PREFS.exception) ? true: false),
      error: (prefs.getBoolPref(PREFS.error) ? true: false),
      info: (prefs.getBoolPref(PREFS.info) ? true: false),
      warn: (prefs.getBoolPref(PREFS.warn) ? true: false),
      log: (prefs.getBoolPref(PREFS.log) ? true: false),
      global: (prefs.getBoolPref(PREFS.global) ? true: false),
    };
  }
  else {
    prefs.setBoolPref(PREFS_BRANCH_PREF, false);
    // default prefs for each HeadsUpDisplay
    prefs.setBoolPref(PREFS.network, true);
    prefs.setBoolPref(PREFS.cssparser, true);
    prefs.setBoolPref(PREFS.exception, true);
    prefs.setBoolPref(PREFS.error, true);
    prefs.setBoolPref(PREFS.info, true);
    prefs.setBoolPref(PREFS.warn, true);
    prefs.setBoolPref(PREFS.log, true);
    prefs.setBoolPref(PREFS.global, false);

    defaultDisplayPrefs = {
      network: prefs.getBoolPref(PREFS.network),
      cssparser: prefs.getBoolPref(PREFS.cssparser),
      exception: prefs.getBoolPref(PREFS.exception),
      error: prefs.getBoolPref(PREFS.error),
      info: prefs.getBoolPref(PREFS.info),
      warn: prefs.getBoolPref(PREFS.warn),
      log: prefs.getBoolPref(PREFS.log),
      global: prefs.getBoolPref(PREFS.global),
    };
  }
  this.defaultDisplayPrefs = defaultDisplayPrefs;
}

ConsoleStorage.prototype = {

  updateDefaultDisplayPrefs:
  function CS_updateDefaultDisplayPrefs(aPrefsObject) {
    prefs.setBoolPref(PREFS.network, (aPrefsObject.network ? true : false));
    prefs.setBoolPref(PREFS.cssparser, (aPrefsObject.cssparser ? true : false));
    prefs.setBoolPref(PREFS.exception, (aPrefsObject.exception ? true : false));
    prefs.setBoolPref(PREFS.error, (aPrefsObject.error ? true : false));
    prefs.setBoolPref(PREFS.info, (aPrefsObject.info ? true : false));
    prefs.setBoolPref(PREFS.warn, (aPrefsObject.warn ? true : false));
    prefs.setBoolPref(PREFS.log, (aPrefsObject.log ? true : false));
    prefs.setBoolPref(PREFS.global, (aPrefsObject.global ? true : false));
  },

  sequenceId: function CS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer();
    }
    return this.sequencer.next();
  },

  createSequencer: function CS_createSequencer()
  {
    function sequencer(aInt) {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(-1);
  },

  globalStore: function CS_globalStore(aIndex)
  {
    return this.displayStore(GLOBAL_CONSOLE_DOM_NODE_ID);
  },

  displayStore: function CS_displayStore(aId)
  {
    var self = this;
    var idx = -1;
    var id = aId;
    var aLength = self.displayIndexes[id].length;

    function displayStoreGenerator(aInt, aLength)
    {
      // create a generator object to iterate through any of the display stores
      // from any index-starting-point
      while(1) {
        // throw if we exceed the length of displayIndexes?
        aInt++;
        var indexIt = self.displayIndexes[id];
        var index = indexIt[aInt];
        if (aLength < aInt) {
          // try to see if we have more entries:
          var newLength = self.displayIndexes[id].length;
          if (newLength > aLength) {
            aLength = newLength;
          }
          else {
            throw new StopIteration();
          }
        }
        var entry = self.consoleDisplays[id][index];
        yield entry;
      }
    }

    return displayStoreGenerator(-1, aLength);
  },

  recordEntries: function CS_recordEntries(aHUDId, aConfigArray)
  {
    var len = aConfigArray.length;
    for (var i = 0; i < len; i++){
      this.recordEntry(aHUDId, aConfigArray[i]);
    }
  },


  recordEntry: function CS_recordEntry(aHUDId, aConfig)
  {
    var id = this.sequenceId();

    this.globalStorageIndex[id] = { hudId: aHUDId };

    var displayStorage = this.consoleDisplays[aHUDId];

    var displayIndex = this.displayIndexes[aHUDId];

    if (displayStorage && displayIndex) {
      var entry = new ConsoleEntry(aConfig, id);
      displayIndex.push(entry.id);
      displayStorage[entry.id] = entry;
      return entry;
    }
    else {
      throw new Error("Cannot get displayStorage or index object for id " + aHUDId);
    }
  },

  getEntry: function CS_getEntry(aId)
  {
    var display = this.globalStorageIndex[aId];
    var storName = display.hudId;
    return this.consoleDisplays[storName][aId];
  },

  updateEntry: function CS_updateEntry(aUUID)
  {
    // update an individual entry
    // TODO: see bug 568634
  },

  createDisplay: function CS_createdisplay(aId)
  {
    if (!this.consoleDisplays[aId]) {
      this.consoleDisplays[aId] = {};
      this.displayIndexes[aId] = [];
    }
  },

  removeDisplay: function CS_removeDisplay(aId)
  {
    try {
      delete this.consoleDisplays[aId];
      delete this.displayIndexes[aId];
    }
    catch (ex) {
      Cu.reportError("Could not remove console display for id " + aId);
    }
  }
};

/**
 * A Console log entry
 *
 * @param JSObject aConfig, object literal with ConsolEntry properties
 * @param integer aId
 * @returns void
 */

function ConsoleEntry(aConfig, id)
{
  if (!aConfig.logLevel && aConfig.message) {
    throw new Error("Missing Arguments when creating a console entry");
  }

  this.config = aConfig;
  this.id = id;
  for (var prop in aConfig) {
    if (!(typeof aConfig[prop] == "function")){
      this[prop] = aConfig[prop];
    }
  }

  if (aConfig.logLevel == "network") {
    this.transactions = { };
    if (aConfig.activity) {
      this.transactions[aConfig.activity.stage] = aConfig.activity;
    }
  }

}

ConsoleEntry.prototype = {

  updateTransaction: function CE_updateTransaction(aActivity) {
    this.transactions[aActivity.stage] = aActivity;
  }
};

//////////////////////////////////////////////////////////////////////////
// HUDWindowObserver
//////////////////////////////////////////////////////////////////////////

HUDWindowObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver,]
  ),

  init: function HWO_init()
  {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "content-document-global-created", false);
  },

  observe: function HWO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "content-document-global-created") {
      HUDService.windowInitializer(aSubject);
    }
    else if (aTopic == "xpcom-shutdown") {
      this.uninit();
    }
  },

  uninit: function HWO_uninit()
  {
    Services.obs.removeObserver(this, "content-document-global-created");
    HUDService.shutdown();
  },

};

///////////////////////////////////////////////////////////////////////////////
// CommandController
///////////////////////////////////////////////////////////////////////////////

/**
 * A controller (an instance of nsIController) that makes editing actions
 * behave appropriately in the context of the Web Console.
 */
function CommandController(aWindow) {
  this.window = aWindow;
}

CommandController.prototype = {
  /**
   * Returns the HUD output node that currently has the focus, or null if the
   * currently-focused element isn't inside the output node.
   *
   * @returns nsIDOMNode
   *          The currently-focused output node.
   */
  _getFocusedOutputNode: function CommandController_getFocusedOutputNode()
  {
    let anchorNode = this.window.getSelection().anchorNode;
    while (!(anchorNode.nodeType === anchorNode.ELEMENT_NODE &&
             anchorNode.classList.contains("hud-output-node"))) {
      anchorNode = anchorNode.parentNode;
    }
    return anchorNode;
  },

  /**
   * Selects all the text in the HUD output.
   *
   * @param nsIDOMNode aOutputNode
   *        The HUD output node.
   * @returns void
   */
  selectAll: function CommandController_selectAll(aOutputNode)
  {
    let selection = this.window.getSelection();
    selection.removeAllRanges();
    selection.selectAllChildren(aOutputNode);
  },

  supportsCommand: function CommandController_supportsCommand(aCommand)
  {
    return aCommand === "cmd_selectAll" &&
           this._getFocusedOutputNode() != null;
  },

  isCommandEnabled: function CommandController_isCommandEnabled(aCommand)
  {
    return aCommand === "cmd_selectAll";
  },

  doCommand: function CommandController_doCommand(aCommand)
  {
    this.selectAll(this._getFocusedOutputNode());
  }
};

///////////////////////////////////////////////////////////////////////////////
// HUDConsoleObserver
///////////////////////////////////////////////////////////////////////////////

/**
 * HUDConsoleObserver: Observes nsIConsoleService for global consoleMessages,
 * if a message originates inside a contentWindow we are tracking,
 * then route that message to the HUDService for logging.
 */

HUDConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver]
  ),

  init: function HCO_init()
  {
    Services.console.registerListener(this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function HCO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      Services.console.unregisterListener(this);
      return;
    }

    if (!(aSubject instanceof Ci.nsIScriptError))
      return;

    switch (aSubject.category) {
      // We ignore chrome-originating errors as we only
      // care about content.
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
      case "FrameConstructor":
        return;

      default:
        let hudIds = ConsoleUtils.getHUDIdsForScriptError(aSubject);
        for (let i = 0; i < hudIds.length; i++) {
          HUDService.logActivity("console-listener", hudIds[i], aSubject);
        }
        return;
    }
  }
};

///////////////////////////////////////////////////////////////////////////
// appName
///////////////////////////////////////////////////////////////////////////

/**
 * Get the app's name so we can properly dispatch app-specific
 * methods per API call
 * @returns Gecko application name
 */
function appName()
{
  let APP_ID = Services.appinfo.QueryInterface(Ci.nsIXULRuntime).ID;

  let APP_ID_TABLE = {
    "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "FIREFOX" ,
    "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "THUNDERBIRD",
    "{a23983c0-fd0e-11dc-95ff-0800200c9a66}": "FENNEC" ,
    "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "SEAMONKEY",
  };

  let name = APP_ID_TABLE[APP_ID];

  if (name){
    return name;
  }
  throw new Error("appName: UNSUPPORTED APPLICATION UUID");
}

///////////////////////////////////////////////////////////////////////////
// HUDService (exported symbol)
///////////////////////////////////////////////////////////////////////////

try {
  // start the HUDService
  // This is in a try block because we want to kill everything if
  // *any* of this fails
  var HUDService = new HUD_SERVICE();
  HUDWindowObserver.init();
  HUDConsoleObserver.init();
  ConsoleAPIObserver.init();
}
catch (ex) {
  Cu.reportError("HUDService failed initialization.\n" + ex);
  // TODO: kill anything that may have started up
  // see bug 568665
}
