/**
 * Send HTTP requests and check if the received content is correct.
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

var expectedResponse;

function makeRequest(uri, response) {
  let requestChannel = NetUtil.newChannel({uri, loadUsingSystemPrincipal: true});

  expectedResponse = response;
  requestChannel.asyncOpen2(new ChannelListener(checkResponse, null, CL_EXPECT_GZIP));
}

function checkResponse(request, buffer) {
  do_check_eq(expectedResponse, buffer);

  do_send_remote_message(`response`);
}

function finish() {
  shouldQuit = true;
}
