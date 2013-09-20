// Opening a second listening socket on the same address as an extant
// socket should elicit NS_ERROR_SOCKET_ADDRESS_IN_USE on non-Windows
// machines.

const CC = Components.Constructor;

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

function testAddrInUse()
{
  // Windows lets us have as many sockets listening on the same address as
  // we like, evidently.
  if ("@mozilla.org/windows-registry-key;1" in Cc) {
    return;
  }

  // Create listening socket:
  // any port (-1), loopback only (true), default backlog (-1)
  let listener = ServerSocket(-1, true, -1);
  do_check_true(listener instanceof Ci.nsIServerSocket);

  // Try to create another listening socket on the same port, whatever that was.
  do_check_throws_nsIException(() => ServerSocket(listener.port, true, -1),
                               "NS_ERROR_SOCKET_ADDRESS_IN_USE");
}

function run_test()
{
  testAddrInUse();
}
