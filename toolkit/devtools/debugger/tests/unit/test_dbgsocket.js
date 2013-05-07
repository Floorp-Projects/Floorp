/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

let port = 2929;

function run_test()
{
  do_print("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_test(test_socket_conn);
  add_test(test_socket_shutdown);
  add_test(test_pipe_conn);

  run_next_test();
}

function really_long() {
  let ret = "0123456789";
  for (let i = 0; i < 18; i++) {
    ret += ret;
  }
  return ret;
}

function test_socket_conn()
{
  do_check_eq(DebuggerServer._socketConnections, 0);
  try_open_listener();
  do_print("Debugger server port is " + port);
  do_check_eq(DebuggerServer._socketConnections, 1);
  // Make sure opening the listener twice does nothing.
  do_check_true(DebuggerServer.openListener(port));
  do_check_eq(DebuggerServer._socketConnections, 1);

  do_print("Starting long and unicode tests at " + new Date().toTimeString());
  let unicodeString = "(╯°□°）╯︵ ┻━┻";
  let transport = debuggerSocketConnect("127.0.0.1", port);
  transport.hooks = {
    onPacket: function(aPacket) {
      this.onPacket = function(aPacket) {
        do_check_eq(aPacket.unicode, unicodeString);
        transport.close();
      }
      // Verify that things work correctly when bigger than the output
      // transport buffers and when transporting unicode...
      transport.send({to: "root",
                      type: "echo",
                      reallylong: really_long(),
                      unicode: unicodeString});
      do_check_eq(aPacket.from, "root");
    },
    onClosed: function(aStatus) {
      run_next_test();
    },
  };
  transport.ready();
}

function test_socket_shutdown()
{
  do_check_eq(DebuggerServer._socketConnections, 1);
  do_check_true(DebuggerServer.closeListener());
  do_check_eq(DebuggerServer._socketConnections, 0);
  // Make sure closing the listener twice does nothing.
  do_check_false(DebuggerServer.closeListener());
  do_check_eq(DebuggerServer._socketConnections, 0);

  do_print("Connecting to a server socket at " + new Date().toTimeString());
  let transport = debuggerSocketConnect("127.0.0.1", port);
  transport.hooks = {
    onPacket: function(aPacket) {
      // Shouldn't reach this, should never connect.
      do_check_true(false);
    },

    onClosed: function(aStatus) {
      do_print("test_socket_shutdown onClosed called at " + new Date().toTimeString());
      do_check_eq(aStatus, Cr.NS_ERROR_CONNECTION_REFUSED);
      run_next_test();
    }
  };

  do_print("Initializing input stream at " + new Date().toTimeString());
  transport.ready();
}

function test_pipe_conn()
{
  let transport = DebuggerServer.connectPipe();
  transport.hooks = {
    onPacket: function(aPacket) {
      do_check_eq(aPacket.from, "root");
      transport.close();
    },
    onClosed: function(aStatus) {
      run_next_test();
    }
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
