/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Bug 755412 - checks if the server drops the connection on an improperly
 * framed packet, i.e. when the length header is invalid.
 */

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource://gre/modules/devtools/DevToolsUtils.jsm");

let port = 2929;

function run_test()
{
  do_print("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_test(test_socket_conn_drops_after_invalid_header);
  add_test(test_socket_conn_drops_after_invalid_header_2);
  add_test(test_socket_conn_drops_after_too_long_header);
  run_next_test();
}

function test_socket_conn_drops_after_invalid_header() {
  return test_helper('fluff30:27:{"to":"root","type":"echo"}');
}

function test_socket_conn_drops_after_invalid_header_2() {
  return test_helper('27asd:{"to":"root","type":"echo"}');
}

function test_socket_conn_drops_after_too_long_header() {
  return test_helper('4305724038957487634549823475894325');
}


function test_helper(payload) {
  try_open_listener();

  let transport = debuggerSocketConnect("127.0.0.1", port);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        do_throw(new Error("This connection should be dropped."));
        transport.close();
      }

      // Inject the payload directly into the stream.
      transport._outgoing += payload;
      transport._flushOutgoing();
    },
    onClosed: function(aStatus) {
      do_check_true(true);
      run_next_test();
    },
  };
  transport.ready();
}

function try_open_listener()
{
  try {
    do_check_true(DebuggerServer.openListener(port));
  } catch (e) {
    // In case the port is unavailable, pick a random one between 2000 and 65000.
    port = Math.floor(Math.random() * (65000 - 2000 + 1)) + 2000;
    try_open_listener();
  }
}
