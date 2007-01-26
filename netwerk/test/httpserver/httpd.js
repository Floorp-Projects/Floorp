/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is the MozJSHTTP server.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher (v1, netwerk/test/TestServ.js)
 *   Christian Biesinger (v2, netwerk/test/unit/head_http_server.js)
 *   Jeff Walden <jwalden+code@mit.edu> (v3, netwerk/test/httpserver/httpd.js)
 *   Robert Sayre <sayrer@gmail.com>
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

/*
 * An implementation of an HTTP server both as a loadable script and as an XPCOM
 * component.  See the accompanying README file for user documentation on
 * MozJSHTTP.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

/**
 * Asserts that the given condition holds.  If it doesn't, the given message is
 * dumped, a stack trace is printed, and an exception is thrown to attempt to
 * stop execution (which unfortunately must rely upon the exception not being
 * accidentally swallowed by the code that uses it).
 */
function NS_ASSERT(cond, msg)
{
  if (debugEnabled() && !cond)
  {
    dumpn("###!!!");
    dumpn("###!!! ASSERTION" + (msg ? ": " + msg : "!"));
    dumpn("###!!! Stack follows:");

    var stack = new Error().stack.split(/\n/);
    dumpn(stack.map(function(val) { return "###!!!   " + val; }).join("\n"));
    
    throw msg;
  }
}

/** Constructs an HTTP error object. */
function HttpError(code, description)
{
  this.code = code;
  this.description = description;
}

/**
 * Errors thrown to trigger specific HTTP server responses.
 */
const HTTP_400 = new HttpError(400, "Bad Request");
const HTTP_401 = new HttpError(401, "Unauthorized");
const HTTP_402 = new HttpError(402, "Payment Required");
const HTTP_403 = new HttpError(403, "Forbidden");
const HTTP_404 = new HttpError(404, "Not Found");
const HTTP_405 = new HttpError(405, "Method Not Allowed");
const HTTP_406 = new HttpError(406, "Not Acceptable");
const HTTP_407 = new HttpError(407, "Proxy Authentication Required");
const HTTP_408 = new HttpError(408, "Request Timeout");
const HTTP_409 = new HttpError(409, "Conflict");
const HTTP_410 = new HttpError(410, "Gone");
const HTTP_411 = new HttpError(411, "Length Required");
const HTTP_412 = new HttpError(412, "Precondition Failed");
const HTTP_413 = new HttpError(413, "Request Entity Too Large");
const HTTP_414 = new HttpError(414, "Request-URI Too Long");
const HTTP_415 = new HttpError(415, "Unsupported Media Type");
const HTTP_416 = new HttpError(416, "Requested Range Not Satisfiable");
const HTTP_417 = new HttpError(417, "Expectation Failed");

const HTTP_500 = new HttpError(500, "Internal Server Error");
const HTTP_501 = new HttpError(501, "Not Implemented");
const HTTP_502 = new HttpError(502, "Bad Gateway");
const HTTP_503 = new HttpError(503, "Service Unavailable");
const HTTP_504 = new HttpError(504, "Gateway Timeout");
const HTTP_505 = new HttpError(505, "HTTP Version Not Supported");

/** Creates a hash with fields corresponding to the values in arr. */
function array2obj(arr)
{
  var obj = {};
  for (var i = 0; i < arr.length; i++)
    obj[arr[i]] = arr[i];
  return obj;
}

/** Returns an array of the integers x through y, inclusive. */
function range(x, y)
{
  var arr = [];
  for (var i = x; i <= y; i++)
    arr.push(i);
  return arr;
}

/** An object (hash) whose fields are the numbers of all HTTP error codes. */
const HTTP_ERROR_CODES = array2obj(range(400, 417).concat(range(500, 505)));

/**
 * Returns true if debugging is enabled, false otherwise.
 */
function debugEnabled()
{
  return true; // this code's pretty new and has no good set of tests (yet)
}

/** dump(str) with a trailing "\n" */
function dumpn(str)
{
  if (debugEnabled())
    dump(str + "\n");
}


/**
 * Returns the RFC 822/1123 representation of a date.
 *
 * @param date
 *   the date, in milliseconds from midnight (00:00:00), January 1, 1970 GMT
 * @returns
 *   the string specifying the given date
 */
function toDateString(date)
{
  //
  // rfc1123-date = wkday "," SP date1 SP time SP "GMT"
  // date1        = 2DIGIT SP month SP 4DIGIT
  //                ; day month year (e.g., 02 Jun 1982)
  // time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
  //                ; 00:00:00 - 23:59:59
  // wkday        = "Mon" | "Tue" | "Wed"
  //              | "Thu" | "Fri" | "Sat" | "Sun"
  // month        = "Jan" | "Feb" | "Mar" | "Apr"
  //              | "May" | "Jun" | "Jul" | "Aug"
  //              | "Sep" | "Oct" | "Nov" | "Dec"
  //

  const wkdayStrings = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
  const monthStrings = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

  /**
   * Processes a date and returns the encoded UTC time as a string according to
   * the format specified in RFC 2616.
   *
   * @param date
   *   the date as a JavaScript Date object
   * @returns
   *   a string of the form "HH:MM:SS", ranging from "00:00:00" to "23:59:59"
   */
  function toTime(date)
  {
    var hrs = date.getUTCHours();
    var rv  = (hrs < 10) ? "0" + hrs : hrs;
    
    var mins = date.getUTCMinutes();
    rv += ":";
    rv += (mins < 10) ? "0" + mins : mins;

    var secs = date.getUTCSeconds();
    rv += ":";
    rv += (secs < 10) ? "0" + secs : secs;

    return rv;
  }

  /**
   * Processes a date and returns the encoded UTC date as a string according to
   * the date1 format specified in RFC 2616.
   *
   * @param date
   *   the date as a JavaScript Date object
   * @returns
   *   a string of the form "HH:MM:SS", ranging from "00:00:00" to "23:59:59"
   */
  function toDate1(date)
  {
    var day = date.getUTCDate();
    var month = date.getUTCMonth();
    var year = date.getUTCFullYear();

    var rv = (day < 10) ? "0" + day : day;
    rv += " " + monthStrings[month];
    rv += " " + year;

    return rv;
  }

  date = new Date(date);

  const fmtString = "%wkday%, %date1% %time% GMT";
  var rv = fmtString.replace("%wkday%", wkdayStrings[date.getUTCDay()]);
  rv = rv.replace("%time%", toTime(date));
  return rv.replace("%date1%", toDate1(date));
}

/**
 * Prints out a human-readable representation of the object o and its fields,
 * omitting those whose names begin with "_" if showMembers != true (to ignore
 * hidden properties exposed via getters/setters).
 */
function printObj(o, showMembers)
{
  var s = "******************************\n";
  s +=    "o = {\n";
  for (var i in o)
  {
    if (typeof(i) != "string" ||
        (showMembers || (i.length > 0 && i[0] != "_")))
      s+= "      " + i + ": " + o[i] + ",\n";
  }
  s +=    "    };\n";
  s +=    "******************************";
  dumpn(s);
}

/**
 * Instantiates a new HTTP server.
 */
function nsHttpServer()
{
  /** The port on which this server listens. */
  this._port = undefined;

  /** The socket associated with this. */
  this._socket = null;

  /** The handler used to process requests to this server. */
  this._handler = new ServerHandler(this);

  /**
   * Indicates when the server is to be shut down at the end of the request.
   */
  this._doQuit = false;

  /**
   * True if the socket in this is closed (and closure notifications have been
   * sent and processed if the socket was ever opened), false otherwise.
   */
  this._socketClosed = true;
}
nsHttpServer.prototype =
{
  // NSISERVERSOCKETLISTENER

  /**
   * This function signals that a new connection has been accepted.  It is the
   * method through which all requests must be handled, and by the end of this
   * method any and all response requests must be sent.
   *
   * @see nsIServerSocketListener.onSocketAccepted
   */
  onSocketAccepted: function(serverSocket, trans)
  {
    dumpn(">>> new connection on " + trans.host + ":" + trans.port);

    var input = trans.openInputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
    var output = trans.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);

    this._processConnection(serverSocket.port, input, output);
  },

  /**
   * Called when the socket associated with this is closed.
   *
   * @see nsIServerSocketListener.onStopListening
   */
  onStopListening: function(serverSocket, status)
  {
    dumpn(">>> shutting down server");
    this._socketClosed = true;
  },

  // NSIHTTPSERVER

  //
  // see nsIHttpServer.start
  //
  start: function(port)
  {
    if (this._socket)
      throw Cr.NS_ERROR_ALREADY_INITIALIZED;

    this._port = port;
    this._doQuit = this._socketClosed = false;

    var socket = Cc["@mozilla.org/network/server-socket;1"]
                   .createInstance(Ci.nsIServerSocket);
    socket.init(this._port,
                true,       // loopback only
                -1);        // default number of pending connections

    dumpn(">>> listening on port " + socket.port);
    socket.asyncListen(this);
    this._socket = socket;
  },

  //
  // see nsIHttpServer.stop
  //
  stop: function()
  {
    if (!this._socket)
      return;

    dumpn(">>> stopping listening on port " + this._socket.port);
    this._socket.close();
    this._socket = null;
    this._doQuit = false;

    // spin an event loop and wait for the socket-close notification, if we can;
    // this is only possible in Mozilla 1.9 code, but in situations where
    // nothing really horrible happens at shutdown, nothing bad will happen in
    // Mozilla 1.8-based code (and we want to support that)
    if ("@mozilla.org/thread-manager;1" in Cc)
    {
      var thr = Cc["@mozilla.org/thread-manager;1"]
                  .getService()
                  .currentThread;
      while (!this._socketClosed || this._handler.hasPendingRequests())
        thr.processNextEvent(true);
    }
  },

  //
  // see nsIHttpServer.registerFile
  //
  registerFile: function(path, file)
  {
    if (!file.exists() || lf.isDirectory())
      throw Cr.NS_ERROR_INVALID_ARG;

    this._handler.registerFile(path, file);
  },

  //
  // see nsIHttpServer.registerDirectory
  //
  registerDirectory: function(path, directory)
  {
    // XXX true path validation!
    if (path.charAt(0) != "/" ||
        path.charAt(path.length - 1) != "/" ||
        (directory &&
         (!directory.exists() || !directory.isDirectory())))
      throw Cr.NS_ERROR_INVALID_ARG;

    // XXX determine behavior of non-existent /foo/bar when a /foo/bar/ mapping
    //     exists!

    this._handler.registerDirectory(path, directory);
  },

  //
  // see nsIHttpServer.registerPathHandler
  //
  registerPathHandler: function(path, handler)
  {
    this._handler.registerPathHandler(path, handler);
  },

  //
  // see nsIHttpServer.registerErrorHandler
  //
  registerErrorHandler: function(code, handler)
  {
    this._handler.registerErrorHandler(code, handler);
  },

  //
  // see nsIHttpServer.setIndexHandler
  //
  setIndexHandler: function(handler)
  {
    this._handler.setIndexHandler(handler);
  },

  // NSISUPPORTS

  //
  // see nsISupports.QueryInterface
  //
  QueryInterface: function(iid)
  {
    if (iid.equals(Ci.nsIHttpServer) ||
        iid.equals(Ci.nsIServerSocketListener) ||
        iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // NON-XPCOM PUBLIC API

  /**
   * Returns true iff this server is not running (and is not in the process of
   * serving any requests still to be processed when the server was last
   * stopped after being run).
   */
  isStopped: function()
  {
    return this._socketClosed && !this._handler.hasPendingRequests();
  },
  
  // PRIVATE IMPLEMENTATION

  /**
   * Processes an incoming request in inStream served through the given port and
   * writes the response to outStream.
   *
   * @param port
   *   the port on which the request was served
   * @param inStream
   *   an nsIInputStream containing the incoming request
   * @param outStream
   *   the nsIOutputStream to which the response should be written
   */
  _processConnection: function(port, inStream, outStream)
  {
    try
    {
      var metadata = new RequestMetadata(port);
      metadata.init(inStream);

      inStream.close();

      this._handler.handleRequest(outStream, metadata);
    }
    catch (e)
    {
      dumpn(">>> internal error, shutting down server: " + e);
      dumpn("*** stack trace: " + e.stack);

      inStream.close(); // in case we failed before then

      this._doQuit = true;
      this._endConnection(this._handler, outStream);
    }

    // stream cleanup, etc. happens in _endConnection below -- called from
    // stream copier observer when all data has been copied
  },

  /**
   * Closes the passed-in output stream and (if necessary) shuts down this
   * server.
   *
   * @param handler
   *   the request handler which handled the request
   * @param outStream
   *   the nsIOutputStream for the processed request which must be closed
   */
  _endConnection: function(handler, outStream)
  {
    //
    // Order is important below: we must decrement handler._pendingRequests
    // BEFORE calling this.stop(), if needed, because this.stop() returns only
    // when the server socket's closed AND all pending requests are complete,
    // which clearly isn't (and never will be) the case if it were the other way
    // around
    //

    outStream.close();  // inputstream already closed after processing
    handler._pendingRequests--;

    // handle possible server quit -- note that this doesn't affect extant open
    // connections and pending requests: nsIServerSocket.close is specified not
    // to affect open connections, and nsIHttpServer.stop attempts to do its
    // best to return only when the server socket is closed and all pending
    // requests have been served
    if (this._doQuit)
      this.stop();
  },

  /**
   * Requests that the server be shut down when possible.
   */
  _requestQuit: function()
  {
    dumpn(">>> requesting a quit");
    this._doQuit = true;
  }

};


/**
 * Gets a content-type for the given file, as best as it is possible to do so.
 *
 * @param file
 *   the nsIFile for which to get a file type
 * @returns
 *   the best content-type which can be determined for the file
 */
function getTypeFromFile(file)
{
  try
  {
    var type = Cc["@mozilla.org/uriloader/external-helper-app-service;1"]
                 .getService(Ci.nsIMIMEService)
                 .getTypeFromFile(file);
  }
  catch (e)
  {
    type = "application/octet-stream";
  }
  return type;
}



/**
 * Creates a request-handling function for an nsIHttpRequestHandler object.
 */
function createHandlerFunc(handler)
{
  return function(metadata, response) { handler.handle(metadata, response); };
}


/**
 * The default handler for directories; writes an HTML response containing a
 * slightly-formatted directory listing.
 */
function defaultIndexHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/html", false);

  var path = htmlEscape(metadata.path);

  //
  // Just do a very basic bit of directory listings -- no need for too much
  // fanciness, especially since we don't have a style sheet in which we can
  // stick rules (don't want to pollute the default path-space).
  //

  var body = '<html>\
                <head>\
                  <title>' + path + '</title>\
                </head>\
                <body>\
                  <h1>' + path + '</h1>\
                  <ol style="list-style-type: none">';

  var directory = metadata.getProperty("directory");
  NS_ASSERT(directory && directory.isDirectory());

  var fileList = [];
  var files = directory.directoryEntries;
  while (files.hasMoreElements())
  {
    var f = files.getNext().QueryInterface(Ci.nsIFile);
    if (!f.isHidden())
      fileList.push(f);
  }

  fileList.sort(fileSort);

  for (var i = 0; i < fileList.length; i++)
  {
    var file = fileList[i];
    try
    {
      var name = file.leafName;
      var sep = file.isDirectory() ? "/" : "";

      // Note: using " to delimit the attribute here because encodeURIComponent
      //       passes through '.
      var item = '<li><a href="' + encodeURIComponent(name) + sep + '">' +
                   htmlEscape(name) + sep +
                 '</a></li>';

      body += item;
    }
    catch (e) { /* some file system error, ignore the file */ }
  }

  body    += '    </ol>\
                </body>\
              </html>';

  response.bodyOutputStream.write(body, body.length);
}

/**
 * Sorts a and b (nsIFile objects) into an aesthetically pleasing order.
 */
function fileSort(a, b)
{
  var dira = a.isDirectory(), dirb = b.isDirectory();

  if (dira && !dirb)
    return -1;
  if (dirb && !dira)
    return 1;

  var namea = a.leafName.toLowerCase(), nameb = b.leafName.toLowerCase();
  return nameb > namea ? -1 : 1;
}


/**
 * An object which handles requests for a server, executing default and
 * overridden behaviors as instructed by the code which uses and manipulates it.
 * Default behavior includes the paths / and /trace (diagnostics), with some
 * support for HTTP error pages for various codes and fallback to HTTP 500 if
 * those codes fail for any reason.
 *
 * @param srv
 *   the nsHttpServer in which this handler is being used
 */
function ServerHandler(srv)
{
  // FIELDS

  /**
   * The nsHttpServer instance associated with this handler.
   */
  this._server = srv;

  /**
   * A variable used to ensure that all requests are fully complete before the
   * server shuts down, to avoid callbacks from compiled code calling back into
   * empty contexts.  See also the comment before this field is next modified.
   */
  this._pendingRequests = 0;

  /**
   * A FileMap object containing the set of path->nsILocalFile mappings for
   * all directory mappings set in the server (e.g., "/" for /var/www/html/,
   * "/foo/bar/" for /local/path/, and "/foo/bar/baz/" for /local/path2).
   *
   * Note carefully: the leading and trailing "/" in each path (not file) are
   * removed before insertion to simplify the code which uses this.  You have
   * been warned!
   */
  this._pathDirectoryMap = new FileMap();

  /**
   * Custom request handlers for the server in which this resides.  Path-handler
   * pairs are stored as property-value pairs in this property.
   *
   * @see also ServerHandler.prototype._defaultPaths
   */
  this._overridePaths = {};

  /**
   * Custom request handlers for the error handlers in the server in which this
   * resides.  Path-handler pairs are stored as property-value pairs in this
   * property.
   *
   * @see also ServerHandler.prototype._defaultErrors
   */
  this._overrideErrors = {};

  /**
   * Init our default handler for directories. The handler used to
   * serve directories when no index file is present.
   */
  this._indexHandler = defaultIndexHandler;
}
ServerHandler.prototype =
{
  // PUBLIC API

  /**
   * Handles a request to this server, responding to the request appropriately
   * and initiating server shutdown if necessary.  If the request metadata
   * specifies an error code, the appropriate error response will be sent.
   *
   * @param outStream
   *   an nsIOutputStream to which a response should be written
   * @param metadata
   *   request metadata as generated from the initial request
   */
  handleRequest: function(outStream, metadata)
  {
    var response = new Response();

    // handle any existing error codes
    if (metadata.errorCode)
    {
      dumpn("*** errorCode == " + metadata.errorCode);
      this._handleError(metadata, response);
      return this._end(response, outStream);
    }

    var path = metadata.path;
    dumpn("*** path == " + path);

    try
    {
      try
      {
        // explicit paths first, then files based on existing directory mappings,
        // then (if the file doesn't exist) built-in server default paths
        if (path in this._overridePaths)
          this._overridePaths[path](metadata, response);
        else
          this._handleDefault(metadata, response);
      }
      catch (e)
      {
        response.recycle();

        if (!(e instanceof HttpError) || e.code != 404)
          throw HTTP_500;

        if (path in this._defaultPaths)
          this._defaultPaths[path](metadata, response);
        else
          throw HTTP_404;
      }
    }
    catch (e2)
    {
      if (!(e2 instanceof HttpError))
      {
        dumpn("*** internal error: e2 == " + e2);
        throw e2;
      }

      var errorCode = e2.code;
      dumpn("*** errorCode == " + errorCode);

      response.recycle();

      metadata.errorCode = errorCode;
      this._handleError(metadata, response);
    }

    return this._end(response, outStream);
  },

  /**
   * Associates a file with a server path so that it is returned by future
   * requests to that path.
   *
   * @param path
   *   the path on the server, which must begin with a "/"
   * @param file
   *   the nsILocalFile representing the file to be served; must not be a
   *   directory
   */
  registerFile: function(path, file)
  {
    dumpn("*** registering '" + path + "' as mapping to " + file.path);
    file = file.clone();

    this._overridePaths[path] =
      function(metadata, response)
      {
        if (!file.exists())
          throw HTTP_404;

        response.setStatusLine(metadata.httpVersion, 200, "OK");

        try
        {
          this._writeFileResponse(file, response);
        }
        catch (e)
        {
          // something happened -- but the calling code should handle the error
          // and clean up the response
          throw e;
        }
      };
  },

  //
  // see nsIHttpServer.registerPathHandler
  //
  registerPathHandler: function(path, handler)
  {
    // XXX true path validation!
    if (path.charAt(0) != "/")
      throw Cr.NS_ERROR_INVALID_ARG;

    this._handlerToField(handler, this._overridePaths, path);
  },

  //
  // see nsIHttpServer.registerDirectory
  //
  registerDirectory: function(path, directory)
  {
    // strip off leading and trailing '/' so that we can use lastIndexOf when
    // determining exactly how a path maps onto a mapped directory --
    // conditional is required here to deal with "/".substring(1, 0) being
    // converted to "/".substring(0, 1) per the JS specification
    var key = path.length == 1 ? "" : path.substring(1, path.length - 1);

    if (directory)
    {
      dumpn("*** mapping '" + path + "' to the location " + directory.path);
      this._pathDirectoryMap.put(key, directory);
    }
    else
    {
      dumpn("*** removing mapping for '" + path + "'");
      this._pathDirectoryMap.put(key, null);
    }
  },

  //
  // see nsIHttpServer.registerErrorHandler
  //
  registerErrorHandler: function(err, handler)
  {
    if (!(err in HTTP_ERROR_CODES))
      dumpn("*** WARNING: registering non-HTTP/1.1 error code " +
            "(" + err + ") handler -- was this intentional?");

    this._handlerToField(handler, this._overrideErrors, err);
  },

  //
  // see nsIHttpServer.setIndexHandler
  //
  setIndexHandler: function(handler)
  {
    if (!handler)
      handler = defaultIndexHandler;
    else if (typeof(handler) != "function")
      handler = createHandlerFunc(handler);

    this._indexHandler = handler;
  },

  /**
   * Set or remove a handler in an ojbect with a key.
   * If handler is null, the key will be deleted.
   *
   * @param handler
   *   A function or an nsIHttpRequestHandler object.
   * @param dict
   *   The object to attach the handler to.
   * @param key
   *   The field name of the handler.
   */
  _handlerToField: function(handler, dict, key) {
    // for convenience, handler can be a function if this is run from xpcshell
    if (typeof(handler) == "function")
      dict[key] = handler;
    else if (handler)
      dict[key] = createHandlerFunc(handler);
    else
      delete dict[key];
  },

  /**
   * Returns true if this handler is in the middle of handling any current
   * requests; this must be false before the server in which this is used may be
   * safely shut down.
   */
  hasPendingRequests: function()
  {
    return this._pendingRequests > 0;
  },

  // PRIVATE API

  /**
   * Handles a request which maps to a file in the local filesystem (if a base
   * path has already been set; otherwise the 404 error is thrown).
   *
   * @param metadata
   *   request-related data as a RequestMetadata object
   * @param response
   *   an uninitialized Response to the given request which must be initialized
   *   by a request handler
   * @throws HTTP_###
   *   if an HTTP error occurred (usually HTTP_404); note that in this case the
   *   calling code must handle cleanup of the response by calling .destroy()
   *   or .recycle()
   */
  _handleDefault: function(metadata, response)
  {
    try
    {
      response.setStatusLine(metadata.httpVersion, 200, "OK");

      var path = metadata.path;
      NS_ASSERT(path.charAt(0) == "/");

      // determine the actual on-disk file; this requires finding the deepest
      // path-to-directory mapping in the requested URL
      var file = this._getFileForPath(path);

      // the "file" might be a directory, in which case we either serve the
      // contained index.html or make the index handler write the response
      if (file.exists() && file.isDirectory())
      {
        file.append("index.html"); // make configurable?
        if (!file.exists() || file.isDirectory())
        {
          metadata._bag.setPropertyAsInterface("directory", file.parent);
          this._indexHandler(metadata, response);
          return;
        }
      }

      // alternately, the file might not exist
      if (!file.exists())
        throw HTTP_404;

      // finally...
      dumpn("*** handling '" + path + "' as mapping to " + file.path);
      this._writeFileResponse(file, response);
    }
    catch (e)
    {
      // something failed, but make the calling code handle Response cleanup
      throw e;
    }
  },

  /**
   * Writes an HTTP response for the given file, including setting headers for
   * file metadata.
   *
   * @param file
   *   the file which is to be sent in the response
   * @param response
   *   the response to which the file should be written
   */
  _writeFileResponse: function(file, response)
  {
    try
    {
      response.setHeader("Last-Modified",
                         toDateString(file.lastModifiedTime),
                         false);
    }
    catch (e) { /* lastModifiedTime threw, ignore */ }

    response.setHeader("Content-Type", getTypeFromFile(file), false);

    var fis = Cc["@mozilla.org/network/file-input-stream;1"]
                .createInstance(Ci.nsIFileInputStream);
    const PR_RDONLY = 0x01;
    fis.init(file, PR_RDONLY, 0444, Ci.nsIFileInputStream.CLOSE_ON_EOF);
    response.bodyOutputStream.writeFrom(fis, file.fileSize);
    fis.close();
  },

  /**
   * Returns the nsILocalFile which corresponds to the path, as determined using
   * all registered path->directory mappings and any paths which are explicitly
   * overridden.
   *
   * @returns
   *   the nsILocalFile which is to be sent as the response to a request for
   *   path
   * @throws HttpError
   *   when the correct action is the corresponding HTTP error (i.e., because no
   *   mapping was found for a directory in path, the referenced file doesn't
   *   exist, etc.)
   */
  _getFileForPath: function(path)
  {
    var pathMap = this._pathDirectoryMap;

    // first, decode path and strip the leading '/'
    try
    {
      path = decodeURI(path);
    }
    catch (e)
    {
      throw HTTP_400; // malformed path
    }

    // next, get the directory which contains this path

    // An example progression of tmp for a path "/foo/bar/baz/" might be:
    // "foo/bar/baz/", "foo/bar/baz", "foo/bar", "foo", ""
    var tmp = path.substring(1);
    while (true)
    {
      // do we have a match for current head of the path?
      var file = pathMap.get(tmp);
      if (file)
      {
        // XXX hack; basically disable showing mapping for /foo/bar/ when the
        //     requested path was /foo/bar, because relative links on the page
        //     will all be incorrect -- we really need the ability to easily
        //     redirect here instead
        if (tmp == path.substring(1) &&
            tmp.length != 0 &&
            tmp.charAt(tmp.length - 1) != "/")
          file = null;
        else
          break;
      }

      // if we've finished trying all prefixes, exit
      if (tmp == "")
        break;

      tmp = tmp.substring(0, tmp.lastIndexOf("/"));
    }

    // no mapping applies, so 404
    if (!file)
      throw HTTP_404;


    // last, get the file for the path within the determined directory
    var parentFolder = file.parent;
    var dirIsRoot = (parentFolder == null);

    // Strategy here is to append components individually, making sure we
    // never move above the given directory; this allows paths such as
    // "<file>/foo/../bar" but prevents paths such as "<file>/../base-sibling";
    // this component-wise approach also means the code works even on platforms
    // which don't use "/" as the directory separator, such as Windows
    var leafPath = path.substring(tmp.length + 1);
    var comps = leafPath.split("/");
    for (var i = 0, sz = comps.length; i < sz; i++)
    {
      var comp = comps[i];

      if (comp == "..")
        file = file.parent;
      else if (comp == "." || comp == "")
        continue;
      else
        file.append(comp);

      if (!dirIsRoot && file.equals(parentFolder))
        throw HTTP_403;
    }

    return file;
  },

  /**
   * Handles a request which generates the given error code, using the
   * user-defined error handler if one has been set, gracefully falling back to
   * the x00 status code if the code has no handler, and failing to status code
   * 500 if all else fails.
   *
   * @param metadata
   *   metadata for the request, which will often be incomplete since this is an
   *   error -- must have its .errorCode set for the desired error
   * @param response
   *   an uninitialized Response should be initialized when this method
   *   completes with information which represents the desired error code in the
   *   ideal case or a fallback code in abnormal circumstances (i.e., 500 is a
   *   fallback for 505, per HTTP specs)
   */
  _handleError: function(metadata, response)
  {
    if (!metadata)
      throw Cr.NS_ERROR_NULL_POINTER;

    var errorCode = metadata.errorCode;
    var errorX00 = errorCode - (errorCode % 100);

    try
    {
      if (!(errorCode in HTTP_ERROR_CODES))
        dumpn("*** WARNING: requested invalid error: " + errorCode);

      // RFC 2616 says that we should try to handle an error by its class if we
      // can't otherwise handle it -- if that fails, we revert to handling it as
      // a 500 internal server error, and if that fails we throw and shut down
      // the server

      // actually handle the error
      try
      {
        if (errorCode in this._overrideErrors)
          this._overrideErrors[errorCode](metadata, response);
        else if (errorCode in this._defaultErrors)
          this._defaultErrors[errorCode](metadata, response);
      }
      catch (e)
      {
        // don't retry the handler that threw
        if (errorX00 == errorCode)
          throw HTTP_500;

        dumpn("*** error in handling for error code " + errorCode + ", " +
              "falling back to " + errorX00 + "...");
        if (errorX00 in this._overrideErrors)
          this._overrideErrors[errorX00](metadata, response);
        else if (errorX00 in this._defaultErrors)
          this._defaultErrors[errorX00](metadata, response);
        else
          throw HTTP_500;
      }
    }
    catch (e)
    {
      response.recycle();

      // we've tried everything possible for a meaningful error -- now try 500
      dumpn("*** error in handling for error code " + errorX00 + ", falling " +
            "back to 500...");

      try
      {
        if (500 in this._overrideErrors)
          this._overrideErrors[500](metadata, response);
        else
          this._defaultErrors[500](metadata, response);
      }
      catch (e2)
      {
        dumpn("*** multiple errors in default error handlers!");
        dumpn("*** e == " + e + ", e2 == " + e2);
        throw e2;
      }
    }
  },

  /**
   * Called when all processing necessary for the current request has completed
   * and response headers and data have been determined.  This method takes
   * those headers and data, sends them to the HTTP client, and halts further
   * processing.  It will also send a quit message to the server if necessary.
   *
   * @param response
   *   a Response object representing the desired response
   * @param outStream
   *   a stream to which the response should be written
   * @note
   *   after completion, response must be considered "dead", and none of its
   *   methods or properties may be accessed
   */
  _end:  function(response, outStream)
  {
    try
    {
      // post-processing
      response.setHeader("Connection", "close", false);
      response.setHeader("Server", "MozJSHTTP", false);
      response.setHeader("Date", toDateString(Date.now()), false);

      var bodyStream = response.bodyInputStream
                               .QueryInterface(Ci.nsIInputStream);

      // XXX suckage time!
      //
      // If the body of the response has had no data written to it (or has never
      // been accessed -- same deal internally since we'll create one if we have
      // to access bodyInputStream but have neither an input stream nor an
      // output stream), the in-tree implementation of nsIPipe is such that
      // when we try to close the pipe's output stream with no data in it, this
      // is interpreted as an error and closing the output stream also closes
      // the input stream.  .available then throws, so we catch and deal as best
      // as we can.
      //
      // Unfortunately, the easy alternative (substitute a storage stream for a
      // pipe) also doesn't work.  There's no problem writing zero bytes to the
      // output end of the stream, but then attempting to get an input stream to
      // read fails because the seek position must be strictly less than the
      // buffer size.
      //
      // Much as I'd like the zero-byte body to be a mostly-unimportant problem,
      // there are some HTTP responses such as 304 Not Modified which MUST have
      // zero-byte bodies, so this *is* a necessary hack.
      try
      {
        var available = bodyStream.available();
      }
      catch (e)
      {
        available = 0;
      }

      response.setHeader("Content-Length", available.toString(), false);


      // construct and send response

      // request-line
      var preamble = "HTTP/" + response.httpVersion + " " +
                     response.httpCode + " " +
                     response.httpDescription + "\r\n";

      // headers
      var head = response.headers;
      var headEnum = head.enumerator;
      while (headEnum.hasMoreElements())
      {
        var fieldName = headEnum.getNext()
                                .QueryInterface(Ci.nsISupportsString)
                                .data;
        preamble += fieldName + ": " + head.getHeader(fieldName) + "\r\n";
      }

      // end request-line/headers
      preamble += "\r\n";
      outStream.write(preamble, preamble.length);


      // In certain situations, it's possible for us to have a race between
      // the copy observer's onStopRequest and the listener for a channel
      // opened to this server.  Since we include a Content-Length header with
      // every response, if the channel snarfs up all the data we promise,
      // calls onStopRequest on the listener (and the server is shut down
      // by that listener, causing the script to finish executing), and then
      // tries to call onStopRequest on the copyObserver, we'll call into a
      // scope with no Components and cause assertions *and* fail to close the
      // connection properly.  To combat this, during server shutdown we delay
      // full shutdown until any pending requests are fully copied using this
      // property on the server handler.  We increment before (possibly)
      // starting the copy observer and decrement when the copy completes,
      // ensuring that all copies complete before the server fully shuts down.
      //
      // We do this for every request primarily to simplify maintenance of this
      // property (and also because it's less fragile when we can remove the
      // zero-sized body hack used above).
      this._pendingRequests++;

      // If we have a body, send it -- if we don't, then don't bother with a
      // heavyweight async copy which doesn't need to happen and just do
      // response post-processing (usually handled by the copy observer)
      // directly
      if (available != 0)
      {
        // local references for use in the copy observer
        var server = this._server;
        var handler = this;

        /**
         * Observer of the copying of data from the body stream generated by a
         * request handler to the output stream for the server socket.  It
         * handles all post-request-writing cleanup details, such as closing
         * open streams and shutting down the server in case of errors.
         */
        var copyObserver =
          {
            onStartRequest: function(request, context) { /* don't care */ },

            /**
             * Called when the async stream copy completes.  This is place where
             * final cleanup should occur, including stream closures and
             * response destruction.  Note that errors which are detected here
             * should still shut down the server, for safety.
             */
            onStopRequest: function(request, context, statusCode)
            {
              // if something bad happened during the copy, be paranoid
              if (!Components.isSuccessCode(statusCode))
                server._requestQuit();

              // we're completely finished with this response
              response.destroy();

              server._endConnection(handler, outStream);
            },

            QueryInterface: function(aIID)
            {
              if (aIID.equals(Ci.nsIRequestObserver) ||
                  aIID.equals(Ci.nsISupports))
                return this;

              throw Cr.NS_ERROR_NO_INTERFACE;
            }
          };


        // body -- written async, because pipes deadlock if we do
        // |outStream.writeFrom(bodyStream, bodyStream.available());|
        var copier = Cc["@mozilla.org/network/async-stream-copier;1"]
                       .createInstance(Ci.nsIAsyncStreamCopier);
        copier.init(bodyStream, outStream, null, true, true, 8192);
        copier.asyncCopy(copyObserver, null);
      }
      else
      {
        // finished with the response -- destroy
        response.destroy();
        this._server._endConnection(this, outStream);
      }
    }
    catch (e)
    {
      // something bad happened -- throw and make calling code deal with the
      // response given to us
      throw e;
    }
  },

  // FIELDS

  /**
   * This object contains the default handlers for the various HTTP error codes.
   */
  _defaultErrors:
  {
    400: function(metadata, response)
    {
      // none of the data in metadata is reliable, so hard-code everything here
      response.setStatusLine("1.1", 400, "Bad Request");
      response.setHeader("Content-Type", "text/plain", false);

      var body = "Bad request\n";
      response.bodyOutputStream.write(body, body.length);
    },
    403: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 403, "Forbidden");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>403 Forbidden</title></head>\
                    <body>\
                      <h1>403 Forbidden</h1>\
                    </body>\
                  </html>";
      response.bodyOutputStream.write(body, body.length);
    },
    404: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 404, "Not Found");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>404 Not Found</title></head>\
                    <body>\
                      <h1>404 Not Found</h1>\
                      <p>\
                        <span style='font-family: monospace;'>" +
                          htmlEscape(metadata.path) +
                       "</span> was not found.\
                      </p>\
                    </body>\
                  </html>";
      response.bodyOutputStream.write(body, body.length);
    },
    500: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion,
                             500,
                             "Internal Server Error");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>500 Internal Server Error</title></head>\
                    <body>\
                      <h1>500 Internal Server Error</h1>\
                      <p>Something's broken in this server and\
                        needs to be fixed.</p>\
                    </body>\
                  </html>";
      response.bodyOutputStream.write(body, body.length);
    },
    501: function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 501, "Not Implemented");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>501 Not Implemented</title></head>\
                    <body>\
                      <h1>501 Not Implemented</h1>\
                      <p>This server is not (yet) Apache.</p>\
                    </body>\
                  </html>";
      response.bodyOutputStream.write(body, body.length);
    },
    505: function(metadata, response)
    {
      response.setStatusLine("1.1", 505, "HTTP Version Not Supported");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>505 HTTP Version Not Supported</title></head>\
                    <body>\
                      <h1>505 HTTP Version Not Supported</h1>\
                      <p>This server only supports HTTP/1.0 and HTTP/1.1\
                        connections.</p>\
                    </body>\
                  </html>";
      response.bodyOutputStream.write(body, body.length);
    }
  },

  /**
   * Contains handlers for the default set of URIs contained in this server.
   */
  _defaultPaths:
  {
    "/": function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html", false);

      var body = "<html>\
                    <head><title>MozJSHTTP</title></head>\
                    <body>\
                      <h1>MozJSHTTP</h1>\
                      <p>If you're seeing this page, MozJSHTTP is up and\
                        serving requests!  Now set a base path and serve some\
                        files!</p>\
                    </body>\
                  </html>";

      response.bodyOutputStream.write(body, body.length);
    },

    "/trace": function(metadata, response)
    {
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/plain", false);

      var body = "Request (semantically equivalent, slightly reformatted):\n\n";
      body += metadata.method + " " + metadata.path;

      if (metadata.queryString)
        body +=  "?" + metadata.queryString;
        
      body += " HTTP/" + metadata.httpVersion + "\n";

      var headEnum = metadata.headers;
      while (headEnum.hasMoreElements())
      {
        var fieldName = headEnum.getNext()
                                .QueryInterface(Ci.nsISupportsString)
                                .data;
        body += fieldName + ": " + metadata.getHeader(fieldName) + "\n";
      }

      response.bodyOutputStream.write(body, body.length);
    }
  }
};


/**
 * Maps absolute paths to files on the local file system (as nsILocalFiles).
 */
function FileMap()
{
  /** Hash which will map paths to nsILocalFiles. */
  this._map = {};
}
FileMap.prototype =
{
  // PUBLIC API

  /**
   * Maps key to a clone of the nsILocalFile value if value is non-null;
   * otherwise, removes any extant mapping for key.
   */
  put: function(key, value)
  {
    if (value)
      this._map[key] = value.clone();
    else
      delete this._map[key];
  },

  /**
   * Returns a clone of the nsILocalFile mapped to key, or null if no such
   * mapping exists.
   */
  get: function(key)
  {
    var val = this._map[key];
    return val ? val.clone() : null;
  }
};


// Response CONSTANTS

// token       = *<any CHAR except CTLs or separators>
// CHAR        = <any US-ASCII character (0-127)>
// CTL         = <any US-ASCII control character (0-31) and DEL (127)>
// separators  = "(" | ")" | "<" | ">" | "@"
//             | "," | ";" | ":" | "\" | <">
//             | "/" | "[" | "]" | "?" | "="
//             | "{" | "}" | SP  | HT
const IS_TOKEN_ARRAY =
  [0, 0, 0, 0, 0, 0, 0, 0, //   0
   0, 0, 0, 0, 0, 0, 0, 0, //   8
   0, 0, 0, 0, 0, 0, 0, 0, //  16
   0, 0, 0, 0, 0, 0, 0, 0, //  24

   0, 1, 0, 1, 1, 1, 1, 1, //  32
   0, 0, 1, 1, 0, 1, 1, 0, //  40
   1, 1, 1, 1, 1, 1, 1, 1, //  48
   1, 1, 0, 0, 0, 0, 0, 0, //  56

   0, 1, 1, 1, 1, 1, 1, 1, //  64
   1, 1, 1, 1, 1, 1, 1, 1, //  72
   1, 1, 1, 1, 1, 1, 1, 1, //  80
   1, 1, 1, 0, 0, 0, 1, 1, //  88

   1, 1, 1, 1, 1, 1, 1, 1, //  96
   1, 1, 1, 1, 1, 1, 1, 1, // 104
   1, 1, 1, 1, 1, 1, 1, 1, // 112
   1, 1, 1, 0, 1, 0, 1];   // 120


/**
 * Determines whether the given character code is a CTL.
 *
 * @param code
 *   the character code
 * @returns
 *   true if code is a CTL, false otherwise
 */
function isCTL(code)
{
  return (code >= 0 && code <= 31) || (code == 127);
}

/**
 * Represents a response to an HTTP request, encapsulating all details of that
 * response.  This includes all headers, the HTTP version, status code and
 * explanation, and the entity itself.
 */
function Response()
{
  // delegate initialization behavior to .recycle(), for code-sharing;
  // see there for field descriptions as well
  this.recycle();
}
Response.prototype =
{
  // PUBLIC CONSTRUCTION API

  //
  // see nsIHttpResponse.bodyOutputStream
  //
  get bodyOutputStream()
  {
    this._ensureAlive();

    if (!this._bodyOutputStream && !this._outputProcessed)
    {
      var pipe = Cc["@mozilla.org/pipe;1"]
                   .createInstance(Ci.nsIPipe);
      const PR_UINT32_MAX = Math.pow(2, 32) - 1;
      pipe.init(false, false, 0, PR_UINT32_MAX, null);
      this._bodyOutputStream = pipe.outputStream;
      this._bodyInputStream = pipe.inputStream;
    }

    return this._bodyOutputStream;
  },

  //
  // see nsIHttpResponse.write
  //
  write: function(data)
  {
    var dataAsString = String(data);
    this.bodyOutputStream.write(dataAsString, dataAsString.length);
  },

  //
  // see nsIHttpResponse.setStatusLine
  //
  setStatusLine: function(httpVersion, code, description)
  {
    this._ensureAlive();

    if (!(code >= 0 && code < 1000))
      throw Cr.NS_ERROR_INVALID_ARG;

    try
    {
      var httpVer;
      // avoid version construction for the most common cases
      if (!httpVersion || httpVersion == "1.1")
        httpVer = nsHttpVersion.HTTP_1_1;
      else if (httpVersion == "1.0")
        httpVer = nsHttpVersion.HTTP_1_0;
      else
        httpVer = new nsHttpVersion(httpVersion);
    }
    catch (e)
    {
      throw Cr.NS_ERROR_INVALID_ARG;
    }

    // Reason-Phrase = *<TEXT, excluding CR, LF>
    // TEXT          = <any OCTET except CTLs, but including LWS>
    //
    // XXX this ends up disallowing octets which aren't Unicode, I think -- not
    //     much to do if description is IDL'd as string
    if (!description)
      description = "";
    for (var i = 0; i < description.length; i++)
      if (isCTL(description.charCodeAt(i)) && description.charAt(i) != "\t")
        throw Cr.NS_ERROR_INVALID_ARG;

    // set the values only after validation to preserve atomicity
    this._httpDescription = description;
    this._httpCode = code;
    this._httpVersion = httpVer;
  },

  //
  // see nsIHttpResponse.setHeader
  //
  setHeader: function(name, value, merge)
  {
    this._ensureAlive();

    this._headers.setHeader(name, value, merge);
  },

  // POST-CONSTRUCTION API (not exposed externally)

  /**
   * The HTTP version number of this, as a string (e.g. "1.1").
   */
  get httpVersion()
  {
    this._ensureAlive();

    return this._httpVersion.toString();
  },

  /**
   * The HTTP status code of this response, as a string of three characters per
   * RFC 2616.
   */
  get httpCode()
  {
    this._ensureAlive();

    var codeString = (this._httpCode < 10 ? "0" : "") +
                     (this._httpCode < 100 ? "0" : "") +
                     this._httpCode;
    return codeString;
  },

  /**
   * The description of the HTTP status code of this response, or "" if none is
   * set.
   */
  get httpDescription()
  {
    this._ensureAlive();

    return this._httpDescription;
  },

  /**
   * The headers in this response, as an nsHttpHeaders object.
   */
  get headers()
  {
    this._ensureAlive();

    return this._headers;
  },

  //
  // see nsHttpHeaders.getHeader
  //
  getHeader: function(name)
  {
    this._ensureAlive();

    return this._headers.getHeader(name);
  },

  /**
   * A stream containing the data stored in the body of this response, which is
   * the data written to this.bodyOutputStream.  Accessing this property will
   * prevent further writes to bodyOutputStream and will remove that property
   * from this, so the only time this should be accessed should be after this
   * Response has been fully processed by a request handler.
   */
  get bodyInputStream()
  {
    this._ensureAlive();

    if (!this._outputProcessed)
    {
      // if nothing was ever written to bodyOutputStream, we never created a
      // pipe -- do so now by writing the empty string to this.bodyOutputStream
      if (!this._bodyOutputStream)
        this.bodyOutputStream.write("", 0);

      this._outputProcessed = true;
    }
    if (this._bodyOutputStream)
    {
      this._bodyOutputStream.close(); // flushes buffered data
      this._bodyOutputStream = null;  // don't try closing again
    }
    return this._bodyInputStream;
  },

  /**
   * Resets this response to its original state, destroying any currently-held
   * resources in the process.  Use this method to invalidate an existing
   * response and reuse it later, such as when an arbitrary handler has
   * failed and may have altered the visible state of this (such as by
   * setting headers).
   *
   * This method may be called on Responses which have been .destroy()ed.
   */
  recycle: function()
  {
    if (this._bodyOutputStream)
    {
      this._bodyOutputStream.close();
      this._bodyOutputStream = null;
    }
    if (this._bodyInputStream)
    {
      this._bodyInputStream.close();
      this._bodyInputStream = null;
    }

    /**
     * The HTTP version of this response; defaults to 1.1 if not set by the
     * handler.
     */
    this._httpVersion = nsHttpVersion.HTTP_1_1;

    /**
     * The HTTP code of this response; defaults to 200.
     */
    this._httpCode = 200;

    /**
     * The description of the HTTP code in this response; defaults to "OK".
     */
    this._httpDescription = "OK";

    /**
     * An nsIHttpHeaders object in which the headers in this response should be
     * stored.
     */
    this._headers = new nsHttpHeaders();

    /**
     * Set to true when this has its .destroy() method called; further actions on
     * this will then fail.
     */
    this._destroyed = false;

    /**
     * Flipped when this.bodyOutputStream is closed; prevents the body from being
     * reopened after it has data written to it and has been closed.
     */
    this._outputProcessed = false;
  },

  /**
   * Destroys all resources held by this.  After this method is called, no other
   * method or property on this must be accessed (except .recycle, which may be
   * used to reuse this Response).  Although in many situations resources may be
   * automagically cleaned up, it is highly recommended that this method be
   * called whenever a Response is no longer used, both as a precaution and
   * because this implementation may not always do so.
   *
   * This method is idempotent.
   */
  destroy: function()
  {
    if (this._destroyed)
      return;

    if (this._bodyOutputStream)
    {
      this._bodyOutputStream.close();
      this._bodyOutputStream = null;
    }
    if (this._bodyInputStream)
    {
      this._bodyInputStream.close();
      this._bodyInputStream = null;
    }

    this._destroyed = true;
  },

  // PRIVATE IMPLEMENTATION

  /** Ensures that this hasn't had its .destroy() method called. */
  _ensureAlive: function()
  {
    if (this._destroyed)
      throw Cr.NS_ERROR_FAILURE;
  }
};


/**
 * A container for utility functions used with HTTP headers.
 */
const headerUtils =
{
  /**
   * Normalizes fieldName (by converting it to lowercase) and ensures it is a
   * valid header field name (although not necessarily one specified in RFC
   * 2616).
   *
   * @throws NS_ERROR_INVALID_ARG
   *   if fieldName does not match the field-name production in RFC 2616
   * @returns
   *   fieldName converted to lowercase if it is a valid header, for characters
   *   where case conversion is possible
   */
  normalizeFieldName: function(fieldName)
  {
     for (var i = 0, sz = fieldName.length; i < sz; i++)
     {
       if (!IS_TOKEN_ARRAY[fieldName.charCodeAt(i)])
       {
         dumpn(fieldName + " is not a valid header field name!");
         throw Cr.NS_ERROR_INVALID_ARG;
       }
     }

     return fieldName.toLowerCase();
  },

  /**
   * Ensures that fieldValue is a valid header field value (although not
   * necessarily as specified in RFC 2616 if the corresponding field name is
   * part of the HTTP protocol), normalizes the value if it is, and
   * returns the normalized value.
   *
   * @param fieldValue
   *   a value to be normalized as an HTTP header field value
   * @throws NS_ERROR_INVALID_ARG
   *   if fieldValue does not match the field-value production in RFC 2616
   * @returns
   *   fieldValue as a normalized HTTP header field value
   */
  normalizeFieldValue: function(fieldValue)
  {
    // field-value    = *( field-content | LWS )
    // field-content  = <the OCTETs making up the field-value
    //                  and consisting of either *TEXT or combinations
    //                  of token, separators, and quoted-string>
    // TEXT           = <any OCTET except CTLs,
    //                  but including LWS>
    // LWS            = [CRLF] 1*( SP | HT )
    //
    // quoted-string  = ( <"> *(qdtext | quoted-pair ) <"> )
    // qdtext         = <any TEXT except <">>
    // quoted-pair    = "\" CHAR
    // CHAR           = <any US-ASCII character (octets 0 - 127)>

    // Any LWS that occurs between field-content MAY be replaced with a single
    // SP before interpreting the field value or forwarding the message
    // downstream (section 4.2); we replace 1*LWS with a single SP
    var val = fieldValue.replace(/(?:(?:\r\n)?[ \t]+)+/g, " ");

    // remove leading/trailing LWS (which has been converted to SP)
    val = val.replace(/^ +/, "").replace(/ +$/, "");

    // that should have taken care of all CTLs, so val should contain no CTLs
    for (var i = 0, len = val.length; i < len; i++)
      if (isCTL(val.charCodeAt(i)))
        throw Cr.NS_ERROR_INVALID_ARG;

    // XXX disallows quoted-pair where CHAR is a CTL -- will not invalidly
    //     normalize, however, so this can be construed as a tightening of the
    //     spec and not entirely as a bug
    return val;
  }
};



/**
 * Converts the given string into a string which is safe for use in an HTML
 * context.
 *
 * @param str
 *   the string to make HTML-safe
 * @returns
 *   an HTML-safe version of str
 */
function htmlEscape(str)
{
  // this is naive, but it'll work
  var s = "";
  for (var i = 0; i < str.length; i++)
    s += "&#" + str.charCodeAt(i) + ";";
  return s;
}


/**
 * Constructs an object representing an HTTP version (see section 3.1).
 *
 * @param versionString
 *   a string of the form "#.#", where # is an non-negative decimal integer with
 *   or without leading zeros
 * @throws
 *   if versionString does not specify a valid HTTP version number
 */
function nsHttpVersion(versionString)
{
  var matches = /^(\d+)\.(\d+)$/.exec(versionString);
  if (!matches)
    throw "Not a valid HTTP version!";

  this.major = parseInt(matches[1], 10);
  this.minor = parseInt(matches[2], 10);
  if (isNaN(this.major) || isNaN(this.minor) ||
      this.major < 0    || this.minor < 0)
    throw "Not a valid HTTP version!";
}
nsHttpVersion.prototype =
{
  /**
   * The major version number of this, as a number
   */
  major: undefined,

  /**
   * The minor version number of this, as a number.
   */
  minor: undefined,

  /**
   * Returns the standard string representation of the HTTP version represented
   * by this (e.g., "1.1").
   */
  toString: function ()
  {
    return this.major + "." + this.minor;
  },

  /**
   * Returns true if this represents the same HTTP version as otherVersion,
   * false otherwise.
   */
  equals: function (otherVersion)
  {
    return this.major == otherVersion.major &&
           this.minor == otherVersion.minor;
  }
};

nsHttpVersion.HTTP_1_0 = new nsHttpVersion("1.0");
nsHttpVersion.HTTP_1_1 = new nsHttpVersion("1.1");


/**
 * An object which stores HTTP headers for a request or response.
 *
 * Note that since headers are case-insensitive, this object converts headers to
 * lowercase before storing them.  This allows the getHeader and hasHeader
 * methods to work correctly for any case of a header, but it means that the
 * values returned by .enumerator may not be equal case-sensitively to the
 * values passed to setHeader when adding headers to this.
 */
function nsHttpHeaders()
{
  /**
   * A hash of headers, with header field names as the keys and header field
   * values as the values.  Header field names are case-insensitive, but upon
   * insertion here they are converted to lowercase.  Header field values are
   * normalized upon insertion to contain no leading or trailing whitespace.
   *
   * Note also that per RFC 2616, section 4.2, two headers with the same name in
   * a message may be treated as one header with the same field name and a field
   * value consisting of the separate field values joined together with a "," in
   * their original order.  This hash stores multiple headers with the same name
   * in this manner.
   */
  this._headers = {};
}
nsHttpHeaders.prototype =
{
  /**
   * Sets the header represented by name and value in this.
   *
   * @param name
   *   the header name
   * @param value
   *   the header value
   * @throws NS_ERROR_INVALID_ARG
   *   if name or value is not a valid header component
   */
  setHeader: function(fieldName, fieldValue, merge)
  {
    var name = headerUtils.normalizeFieldName(fieldName);
    var value = headerUtils.normalizeFieldValue(fieldValue);

    if (merge && name in this._headers)
      this._headers[name] = this._headers[name] + "," + value;
    else
      this._headers[name] = value;
  },

  /**
   * Returns the value for the header specified by this.
   *
   * @returns
   *   the field value for the given header, possibly with non-semantic changes
   *   (i.e., leading/trailing whitespace stripped, whitespace runs replaced
   *   with spaces, etc.) at the option of the implementation
   * @throws NS_ERROR_INVALID_ARG
   *   if fieldName does not constitute a valid header field name
   * @throws NS_ERROR_NOT_AVAILABLE
   *   if the given header does not exist in this
   */
  getHeader: function(fieldName)
  {
    var name = headerUtils.normalizeFieldName(fieldName);

    if (name in this._headers)
      return this._headers[name];
    else
      throw Cr.NS_ERROR_NOT_AVAILABLE;
  },

  /**
   * Returns true if a header with the given field name exists in this, false
   * otherwise.
   *
   * @param fieldName
   *   the field name whose existence is to be determined in this
   * @throws NS_ERROR_INVALID_ARG
   *   if fieldName does not constitute a valid header field name
   */
  hasHeader: function(fieldName)
  {
    var name = headerUtils.normalizeFieldName(fieldName);
    return (name in this._headers);
  },

  /**
   * Returns a new enumerator over the field names of the headers in this, as
   * nsISupportsStrings.  The names returned will be in lowercase, regardless of
   * how they were input using setHeader (header names are case-insensitive per
   * RFC 2616).
   */
  get enumerator()
  {
    var headers = [];
    for (var i in this._headers)
    {
      var supports = Cc["@mozilla.org/supports-string;1"]
                       .createInstance(Ci.nsISupportsString);
      supports.data = i;
      headers.push(supports);
    }

    return new nsSimpleEnumerator(headers);
  }
};


/**
 * Constructs an nsISimpleEnumerator for the given array of items.
 *
 * @param items
 *   the array of items, which must all implement nsISupports
 */
function nsSimpleEnumerator(items)
{
  this._items = items;
  this._nextIndex = 0;
}
nsSimpleEnumerator.prototype =
{
  hasMoreElements: function()
  {
    return this._nextIndex < this._items.length;
  },
  getNext: function()
  {
    if (!this.hasMoreElements())
      throw Cr.NS_ERROR_NOT_AVAILABLE;

    return this._items[this._nextIndex++];
  },
  QueryInterface: function(aIID)
  {
    if (Ci.nsISimpleEnumerator.equals(aIID) ||
        Ci.nsISupports.equals(aIID))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};


/**
 * Parses a server request into a set of metadata, so far as is possible.  Any
 * detected errors will result in this.errorCode being set to an HTTP error code
 * value.  Users MUST check this value after creation and any external
 * initialization of RequestMetadata objects to ensure that errors are handled
 * correctly.
 *
 * @param port
 *   the port on which the server receiving this request runs
 */
function RequestMetadata(port)
{
  this._method = "";
  this._path = "";
  this._queryString = "";
  this._host = "";
  this._port = port;

  /**
   * The headers in this request.
   */
  this._headers = new nsHttpHeaders();

  /**
   * For the addition of ad-hoc properties and new functionality
   * without having to tweak nsIHttpRequestMetadata every time.
   */
  this._bag = Cc["@mozilla.org/hash-property-bag;1"]
                .createInstance(Ci.nsIWritablePropertyBag2);

  /**
   * The numeric HTTP error, if any, associated with this request.  This value
   * may be set by the constructor but is usually only set by the handler after
   * this has been constructed.  After this has been initialized, this value
   * MUST be checked for errors.
   */
  this.errorCode = 0;
}
RequestMetadata.prototype =
{
  // SERVER METADATA

  //
  // see nsIHttpRequestMetadata.host
  //
  get host()
  {
    return this._host;
  },

  //
  // see nsIHttpRequestMetadata.port
  //
  get port()
  {
    return this._port;
  },

  // REQUEST LINE

  //
  // see nsIHttpRequestMetadata.method
  //
  get method()
  {
    return this._method;
  },

  //
  // see nsIHttpRequestMetadata.httpVersion
  //
  get httpVersion()
  {
    return this._httpVersion.toString();
  },

  //
  // see nsIHttpRequestMetadata.path
  //
  get path()
  {
    return this._path;
  },

  //
  // see nsIHttpRequestMetadata.queryString
  //
  get queryString()
  {
    return this._queryString;
  },

  // HEADERS

  //
  // see nsIHttpRequestMetadata.getHeader
  //
  getHeader: function(name)
  {
    return this._headers.getHeader(name);
  },

  //
  // see nsIHttpRequestMetadata.hasHeader
  //
  hasHeader: function(name)
  {
    return this._headers.hasHeader(name);
  },

  //
  // see nsIHttpRequestMetadata.headers
  //
  get headers()
  {
    return this._headers.enumerator;
  },

  //
  // see nsIPropertyBag.enumerator
  //
  get enumerator()
  {
    return this._bag.enumerator;
  },

  //
  // see nsIPropertyBag.getProperty
  //
  getProperty: function(name) 
  {
    return this._bag.getProperty(name);
  },

  // ENTITY

  /**
   * An input stream which contains the body of this request.
   */
  get bodyStream()
  {
    // we want this once we do real request processing -- expose externally when
    // we do this
    return null;
  },

  // PUBLIC CONSTRUCTION API

  /**
   * The HTTP error code which should be the result of this request.  It must be
   * checked whenever other API documentation says it should be checked.
   */
  errorCode: 0,

  // INITIALIZATION
  init: function(input)
  {
    // XXX this is incredibly un-RFC2616 in every possible way:
    //
    // - accepts non-CRLF line endings
    // - no real testing for non-US-ASCII text and throwing in that case
    // - handles POSTs by displaying the URL and throwing away the request
    //   entity
    // - need to support RFC 2047-encoded non-US-ASCII characters
    // - support absolute URLs (requires telling the server its hostname, beyond
    //   just localhost:port and 127.0.0.1:port)
    // - etc.

    // read the input line by line; the first line either tells us the requested
    // path or is empty, in which case the second line contains the path
    var is = Cc["@mozilla.org/intl/converter-input-stream;1"]
               .createInstance(Ci.nsIConverterInputStream);
    is.init(input, "ISO-8859-1", 1024, 0xFFFD);

    var lis = is.QueryInterface(Ci.nsIUnicharLineInputStream);


    this._parseRequestLine(lis);
    if (this.errorCode)
      return;

    this._parseHeaders(lis);
    if (this.errorCode)
      return;

    // XXX need to put body transmitted with this request into an input stream!

    // 19.6.1.1 -- servers MUST report 400 to HTTP/1.1 requests w/o Host header
    if (!this._headers.hasHeader("Host") &&
        this._httpVersion.equals(nsHttpVersion.HTTP_1_1))
    {
      this.errorCode = 400;
      return;
    }

    // XXX set this based on Host or the request URI?
    this._host = "localhost";
  },

  // PRIVATE API

  /**
   * Parses the request line for the HTTP request in the given input stream.  On
   * completion this.errorCode must be checked to determine whether any errors
   * occurred during header parsing.
   *
   * @param lis
   *   an nsIUnicharLineInputStream from which to parse HTTP headers
   */
  _parseRequestLine: function(lis)
  {
    // servers SHOULD ignore any empty line(s) received where a Request-Line
    // is expected (section 4.1)
    var line = {};
    while (lis.readLine(line) && line.value == "")
      dumpn("*** ignoring beginning blank line...");

    // clients and servers SHOULD accept any amount of SP or HT characters
    // between fields, even though only a single SP is required (section 19.3)
    var request = line.value.split(/[ \t]+/);
    if (!request || request.length != 3)
    {
      this.errorCode = 400;
      return;
    }

    this._method = request[0];

    // check the HTTP version
    var ver = request[2];
    var match = ver.match(/^HTTP\/(\d+\.\d+)$/);
    if (!match)
    {
      this.errorCode = 400;
      return;
    }

    // reject unrecognized methods
    if (request[0] != "GET" && request[0] != "POST")
    {
      this.errorCode = 501;
      return;
    }

    // determine HTTP version
    try
    {
      this._httpVersion = new nsHttpVersion(match[1]);
      if (!this._httpVersion.equals(nsHttpVersion.HTTP_1_0) &&
          !this._httpVersion.equals(nsHttpVersion.HTTP_1_1))
        throw "unsupported HTTP version";
    }
    catch (e)
    {
      // we support HTTP/1.0 and HTTP/1.1 only
      this.errorCode = 501;
      return;
    }

    var fullPath = request[1];

    // XXX we don't support absolute URIs yet -- a MUST for HTTP/1.1
    if (fullPath.charAt(0) != "/")
    {
      this.errorCode = 400;
      return;
    }

    var splitter = fullPath.indexOf("?");
    if (splitter < 0)
    {
      // _queryString already set in ctor
      this._path = fullPath;
    }
    else
    {
      this._path = fullPath.substring(0, splitter);
      this._queryString = fullPath.substring(splitter + 1);
    }
  },

  /**
   * Parses all available HTTP headers from the given input stream.  On
   * completion, this.errorCode must be checked to determine whether any errors
   * occurred during header parsing.
   *
   * @param lis
   *   the nsIUnicharLineInputStream from which to parse HTTP headers
   */
  _parseHeaders: function(lis)
  {
    var headers = this._headers;
    var lastName, lastVal;

    var line = {};
    while (true)
    {
      NS_ASSERT((lastVal === undefined && lastName === undefined) ||
                (lastVal !== undefined && lastName !== undefined),
                lastName === undefined ?
                  "lastVal without lastName?  lastVal: '" + lastVal + "'" :
                  "lastName without lastVal?  lastName: '" + lastName + "'");

      lis.readLine(line);
      var lineText = line.value;
      var firstChar = lineText.charAt(0);

      // blank line means end of headers
      if (lineText == "")
      {
        // we're finished with the previous header
        if (lastName)
        {
          try
          {
            headers.setHeader(lastName, lastVal, true);
          }
          catch (e)
          {
            dumpn("*** e == " + e);
            this.errorCode = 400;
            return;
          }
        }
        else
        {
          // no headers in request -- valid for HTTP/1.0 requests
        }

        // either way, we're done processing headers
        break;
      }
      else if (firstChar == " " || firstChar == "\t")
      {
        // multi-line header if we've seen a header line
        if (!lastName)
        {
          // we don't have a header to continue!
          this.errorCode = 400;
          return;
        }

        // append this line's text to the value; starts with SP/HT, so no need
        // for separating whitespace
        lastVal += lineText;
      }
      else
      {
        // we have a new header, so set the old one (if one existed)
        if (lastName)
        {
          try
          {
            headers.setHeader(lastName, lastVal, true);
          }
          catch (e)
          {
            dumpn("*** e == " + e);
            this.errorCode = 400;
            return;
          }
        }

        var colon = lineText.indexOf(":"); // first colon must be splitter
        if (colon < 1)
        {
          // no colon or missing header field-name
          this.errorCode = 400;
          return;
        }

        // set header name, value (to be set in the next loop, usually)
        lastName = lineText.substring(0, colon);
        lastVal = lineText.substring(colon + 1);
      } // empty, continuation, start of header
    } // while (true)
  }
};


// XPCOM trappings

/**
 * Creates a factory for instances of an object created using the passed-in
 * constructor.
 */
function makeFactory(ctor)
{
  function ci(outer, iid)
  {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return (new ctor()).QueryInterface(iid);
  } 

  return {
           createInstance: ci,
           lockFactory: function(lock) { },
           QueryInterface: function(aIID)
           {
             if (Ci.nsIFactory.equals(aIID) ||
                 Ci.nsISupports.equals(aIID))
               return this;
             throw Cr.NS_ERROR_NO_INTERFACE;
           }
         };
}

/** The XPCOM module containing the HTTP server. */
const module =
{
  // nsISupports
  QueryInterface: function(aIID)
  {
    if (Ci.nsIModule.equals(aIID) ||
        Ci.nsISupports.equals(aIID))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // nsIModule
  registerSelf: function(compMgr, fileSpec, location, type)
  {
    compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
    
    for (var key in this._objects)
    {
      var obj = this._objects[key];
      compMgr.registerFactoryLocation(obj.CID, obj.className, obj.contractID,
                                               fileSpec, location, type);
    }
  },
  unregisterSelf: function (compMgr, location, type)
  {
    compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);

    for (var key in this._objects)
    {
      var obj = this._objects[key];
      compMgr.unregisterFactoryLocation(obj.CID, location);
    }
  },
  getClassObject: function(compMgr, cid, iid)
  {
    if (!iid.equals(Ci.nsIFactory))
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;

    for (var key in this._objects)
    {
      if (cid.equals(this._objects[key].CID))
        return this._objects[key].factory;
    }
    
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  canUnload: function(compMgr)
  {
    return true;
  },

  // private implementation
  _objects:
  {
    server:
    {
      CID:         Components.ID("{54ef6f81-30af-4b1d-ac55-8ba811293e41}"),
      contractID:  "@mozilla.org/server/jshttp;1",
      className:   "MozJSHTTP server",
      factory:     makeFactory(nsHttpServer)
    }
  }
};


/**
 * NSGetModule, so this code can be used as a JS component.  Whether multiple
 * servers can run at once is currently questionable, but the code certainly
 * isn't very far from supporting it.
 */
function NSGetModule(compMgr, fileSpec)
{
  return module;
}


/**
 * Creates a new HTTP server listening for loopback traffic on the given port,
 * starts it, and runs the server until the server processes a shutdown request,
 * spinning an event loop so that events posted by the server's socket are
 * processed.
 *
 * This method is primarily intended for use in running this script from within
 * xpcshell and running a functional HTTP server without having to deal with
 * non-essential details.
 *
 * Note that running multiple servers using variants of this method probably
 * doesn't work, simply due to how the internal event loop is spun and stopped.
 *
 * @note
 *   This method only works with Mozilla 1.9 (i.e., Firefox 3 or trunk code);
 *   you should use this server as a component in Mozilla 1.8.
 * @param port
 *   the port on which the server will run, or -1 if there exists no preference
 *   for a specific port; note that attempting to use some values for this
 *   parameter (particularly those below 1024) may cause this method to throw or
 *   may result in the server being prematurely shut down
 * @param basePath
 *   a local directory from which requests will be served (i.e., if this is
 *   "/home/jwalden/" then a request to /index.html will load
 *   /home/jwalden/index.html); if this is omitted, only the default URLs in
 *   this server implementation will be functional
 */
function server(port, basePath)
{
  if (basePath)
  {
    var lp = Cc["@mozilla.org/file/local;1"]
               .createInstance(Ci.nsILocalFile);
    lp.initWithPath(basePath);
  }

  var srv = new nsHttpServer();
  if (lp)
    srv.registerDirectory("/", lp);
  srv.start(port);

  var thread = Cc["@mozilla.org/thread-manager;1"]
                 .getService()
                 .currentThread;
  while (!srv.isStopped())
    thread.processNextEvent(true);

  // get rid of any pending requests
  while (thread.hasPendingEvents())
    thread.processNextEvent(true);
}
