"use strict";

function getLinkFile() {
  if (mozinfo.os == "win") {
    return do_get_file("test_link.url");
  }
  if (mozinfo.os == "linux") {
    return do_get_file("test_link.desktop");
  }
  do_throw("Unexpected platform");
  return null;
}

const ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var link;
var linkURI;
const newURI = ios.newURI("http://www.mozilla.org/");

function NotificationCallbacks(origURI, newURI) {
  this._origURI = origURI;
  this._newURI = newURI;
}
NotificationCallbacks.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIInterfaceRequestor",
    "nsIChannelEventSink",
  ]),
  getInterface(iid) {
    return this.QueryInterface(iid);
  },
  asyncOnChannelRedirect(oldChan, newChan, flags, callback) {
    Assert.equal(oldChan.URI.spec, this._origURI.spec);
    Assert.equal(oldChan.URI, this._origURI);
    Assert.equal(oldChan.originalURI.spec, this._origURI.spec);
    Assert.equal(oldChan.originalURI, this._origURI);
    Assert.equal(newChan.originalURI.spec, this._newURI.spec);
    Assert.equal(newChan.originalURI, newChan.URI);
    Assert.equal(newChan.URI.spec, this._newURI.spec);
    throw Components.Exception("", Cr.NS_ERROR_ABORT);
  },
};

function RequestObserver(origURI, newURI, nextTest) {
  this._origURI = origURI;
  this._newURI = newURI;
  this._nextTest = nextTest;
}
RequestObserver.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "nsIRequestObserver",
    "nsIStreamListener",
  ]),
  onStartRequest(req) {
    var chan = req.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.URI.spec, this._origURI.spec);
    Assert.equal(chan.URI, this._origURI);
    Assert.equal(chan.originalURI.spec, this._origURI.spec);
    Assert.equal(chan.originalURI, this._origURI);
  },
  onDataAvailable(req, stream, offset, count) {
    do_throw("Unexpected call to onDataAvailable");
  },
  onStopRequest(req, status) {
    var chan = req.QueryInterface(Ci.nsIChannel);
    try {
      Assert.equal(chan.URI.spec, this._origURI.spec);
      Assert.equal(chan.URI, this._origURI);
      Assert.equal(chan.originalURI.spec, this._origURI.spec);
      Assert.equal(chan.originalURI, this._origURI);
      Assert.equal(status, Cr.NS_ERROR_ABORT);
      Assert.ok(!chan.isPending());
    } catch (e) {}
    this._nextTest();
  },
};

function test_cancel() {
  var chan = NetUtil.newChannel({
    uri: linkURI,
    loadUsingSystemPrincipal: true,
  });
  Assert.equal(chan.URI, linkURI);
  Assert.equal(chan.originalURI, linkURI);
  chan.asyncOpen(new RequestObserver(linkURI, newURI, do_test_finished));
  Assert.ok(chan.isPending());
  chan.cancel(Cr.NS_ERROR_ABORT);
  Assert.ok(chan.isPending());
}

function run_test() {
  if (mozinfo.os != "win" && mozinfo.os != "linux") {
    return;
  }

  link = getLinkFile();
  if (link.isSymlink()) {
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(link.target);
    linkURI = ios.newFileURI(file);
  } else {
    linkURI = ios.newFileURI(link);
  }

  do_test_pending();
  var chan = NetUtil.newChannel({
    uri: linkURI,
    loadUsingSystemPrincipal: true,
  });
  Assert.equal(chan.URI, linkURI);
  Assert.equal(chan.originalURI, linkURI);
  chan.notificationCallbacks = new NotificationCallbacks(linkURI, newURI);
  chan.asyncOpen(new RequestObserver(linkURI, newURI, test_cancel));
  Assert.ok(chan.isPending());
}
