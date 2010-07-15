/**
 * Basic tests for the moz-icon:// protocol
 * Tests will try to load icons for a set of given extensions.
 * Different icon sizes will be tested as well.
 * 
 * Some systems use svg icons. Those svg icons are also
 * expected to load just fine.
 *
 *
 * Written by Nils Maier <MaierMan@web.de>
 * Code is in the public domain; Copyrights are disclaimed
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Ctor = Components.Constructor;
const Exception = Components.Exception;

const URIDEFAULT = "moz-icon://file.%e";
const URISIZE = "moz-icon://file.%e?size=%s";


const IOService = Cc["@mozilla.org/network/io-service;1"].
                  getService(Ci.nsIIOService);


// These file extensions should be tested
// All systems should provide some kind of icon
var extensions = [
  'png', 'jpg', 'jpeg',
  'pdf',
  'mpg', 'avi', 'mov',
  'zip', 'rar', 'tar.gz', 'tar.bz2',
];

// What sizes to test
var sizes = [
  16,
  24,
  32,
  48,
  64
];

// Stream listener for the channels
function Listener() {}
Listener.prototype = {
  _dataReceived: false,
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener)
        || iid.equals(Ci.nsIRequestObserver)
        || iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onStopRequest: function(aRequest, aContext, aStatusCode) {
    // Usually newChannel will throw
    // However, better be safe than sorry and check
    // the channel actually returned data
    do_check_true(!!this._dataReceived);

    do_test_finished();
  },
  onStartRequest: function() {},
  onDataAvailable: function(aRequest, aContext, aInputStream, aOffset, aCount) {
    this._dataReceived |= aInputStream.available() > 0;

    aRequest.cancel(0x804B0002); // binding aborted
  }
};

// test a single url for all extensions
function verifyChannelFor(aExt, aURI) {
  var uri = aURI.replace(/%e/g, aExt);
  try {
    var channel = IOService.newChannel(uri, null, null);
    channel.asyncOpen(new Listener(), null);

    do_test_pending();
  }
  catch (ex) {
    // If moz-icon: cannot "resolve" an icon then newChannel will throw.
    do_throw("Cannot open channel for " + uri + " Error: " + ex);
  }
}

// runs the test
function run_test() {
  for each (let ext in extensions) {
    verifyChannelFor(ext, URIDEFAULT);
    for each (let size in sizes) {
      verifyChannelFor(ext, URISIZE.replace(/%s/g, size));
    }
  }
};
