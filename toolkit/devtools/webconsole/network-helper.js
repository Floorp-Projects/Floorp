/* vim:set ts=2 sw=2 sts=2 et: */
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
 *  Mihai Sucan (Mozilla Corp.)
 */

const {Cc, Ci, Cu} = require("chrome");
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

/**
 * Helper object for networking stuff.
 *
 * Most of the following functions have been taken from the Firebug source. They
 * have been modified to match the Firefox coding rules.
 */
let NetworkHelper = {
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
    try {
      conv.charset = aCharset || "UTF-8";
      return conv.ConvertToUnicode(aText);
    }
    catch (ex) {
      return aText;
    }
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
   * @param string aCharset
   *        The content document charset, used when reading the POSTed data.
   * @returns string or null
   *          Returns the posted string if it was possible to read from aRequest
   *          otherwise null.
   */
  readPostTextFromRequest: function NH_readPostTextFromRequest(aRequest, aCharset)
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
      let text = this.readAndConvertFromStream(iStream, aCharset);

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
   * @param nsIDocShell aDocShell
   * @param string aCharset
   * @returns string or null
   *          Returns the posted string if it was possible to read from
   *          aDocShell otherwise null.
   */
  readPostTextFromPage: function NH_readPostTextFromPage(aDocShell, aCharset)
  {
    let webNav = aDocShell.QueryInterface(Ci.nsIWebNavigation);
    return this.readPostTextFromPageViaWebNav(webNav, aCharset);
  },

  /**
   * Reads the posted text from the page's cache, given an nsIWebNavigation
   * object.
   *
   * @param nsIWebNavigation aWebNav
   * @param string aCharset
   * @returns string or null
   *          Returns the posted string if it was possible to read from
   *          aWebNav, otherwise null.
   */
  readPostTextFromPageViaWebNav:
  function NH_readPostTextFromPageViaWebNav(aWebNav, aCharset)
  {
    if (aWebNav instanceof Ci.nsIWebPageDescriptor) {
      let descriptor = aWebNav.currentDescriptor;

      if (descriptor instanceof Ci.nsISHEntry && descriptor.postData &&
          descriptor instanceof Ci.nsISeekableStream) {
        descriptor.seek(NS_SEEK_SET, 0);

        return this.readAndConvertFromStream(descriptor, aCharset);
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
    try {
      return this.getRequestLoadContext(aRequest).associatedWindow;
    } catch (ex) {
      // TODO: bug 802246 - getWindowForRequest() throws on b2g: there is no
      // associatedWindow property.
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
    try {
      return aRequest.notificationCallbacks.getInterface(Ci.nsILoadContext);
    } catch (ex) { }

    try {
      return aRequest.loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
    } catch (ex) { }

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
      aCallback(this.readAndConvertFromStream(aInputStream, contentCharset));
    });
  },

  /**
   * Parse a raw Cookie header value.
   *
   * @param string aHeader
   *        The raw Cookie header value.
   * @return array
   *         Array holding an object for each cookie. Each object holds the
   *         following properties: name and value.
   */
  parseCookieHeader: function NH_parseCookieHeader(aHeader)
  {
    let cookies = aHeader.split(";");
    let result = [];

    cookies.forEach(function(aCookie) {
      let equal = aCookie.indexOf("=");
      let name = aCookie.substr(0, equal);
      let value = aCookie.substr(equal + 1);
      result.push({name: unescape(name.trim()),
                   value: unescape(value.trim())});
    });

    return result;
  },

  /**
   * Parse a raw Set-Cookie header value.
   *
   * @param string aHeader
   *        The raw Set-Cookie header value.
   * @return array
   *         Array holding an object for each cookie. Each object holds the
   *         following properties: name, value, secure (boolean), httpOnly
   *         (boolean), path, domain and expires (ISO date string).
   */
  parseSetCookieHeader: function NH_parseSetCookieHeader(aHeader)
  {
    let rawCookies = aHeader.split(/\r\n|\n|\r/);
    let cookies = [];

    rawCookies.forEach(function(aCookie) {
      let equal = aCookie.indexOf("=");
      let name = unescape(aCookie.substr(0, equal).trim());
      let parts = aCookie.substr(equal + 1).split(";");
      let value = unescape(parts.shift().trim());

      let cookie = {name: name, value: value};

      parts.forEach(function(aPart) {
        let part = aPart.trim();
        if (part.toLowerCase() == "secure") {
          cookie.secure = true;
        }
        else if (part.toLowerCase() == "httponly") {
          cookie.httpOnly = true;
        }
        else if (part.indexOf("=") > -1) {
          let pair = part.split("=");
          pair[0] = pair[0].toLowerCase();
          if (pair[0] == "path" || pair[0] == "domain") {
            cookie[pair[0]] = pair[1];
          }
          else if (pair[0] == "expires") {
            try {
              pair[1] = pair[1].replace(/-/g, ' ');
              cookie.expires = new Date(pair[1]).toISOString();
            }
            catch (ex) { }
          }
        }
      });

      cookies.push(cookie);
    });

    return cookies;
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
    "application/json-rpc": "json",
    "application/x-web-app-manifest+json": "json",
  },

  /**
   * Check if the given MIME type is a text-only MIME type.
   *
   * @param string aMimeType
   * @return boolean
   */
  isTextMimeType: function NH_isTextMimeType(aMimeType)
  {
    if (aMimeType.indexOf("text/") == 0) {
      return true;
    }

    if (/^application\/[a-z-]+\+xml$/.test(aMimeType)) {
      return true;
    }

    switch (this.mimeCategoryMap[aMimeType]) {
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
};

for (let prop of Object.getOwnPropertyNames(NetworkHelper)) {
  exports[prop] = NetworkHelper[prop];
}
