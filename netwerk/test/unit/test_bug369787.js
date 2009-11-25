do_load_httpd_js();

const BUGID = "369787";
var server = null;
var channel = null;

function change_content_type() {
  var origType = channel.contentType;
  const newType = "x-foo/x-bar";
  channel.contentType = newType;
  do_check_eq(channel.contentType, newType);
  channel.contentType = origType;
  do_check_eq(channel.contentType, origType);
}

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {
  change_content_type();
}
TestListener.prototype.onStopRequest = function(request, context, status) {
  change_content_type();

  do_timeout(0, after_channel_closed);
}

function after_channel_closed() {
  try {
    change_content_type();
  } finally {
    server.stop(do_test_finished);
  }
}

function run_test() {
  // start server
  server = new nsHttpServer();

  server.registerPathHandler("/bug" + BUGID, bug369787);

  server.start(4444);

  // make request
  channel =
      Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService).
      newChannel("http://localhost:4444/bug" + BUGID, null, null);

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}

// PATH HANDLER FOR /bug369787
function bug369787(metadata, response) {
  /* do nothing */
}
