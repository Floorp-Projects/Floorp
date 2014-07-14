/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");

let gPort;
let gExtraListener;

function run_test()
{
  do_print("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_test(test_socket_conn);
  add_test(test_socket_shutdown);
  add_test(test_pipe_conn);

  run_next_test();
}

function test_socket_conn()
{
  do_check_eq(DebuggerServer.listeningSockets, 0);
  do_check_true(DebuggerServer.openListener(-1));
  do_check_eq(DebuggerServer.listeningSockets, 1);
  gPort = DebuggerServer._listeners[0].port;
  do_print("Debugger server port is " + gPort);
  // Open a second, separate listener
  gExtraListener = DebuggerServer.openListener(-1);
  do_check_eq(DebuggerServer.listeningSockets, 2);

  do_print("Starting long and unicode tests at " + new Date().toTimeString());
  let unicodeString = "(╯°□°）╯︵ ┻━┻";
  let transport = debuggerSocketConnect("127.0.0.1", gPort);
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
  do_check_eq(DebuggerServer.listeningSockets, 2);
  gExtraListener.close();
  do_check_eq(DebuggerServer.listeningSockets, 1);
  do_check_true(DebuggerServer.closeAllListeners());
  do_check_eq(DebuggerServer.listeningSockets, 0);
  // Make sure closing the listener twice does nothing.
  do_check_false(DebuggerServer.closeAllListeners());
  do_check_eq(DebuggerServer.listeningSockets, 0);

  do_print("Connecting to a server socket at " + new Date().toTimeString());
  let transport = debuggerSocketConnect("127.0.0.1", gPort);
  transport.hooks = {
    onPacket: function(aPacket) {
      // Shouldn't reach this, should never connect.
      do_check_true(false);
    },

    onClosed: function(aStatus) {
      do_print("test_socket_shutdown onClosed called at " + new Date().toTimeString());
      // The connection should be refused here, but on slow or overloaded
      // machines it may just time out.
      let expected = [ Cr.NS_ERROR_CONNECTION_REFUSED, Cr.NS_ERROR_NET_TIMEOUT ];
      do_check_neq(expected.indexOf(aStatus), -1);
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
