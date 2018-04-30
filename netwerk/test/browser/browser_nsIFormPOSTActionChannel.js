/*
 * Tests for bug 1241377: A channel with nsIFormPOSTActionChannel interface
 * should be able to accept form POST.
 */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const SCHEME = "x-bug1241377";

const FORM_BASE = SCHEME + "://dummy/form/";
const NORMAL_FORM_URI = FORM_BASE + "normal.html";
const UPLOAD_FORM_URI = FORM_BASE + "upload.html";
const POST_FORM_URI = FORM_BASE + "post.html";

const ACTION_BASE = SCHEME + "://dummy/action/";
const NORMAL_ACTION_URI = ACTION_BASE + "normal.html";
const UPLOAD_ACTION_URI = ACTION_BASE + "upload.html";
const POST_ACTION_URI = ACTION_BASE + "post.html";

function CustomProtocolHandler() {
}
CustomProtocolHandler.prototype = {
  /** nsIProtocolHandler */
  get scheme() {
    return SCHEME;
  },
  get defaultPort() {
    return -1;
  },
  get protocolFlags() {
    return Ci.nsIProtocolHandler.URI_NORELATIVE |
           Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE;
  },
  newURI: function(aSpec, aOriginCharset, aBaseURI) {
    return Cc["@mozilla.org/network/standard-url-mutator;1"]
             .createInstance(Ci.nsIURIMutator)
             .setSpec(aSpec)
             .finalize()
  },
  newChannel2: function(aURI, aLoadInfo) {
    return new CustomChannel(aURI, aLoadInfo);
  },
  newChannel: function(aURI) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  allowPort: function(port, scheme) {
    return port != -1;
  },

  /** nsIFactory */
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(aIID);
  },
  lockFactory: function() {},

  /** nsISupports */
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler,
                                          Ci.nsIFactory]),
  classID: Components.ID("{16d594bc-d9d8-47ae-a139-ea714dc0c35c}")
};

function CustomChannel(aURI, aLoadInfo) {
  this.uri = aURI;
  this.loadInfo = aLoadInfo;

  this._uploadStream = null;

  var interfaces = [Ci.nsIRequest, Ci.nsIChannel];
  if (this.uri.spec == POST_ACTION_URI) {
    interfaces.push(Ci.nsIFormPOSTActionChannel);
  } else if (this.uri.spec == UPLOAD_ACTION_URI) {
    interfaces.push(Ci.nsIUploadChannel);
  }
  this.QueryInterface = XPCOMUtils.generateQI(interfaces);
}
CustomChannel.prototype = {
  /** nsIUploadChannel */
  get uploadStream() {
    return this._uploadStream;
  },
  set uploadStream(val) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  setUploadStream: function(aStream, aContentType, aContentLength) {
    this._uploadStream = aStream;
  },

  /** nsIChannel */
  get originalURI() {
    return this.uri;
  },
  get URI() {
    return this.uri;
  },
  owner: null,
  notificationCallbacks: null,
  get securityInfo() {
    return null;
  },
  get contentType() {
    return "text/html";
  },
  set contentType(val) {
  },
  contentCharset: "UTF-8",
  get contentLength() {
    return -1;
  },
  set contentLength(val) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  open: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  open2: function() {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  asyncOpen: function(aListener, aContext) {
    var data = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>test bug 1241377</title>
</head>
<body>
`;

    if (this.uri.spec.startsWith(FORM_BASE)) {
      data += `
<form id="form" action="${this.uri.spec.replace(FORM_BASE, ACTION_BASE)}"
      method="post" enctype="text/plain" target="frame">
<input type="hidden" name="foo" value="bar">
<input type="submit">
</form>

<iframe id="frame" name="frame" width="200" height="200"></iframe>

<script type="text/javascript">
<!--
document.getElementById('form').submit();
//-->
</script>
`;
    } else if (this.uri.spec.startsWith(ACTION_BASE)) {
      var postData = "";
      var headers = {};
      if (this._uploadStream) {
        var bstream = Cc["@mozilla.org/binaryinputstream;1"]
            .createInstance(Ci.nsIBinaryInputStream);
        bstream.setInputStream(this._uploadStream);
        postData = bstream.readBytes(bstream.available());

        if (this._uploadStream instanceof Ci.nsIMIMEInputStream) {
          this._uploadStream.visitHeaders((name, value) => {
            headers[name] = value;
          });
        }
      }
      data += `
<input id="upload_stream" value="${this._uploadStream ? "yes" : "no"}">
<input id="post_data" value="${btoa(postData)}">
<input id="upload_headers" value='${JSON.stringify(headers)}'>
`;
    }

    data += `
</body>
</html>
`;

    var stream = Cc["@mozilla.org/io/string-input-stream;1"]
        .createInstance(Ci.nsIStringInputStream);
    stream.setData(data, data.length);

    var runnable = {
      run: () => {
        try {
          aListener.onStartRequest(this, aContext);
        } catch(e) {}
        try {
          aListener.onDataAvailable(this, aContext, stream, 0, stream.available());
        } catch(e) {}
        try {
          aListener.onStopRequest(this, aContext, Cr.NS_OK);
        } catch(e) {}
      }
    };
    Services.tm.dispatchToMainThread(runnable);
  },
  asyncOpen2: function(aListener) {
    this.asyncOpen(aListener, null);
  },

  /** nsIRequest */
  get name() {
    return this.uri.spec;
  },
  isPending: function () {
    return false;
  },
  get status() {
    return Cr.NS_OK;
  },
  cancel: function(status) {},
  loadGroup: null,
  loadFlags: Ci.nsIRequest.LOAD_NORMAL |
             Ci.nsIRequest.INHIBIT_CACHING |
             Ci.nsIRequest.LOAD_BYPASS_CACHE,
};

function frameScript() {
  addMessageListener("Test:WaitForIFrame", function() {
    var check = function() {
      if (content) {
        var frame = content.document.getElementById("frame");
        if (frame) {
          var upload_stream = frame.contentDocument.getElementById("upload_stream");
          var post_data = frame.contentDocument.getElementById("post_data");
          var headers = frame.contentDocument.getElementById("upload_headers");
          if (upload_stream && post_data && headers) {
            sendAsyncMessage("Test:IFrameLoaded", [upload_stream.value, post_data.value, headers.value]);
            return;
          }
        }
      }

      setTimeout(check, 100);
    };

    check();
  });
}

function loadTestTab(uri) {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, uri);
  var browser = gBrowser.selectedBrowser;

  let manager = browser.messageManager;
  browser.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")();", true);

  return new Promise(resolve => {
    function listener({ data: [hasUploadStream, postData, headers] }) {
      manager.removeMessageListener("Test:IFrameLoaded", listener);
      resolve([hasUploadStream, atob(postData), JSON.parse(headers)]);
    }

    manager.addMessageListener("Test:IFrameLoaded", listener);
    manager.sendAsyncMessage("Test:WaitForIFrame");
  });
}

add_task(async function() {
  var handler = new CustomProtocolHandler();
  var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(handler.classID, "",
                            "@mozilla.org/network/protocol;1?name=" + handler.scheme,
                            handler);
  registerCleanupFunction(function() {
    registrar.unregisterFactory(handler.classID, handler);
  });
});

add_task(async function() {
  var [hasUploadStream, postData] = await loadTestTab(NORMAL_FORM_URI);
  is(hasUploadStream, "no", "normal action should not have uploadStream");

  gBrowser.removeCurrentTab();
});

add_task(async function() {
  var [hasUploadStream, postData] = await loadTestTab(UPLOAD_FORM_URI);
  is(hasUploadStream, "no", "upload action should not have uploadStream");

  gBrowser.removeCurrentTab();
});

add_task(async function() {
  var [hasUploadStream, postData, headers] = await loadTestTab(POST_FORM_URI);

  is(hasUploadStream, "yes", "post action should have uploadStream");
  is(postData, "foo=bar\r\n",
     "POST data is received correctly");

  is(headers["Content-Type"], "text/plain", "Content-Type header is correct");
  is(headers["Content-Length"], undefined, "Content-Length header is correct");

  gBrowser.removeCurrentTab();
});
