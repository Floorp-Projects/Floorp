/**
 * Send HTTP requests and notify the parent about their channelId
 */

Cu.import("resource://gre/modules/NetUtil.jsm");

let shouldQuit = false;

function run_test() {
  // keep the event loop busy and the test alive until a "finish" command
  // is issued by parent
  do_timeout(100, function keepAlive() {
    if (!shouldQuit) {
      do_timeout(100, keepAlive);
    }
  });
}

function makeRequest(uri) {
  let requestChannel = NetUtil.newChannel({uri, loadUsingSystemPrincipal: true});
  requestChannel.asyncOpen2(new ChannelListener(checkResponse, requestChannel));
  requestChannel.QueryInterface(Ci.nsIHttpChannel);
  dump(`Child opened request: ${uri}, channelId=${requestChannel.channelId}\n`);
}

function checkResponse(request, buffer, requestChannel) {
  // notify the parent process about the original request channel
  requestChannel.QueryInterface(Ci.nsIHttpChannel);
  do_send_remote_message(`request:${requestChannel.channelId}`);

  // the response channel can be different (if it was redirected)
  let responseChannel = request.QueryInterface(Ci.nsIHttpChannel);

  let uri = responseChannel.URI.spec;
  let origUri = responseChannel.originalURI.spec;
  let id = responseChannel.channelId;
  dump(`Child got response to: ${uri} (orig=${origUri}), channelId=${id}\n`);

  // notify the parent process about this channel's ID
  do_send_remote_message(`response:${id}`);
}

function finish() {
  shouldQuit = true;
}
