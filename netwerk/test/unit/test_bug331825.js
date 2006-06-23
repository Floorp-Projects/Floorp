var server;
var BUGID = "331825";

function handle_response(stream) {
  var response = server.handler.headers("304 Not Modified") + "\r\n";
  stream.write(response, response.length);
}

function TestListener() {
}
TestListener.prototype.onStartRequest = function(request, context) {
}
TestListener.prototype.onStopRequest = function(request, context, status) {
  var channel = request.QueryInterface(Components.interfaces.nsIHttpChannel);
  do_check_eq(channel.responseStatus, 304);

  do_test_finished();
}

function run_test() {
  // start server
  server = new nsTestServ(4444);
  server.handler["/bug" + BUGID] = handle_response;
  server.startListening();

  // make request
  var channel =
      Components.classes["@mozilla.org/network/io-service;1"].
      getService(Components.interfaces.nsIIOService).
      newChannel("http://localhost:4444/bug" + BUGID, null, null);

  channel.QueryInterface(Components.interfaces.nsIHttpChannel);
  channel.setRequestHeader("If-None-Match", "foobar", false);
  channel.asyncOpen(new TestListener(), null);

  do_test_pending();
}
