/*
 * Test to ensure that image loading/decoding notifications are always
 * delivered async, and in the order we expect.
 *
 * Must be included from a file that has a uri of the image to test defined in
 * var uri.
 */

do_load_httpd_js();

var server = new nsHttpServer();
server.registerDirectory("/", do_get_file(''));
server.registerContentType("sjs", "sjs");
server.start(8088);

load('image_load_helpers.js');

var requests = [];

// Return a closure that holds on to the listener from the original
// imgIRequest, and compares its results to the cloned one.
function getCloneStopCallback(original_listener)
{
  return function cloneStop(listener) {
    do_check_eq(original_listener.state, listener.state);

    // Sanity check to make sure we didn't accidentally use the same listener
    // twice.
    do_check_neq(original_listener, listener);
    do_test_finished();
  }
}

// Make sure that cloned requests get all the same callbacks as the original,
// but they aren't synchronous right now.
function checkClone(other_listener, aRequest)
{
  do_test_pending();

  // For as long as clone notification is synchronous, we can't test the clone state reliably.
  var listener = new ImageListener(null, function(foo, bar) { do_test_finished(); } /*getCloneStopCallback(other_listener)*/);
  listener.synchronous = false;
  var clone = aRequest.clone(listener);
}

// Ensure that all the callbacks were called on aRequest.
function checkAllCallbacks(listener, aRequest)
{
  do_check_eq(listener.state, ALL_BITS);

  do_test_finished();
}

function secondLoadDone(oldlistener, aRequest)
{
  do_test_pending();

  try {
    var staticrequest = aRequest.getStaticRequest();

    // For as long as clone notification is synchronous, we can't test the
    // clone state reliably.
    var listener = new ImageListener(null, checkAllCallbacks);
    listener.synchronous = false;
    var staticrequestclone = staticrequest.clone(listener);
  } catch(e) {
    // We can't create a static request. Most likely the request we started
    // with didn't load successfully.
    do_test_finished();
  }

  run_loadImageWithChannel_tests();

  do_test_finished();
}

// Load the request a second time. This should come from the image cache, and
// therefore would be at most risk of being served synchronously.
function checkSecondLoad()
{
  do_test_pending();

  var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);
  var listener = new ImageListener(checkClone, secondLoadDone);
  requests.push(loader.loadImage(uri, null, null, null, listener, null, 0, null, null, null));
  listener.synchronous = false;
}

function firstLoadDone(oldlistener, aRequest)
{
  checkSecondLoad(uri);

  do_test_finished();
}

// Return a closure that allows us to check the stream listener's status when the
// image starts loading.
function getChannelLoadImageStartCallback(streamlistener)
{
  return function channelLoadStart(imglistener, aRequest) {
    // We must not have received any status before we get this start callback.
    // If we have, we've broken people's expectations by delaying events from a
    // channel we were given.
    do_check_eq(streamlistener.requestStatus, 0);

    checkClone(imglistener, aRequest);
  }
}

// Return a closure that allows us to check the stream listener's status when the
// image finishes loading.
function getChannelLoadImageStopCallback(streamlistener, next)
{
  return function channelLoadStop(imglistener, aRequest) {
    // We absolutely must not get imgIDecoderObserver::onStopRequest after
    // nsIRequestObserver::onStopRequest has fired. If we do that, we've broken
    // people's expectations by delaying events from a channel we were given.
    do_check_eq(streamlistener.requestStatus & STOP_REQUEST, 0);

    next();

    do_test_finished();
  }
}

// Load the request a second time. This should come from the image cache, and
// therefore would be at most risk of being served synchronously.
function checkSecondChannelLoad()
{
  do_test_pending();

  var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);  
  var channel = ioService.newChannelFromURI(uri);
  var channellistener = new ChannelListener();
  channel.asyncOpen(channellistener, null);

  var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);
  var listener = new ImageListener(getChannelLoadImageStartCallback(channellistener),
                                   getChannelLoadImageStopCallback(channellistener,
                                                                   all_done_callback));
  var outlistener = {};
  requests.push(loader.loadImageWithChannel(channel, listener, null, outlistener));
  channellistener.outputListener = outlistener.value;

  listener.synchronous = false;
}

function run_loadImageWithChannel_tests()
{
  // To ensure we're testing what we expect to, clear the content image cache
  // between test runs.
  var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);
  loader.QueryInterface(Ci.imgICache);
  loader.clearCache(false);

  do_test_pending();

  var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);  
  var channel = ioService.newChannelFromURI(uri);
  var channellistener = new ChannelListener();
  channel.asyncOpen(channellistener, null);

  var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);
  var listener = new ImageListener(getChannelLoadImageStartCallback(channellistener),
                                   getChannelLoadImageStopCallback(channellistener,
                                                                   checkSecondChannelLoad));
  var outlistener = {};
  requests.push(loader.loadImageWithChannel(channel, listener, null, outlistener));
  channellistener.outputListener = outlistener.value;

  listener.synchronous = false;
}

function all_done_callback()
{
  server.stop(function() { do_test_finished(); });
}

function startImageCallback(otherCb)
{
  return function(listener, request)
  {
    var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);

    // Make sure we can load the same image immediately out of the cache.
    do_test_pending();
    var listener2 = new ImageListener(null, function(foo, bar) { do_test_finished(); });
    requests.push(loader.loadImage(uri, null, null, null, listener2, null, 0, null, null, null));
    listener2.synchronous = false;

    // Now that we've started another load, chain to the callback.
    otherCb(listener, request);
  }
}

function run_test()
{
  var loader = Cc["@mozilla.org/image/loader;1"].getService(Ci.imgILoader);

  do_test_pending();
  var listener = new ImageListener(startImageCallback(checkClone), firstLoadDone);
  var req = loader.loadImage(uri, null, null, null, listener, null, 0, null, null, null);
  requests.push(req);

  // Ensure that we don't cause any mayhem when we lock an image.
  req.lockImage();

  listener.synchronous = false;
}
