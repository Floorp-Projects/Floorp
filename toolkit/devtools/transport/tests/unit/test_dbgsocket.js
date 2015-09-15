/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var gPort;
var gExtraListener;

function run_test()
{
  do_print("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_task(test_socket_conn);
  add_task(test_socket_shutdown);
  add_test(test_pipe_conn);

  run_next_test();
}

function* test_socket_conn()
{
  do_check_eq(DebuggerServer.listeningSockets, 0);
  let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
  let authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  let listener = DebuggerServer.createListener();
  do_check_true(listener);
  listener.portOrPath = -1 /* any available port */;
  listener.authenticator = authenticator;
  listener.open();
  do_check_eq(DebuggerServer.listeningSockets, 1);
  gPort = DebuggerServer._listeners[0].port;
  do_print("Debugger server port is " + gPort);
  // Open a second, separate listener
  gExtraListener = DebuggerServer.createListener();
  gExtraListener.portOrPath = -1;
  gExtraListener.authenticator = authenticator;
  gExtraListener.open();
  do_check_eq(DebuggerServer.listeningSockets, 2);

  do_print("Starting long and unicode tests at " + new Date().toTimeString());
  let unicodeString = "(╯°□°）╯︵ ┻━┻";
  let transport = yield DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: gPort
  });
  let closedDeferred = promise.defer();
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
      closedDeferred.resolve();
    },
  };
  transport.ready();
  return closedDeferred.promise;
}

function* test_socket_shutdown()
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
  try {
    let transport = yield DebuggerClient.socketConnect({
      host: "127.0.0.1",
      port: gPort
    });
  } catch (e) {
    if (e.result == Cr.NS_ERROR_CONNECTION_REFUSED ||
        e.result == Cr.NS_ERROR_NET_TIMEOUT) {
      // The connection should be refused here, but on slow or overloaded
      // machines it may just time out.
      do_check_true(true);
      return;
    } else {
      throw e;
    }
  }

  // Shouldn't reach this, should never connect.
  do_check_true(false);
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
