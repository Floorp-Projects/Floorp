const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);
const isLinux = ("@mozilla.org/gnome-gconf-service;1" in Cc);

function getLinkFile()
{
  if (isWindows) {
    return do_get_file("test_link.url");
  }
  if (isLinux) {
    return do_get_file("test_link.desktop");
  }
  do_throw("Unexpected platform");
  return null;
}

const ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var link;
var linkURI;
const newURI = ios.newURI("http://www.mozilla.org/", null, null);
  
function NotificationCallbacks(origURI, newURI)
{
  this._origURI = origURI;
  this._newURI = newURI;
}
NotificationCallbacks.prototype = {
  QueryInterface: function(iid)
  {
    if (iid.equals(Ci.nsISupports) ||
	iid.equals(Ci.nsIInterfaceRequestor) ||
	iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  getInterface: function (iid)
  {
    return this.QueryInterface(iid);
  },
  asyncOnChannelRedirect: function(oldChan, newChan, flags, callback)
  {
    do_check_eq(oldChan.URI.spec, this._origURI.spec);
    do_check_eq(oldChan.URI, this._origURI);
    do_check_eq(oldChan.originalURI.spec, this._origURI.spec);
    do_check_eq(oldChan.originalURI, this._origURI);
    do_check_eq(newChan.originalURI.spec, this._newURI.spec);
    do_check_eq(newChan.originalURI, newChan.URI);
    do_check_eq(newChan.URI.spec, this._newURI.spec);
    throw Cr.NS_ERROR_ABORT;
  }
};

function RequestObserver(origURI, newURI, nextTest)
{
  this._origURI = origURI;
  this._newURI = newURI;
  this._nextTest = nextTest;
}
RequestObserver.prototype = {
  QueryInterface: function(iid)
  {
    if (iid.equals(Ci.nsISupports) ||
	iid.equals(Ci.nsIRequestObserver) ||
	iid.equals(Ci.nsIStreamListener)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  onStartRequest: function (req, ctx)
  {
    var chan = req.QueryInterface(Ci.nsIChannel);
    do_check_eq(chan.URI.spec, this._origURI.spec);
    do_check_eq(chan.URI, this._origURI);
    do_check_eq(chan.originalURI.spec, this._origURI.spec);
    do_check_eq(chan.originalURI, this._origURI);
  },
  onDataAvailable: function(req, ctx, stream, offset, count)
  {
    do_throw("Unexpected call to onDataAvailable");
  },
  onStopRequest: function (req, ctx, status)
  {
    var chan = req.QueryInterface(Ci.nsIChannel);
    try {
      do_check_eq(chan.URI.spec, this._origURI.spec);
      do_check_eq(chan.URI, this._origURI);
      do_check_eq(chan.originalURI.spec, this._origURI.spec);
      do_check_eq(chan.originalURI, this._origURI);
      do_check_eq(status, Cr.NS_ERROR_ABORT);
      do_check_false(chan.isPending());
    } catch(e) {}
    this._nextTest();
  }
};

function test_cancel()
{
  var chan = ios.newChannelFromURI(linkURI);
  do_check_eq(chan.URI, linkURI);
  do_check_eq(chan.originalURI, linkURI);
  chan.asyncOpen(new RequestObserver(linkURI, newURI, do_test_finished), null);
  do_check_true(chan.isPending());
  chan.cancel(Cr.NS_ERROR_ABORT);
  do_check_true(chan.isPending());
}

function run_test()
{
  if (!isWindows && !isLinux) {
    return;
  }

  link = getLinkFile();
  linkURI = ios.newFileURI(link);

  do_test_pending();

  var chan = ios.newChannelFromURI(linkURI);
  do_check_eq(chan.URI, linkURI);
  do_check_eq(chan.originalURI, linkURI);
  chan.notificationCallbacks = new NotificationCallbacks(linkURI, newURI);
  chan.asyncOpen(new RequestObserver(linkURI, newURI, test_cancel), null);
  do_check_true(chan.isPending());
}
