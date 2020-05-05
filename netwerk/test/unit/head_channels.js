/**
 * Read count bytes from stream and return as a String object
 */

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */

function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  wrapper.setInputStream(stream);
  /* JS methods can be called with a maximum of 65535 arguments, and input
     streams don't have to return all the data they make .available() when
     asked to .read() that number of bytes. */
  var data = [];
  while (count > 0) {
    var bytes = wrapper.readByteArray(Math.min(65535, count));
    data.push(String.fromCharCode.apply(null, bytes));
    count -= bytes.length;
    if (bytes.length == 0) {
      do_throw("Nothing read from input stream!");
    }
  }
  return data.join("");
}

const CL_EXPECT_FAILURE = 0x1;
const CL_EXPECT_GZIP = 0x2;
const CL_EXPECT_3S_DELAY = 0x4;
const CL_SUSPEND = 0x8;
const CL_ALLOW_UNKNOWN_CL = 0x10;
const CL_EXPECT_LATE_FAILURE = 0x20;
const CL_FROM_CACHE = 0x40; // Response must be from the cache
const CL_NOT_FROM_CACHE = 0x80; // Response must NOT be from the cache
const CL_IGNORE_CL = 0x100; // don't bother to verify the content-length

const SUSPEND_DELAY = 3000;

/**
 * A stream listener that calls a callback function with a specified
 * context and the received data when the channel is loaded.
 *
 * Signature of the closure:
 *   void closure(in nsIRequest request, in ACString data, in JSObject context);
 *
 * This listener makes sure that various parts of the channel API are
 * implemented correctly and that the channel's status is a success code
 * (you can pass CL_EXPECT_FAILURE or CL_EXPECT_LATE_FAILURE as flags
 * to allow a failure code)
 *
 * Note that it also requires a valid content length on the channel and
 * is thus not fully generic.
 */
function ChannelListener(closure, ctx, flags) {
  this._closure = closure;
  this._closurectx = ctx;
  this._flags = flags;
  this._isFromCache = false;
  this._cacheEntryId = undefined;
}
ChannelListener.prototype = {
  _closure: null,
  _closurectx: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,
  _lastEvent: 0,

  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  onStartRequest(request) {
    try {
      if (this._got_onstartrequest) {
        do_throw("Got second onStartRequest event!");
      }
      this._got_onstartrequest = true;
      this._lastEvent = Date.now();

      try {
        this._isFromCache = request
          .QueryInterface(Ci.nsICacheInfoChannel)
          .isFromCache();
      } catch (e) {}

      var thrown = false;
      try {
        this._cacheEntryId = request
          .QueryInterface(Ci.nsICacheInfoChannel)
          .getCacheEntryId();
      } catch (e) {
        thrown = true;
      }
      if (this._isFromCache && thrown) {
        do_throw("Should get a CacheEntryId");
      } else if (!this._isFromCache && !thrown) {
        do_throw("Shouldn't get a CacheEntryId");
      }

      request.QueryInterface(Ci.nsIChannel);
      try {
        this._contentLen = request.contentLength;
      } catch (ex) {
        if (!(this._flags & (CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL))) {
          do_throw("Could not get contentLength");
        }
      }
      if (!request.isPending()) {
        do_throw("request reports itself as not pending from onStartRequest!");
      }
      if (
        this._contentLen == -1 &&
        !(this._flags & (CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL))
      ) {
        do_throw("Content length is unknown in onStartRequest!");
      }

      if (this._flags & CL_FROM_CACHE) {
        request.QueryInterface(Ci.nsICachingChannel);
        if (!request.isFromCache()) {
          do_throw("Response is not from the cache (CL_FROM_CACHE)");
        }
      }
      if (this._flags & CL_NOT_FROM_CACHE) {
        request.QueryInterface(Ci.nsICachingChannel);
        if (request.isFromCache()) {
          do_throw("Response is from the cache (CL_NOT_FROM_CACHE)");
        }
      }

      if (this._flags & CL_SUSPEND) {
        request.suspend();
        do_timeout(SUSPEND_DELAY, function() {
          request.resume();
        });
      }
    } catch (ex) {
      do_throw("Error in onStartRequest: " + ex);
    }
  },

  onDataAvailable(request, stream, offset, count) {
    try {
      let current = Date.now();

      if (!this._got_onstartrequest) {
        do_throw("onDataAvailable without onStartRequest event!");
      }
      if (this._got_onstoprequest) {
        do_throw("onDataAvailable after onStopRequest event!");
      }
      if (!request.isPending()) {
        do_throw("request reports itself as not pending from onDataAvailable!");
      }
      if (this._flags & CL_EXPECT_FAILURE) {
        do_throw("Got data despite expecting a failure");
      }

      if (
        current - this._lastEvent >= SUSPEND_DELAY &&
        !(this._flags & CL_EXPECT_3S_DELAY)
      ) {
        do_throw("Data received after significant unexpected delay");
      } else if (
        current - this._lastEvent < SUSPEND_DELAY &&
        this._flags & CL_EXPECT_3S_DELAY
      ) {
        do_throw("Data received sooner than expected");
      } else if (
        current - this._lastEvent >= SUSPEND_DELAY &&
        this._flags & CL_EXPECT_3S_DELAY
      ) {
        this._flags &= ~CL_EXPECT_3S_DELAY;
      } // No more delays expected

      this._buffer = this._buffer.concat(read_stream(stream, count));
      this._lastEvent = current;
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest(request, status) {
    try {
      var success = Components.isSuccessCode(status);
      if (!this._got_onstartrequest) {
        do_throw("onStopRequest without onStartRequest event!");
      }
      if (this._got_onstoprequest) {
        do_throw("Got second onStopRequest event!");
      }
      this._got_onstoprequest = true;
      if (
        this._flags & (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE) &&
        success
      ) {
        do_throw(
          "Should have failed to load URL (status is " +
            status.toString(16) +
            ")"
        );
      } else if (
        !(this._flags & (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE)) &&
        !success
      ) {
        do_throw("Failed to load URL: " + status.toString(16));
      }
      if (status != request.status) {
        do_throw("request.status does not match status arg to onStopRequest!");
      }
      if (request.isPending()) {
        do_throw("request reports itself as pending from onStopRequest!");
      }
      if (
        !(
          this._flags &
          (CL_EXPECT_FAILURE | CL_EXPECT_LATE_FAILURE | CL_IGNORE_CL)
        ) &&
        !(this._flags & CL_EXPECT_GZIP) &&
        this._contentLen != -1
      ) {
        Assert.equal(this._buffer.length, this._contentLen);
      }
    } catch (ex) {
      do_throw("Error in onStopRequest: " + ex);
    }
    try {
      this._closure(
        request,
        this._buffer,
        this._closurectx,
        this._isFromCache,
        this._cacheEntryId
      );
      this._closurectx = null;
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  },
};

var ES_ABORT_REDIRECT = 0x01;

function ChannelEventSink(flags) {
  this._flags = flags;
}

ChannelEventSink.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface(iid) {
    if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  asyncOnChannelRedirect(oldChannel, newChannel, flags, callback) {
    if (this._flags & ES_ABORT_REDIRECT) {
      throw Components.Exception("", Cr.NS_BINDING_ABORTED);
    }

    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },
};

/**
 * A helper class to construct origin attributes.
 */
function OriginAttributes(inIsolatedMozBrowser, privateId) {
  this.inIsolatedMozBrowser = inIsolatedMozBrowser;
  this.privateBrowsingId = privateId;
}
OriginAttributes.prototype = {
  inIsolatedMozBrowser: false,
  privateBrowsingId: 0,
};

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let pem = readFile(certFile)
    .replace(/-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----/, "")
    .replace(/[\r\n]/g, "");
  certdb.addCertFromBase64(pem, trustString);
}
