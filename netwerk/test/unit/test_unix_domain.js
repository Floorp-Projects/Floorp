// Exercise Unix domain sockets.

var CC = Components.Constructor;

const UnixServerSocket = CC("@mozilla.org/network/server-socket;1",
                            "nsIServerSocket",
                            "initWithFilename");

const ScriptableInputStream = CC("@mozilla.org/scriptableinputstream;1",
                                 "nsIScriptableInputStream",
                                 "init");

const IOService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
const socketTransportService = Cc["@mozilla.org/network/socket-transport-service;1"]
                               .getService(Ci.nsISocketTransportService);

const threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

const allPermissions = parseInt("777", 8);

function run_test()
{
  // If we're on Windows, simply check for graceful failure.
  if (mozinfo.os == "win") {
    test_not_supported();
    return;
  }

  add_test(test_echo);
  add_test(test_name_too_long);
  add_test(test_no_directory);
  add_test(test_no_such_socket);
  add_test(test_address_in_use);
  add_test(test_file_in_way);
  add_test(test_create_permission);
  add_test(test_connect_permission);
  add_test(test_long_socket_name);
  add_test(test_keep_when_offline);

  run_next_test();
}

// Check that creating a Unix domain socket fails gracefully on Windows.
function test_not_supported()
{
  let socketName = do_get_tempdir();
  socketName.append('socket');
  do_print("creating socket: " + socketName.path);

  do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                               "NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED");

  do_check_throws_nsIException(() => socketTransportService.createUnixDomainTransport(socketName),
                               "NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED");
}

// Actually exchange data with Unix domain sockets.
function test_echo()
{
  let log = '';

  let socketName = do_get_tempdir();
  socketName.append('socket');

  // Create a server socket, listening for connections.
  do_print("creating socket: " + socketName.path);
  let server = new UnixServerSocket(socketName, allPermissions, -1);
  server.asyncListen({
    onSocketAccepted: function(aServ, aTransport) {
      do_print("called test_echo's onSocketAccepted");
      log += 'a';

      Assert.equal(aServ, server);

      let connection = aTransport;

      // Check the server socket's self address.
      let connectionSelfAddr = connection.getScriptableSelfAddr();
      Assert.equal(connectionSelfAddr.family, Ci.nsINetAddr.FAMILY_LOCAL);
      Assert.equal(connectionSelfAddr.address, socketName.path);

      // The client socket is anonymous, so the server transport should
      // have an empty peer address.
      Assert.equal(connection.host, '');
      Assert.equal(connection.port, 0);
      let connectionPeerAddr = connection.getScriptablePeerAddr();
      Assert.equal(connectionPeerAddr.family, Ci.nsINetAddr.FAMILY_LOCAL);
      Assert.equal(connectionPeerAddr.address, '');

      let serverAsyncInput = connection.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
      let serverOutput = connection.openOutputStream(0, 0, 0);

      serverAsyncInput.asyncWait(function (aStream) {
        do_print("called test_echo's server's onInputStreamReady");
        let serverScriptableInput = new ScriptableInputStream(aStream);

        // Receive data from the client, and send back a response.
        Assert.equal(serverScriptableInput.readBytes(17), "Mervyn Murgatroyd");
        do_print("server has read message from client");
        serverOutput.write("Ruthven Murgatroyd", 18);
        do_print("server has written to client");
      }, 0, 0, threadManager.currentThread);
    },

    onStopListening: function(aServ, aStatus) {
      do_print("called test_echo's onStopListening");
      log += 's';

      Assert.equal(aServ, server);
      Assert.equal(log, 'acs');

      run_next_test();
    }
  });

  // Create a client socket, and connect to the server.
  let client = socketTransportService.createUnixDomainTransport(socketName);
  Assert.equal(client.host, socketName.path);
  Assert.equal(client.port, 0);

  let clientAsyncInput = client.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
  let clientInput = new ScriptableInputStream(clientAsyncInput);
  let clientOutput = client.openOutputStream(0, 0, 0);

  clientOutput.write("Mervyn Murgatroyd", 17);
  do_print("client has written to server");

  clientAsyncInput.asyncWait(function (aStream) {
    do_print("called test_echo's client's onInputStreamReady");
    log += 'c';

    Assert.equal(aStream, clientAsyncInput);

    // Now that the connection has been established, we can check the
    // transport's self and peer addresses.
    let clientSelfAddr = client.getScriptableSelfAddr();
    Assert.equal(clientSelfAddr.family, Ci.nsINetAddr.FAMILY_LOCAL);
    Assert.equal(clientSelfAddr.address, '');

    Assert.equal(client.host, socketName.path); // re-check, but hey
    let clientPeerAddr = client.getScriptablePeerAddr();
    Assert.equal(clientPeerAddr.family, Ci.nsINetAddr.FAMILY_LOCAL);
    Assert.equal(clientPeerAddr.address, socketName.path);

    Assert.equal(clientInput.readBytes(18), "Ruthven Murgatroyd");
    do_print("client has read message from server");

    server.close();
  }, 0, 0, threadManager.currentThread);
}

// Create client and server sockets using a path that's too long.
function test_name_too_long()
{
  let socketName = do_get_tempdir();
  // The length limits on all the systems NSPR supports are a bit past 100.
  socketName.append(new Array(1000).join('x'));

  // The length must be checked before we ever make any system calls --- we
  // have to create the sockaddr first --- so it's unambiguous which error
  // we should get here.

  do_check_throws_nsIException(() => new UnixServerSocket(socketName, 0, -1),
                               "NS_ERROR_FILE_NAME_TOO_LONG");

  // Unlike most other client socket errors, this one gets reported
  // immediately, as we can't even initialize the sockaddr with the given
  // name.
  do_check_throws_nsIException(() => socketTransportService.createUnixDomainTransport(socketName),
                               "NS_ERROR_FILE_NAME_TOO_LONG");

  run_next_test();
}

// Try creating a socket in a directory that doesn't exist.
function test_no_directory()
{
  let socketName = do_get_tempdir();
  socketName.append('directory-that-does-not-exist');
  socketName.append('socket');

  do_check_throws_nsIException(() => new UnixServerSocket(socketName, 0, -1),
                               "NS_ERROR_FILE_NOT_FOUND");

  run_next_test();
}

// Try connecting to a server socket that isn't there.
function test_no_such_socket()
{
  let socketName = do_get_tempdir();
  socketName.append('nonexistent-socket');

  let client = socketTransportService.createUnixDomainTransport(socketName);
  let clientAsyncInput = client.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
  clientAsyncInput.asyncWait(function (aStream) {
    do_print("called test_no_such_socket's onInputStreamReady");

    Assert.equal(aStream, clientAsyncInput);

    // nsISocketTransport puts off actually creating sockets as long as
    // possible, so the error in connecting doesn't actually show up until
    // this point.
    do_check_throws_nsIException(() => clientAsyncInput.available(),
                                 "NS_ERROR_FILE_NOT_FOUND");

    clientAsyncInput.close();
    client.close(Cr.NS_OK);

    run_next_test();
  }, 0, 0, threadManager.currentThread);
}

// Creating a socket with a name that another socket is already using is an
// error.
function test_address_in_use()
{
  let socketName = do_get_tempdir();
  socketName.append('socket-in-use');

  // Create one server socket.
  let server = new UnixServerSocket(socketName, allPermissions, -1);

  // Now try to create another with the same name.
  do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                               "NS_ERROR_SOCKET_ADDRESS_IN_USE");

  run_next_test();
}

// Creating a socket with a name that is already a file is an error.
function test_file_in_way()
{
  let socketName = do_get_tempdir();
  socketName.append('file_in_way');

  // Create a file with the given name.
  socketName.create(Ci.nsIFile.NORMAL_FILE_TYPE, allPermissions);

  // Try to create a socket with the same name.
  do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                               "NS_ERROR_SOCKET_ADDRESS_IN_USE");

  // Try to create a socket under a name that uses that as a parent directory.
  socketName.append('socket');
  do_check_throws_nsIException(() => new UnixServerSocket(socketName, 0, -1),
                               "NS_ERROR_FILE_NOT_DIRECTORY");

  run_next_test();
}

// It is not permitted to create a socket in a directory which we are not
// permitted to execute, or create files in.
function test_create_permission()
{
  let dirName = do_get_tempdir();
  dirName.append('unfriendly');

  let socketName = dirName.clone();
  socketName.append('socket');

  // The test harness has difficulty cleaning things up if we don't make
  // everything writable before we're done.
  try {
    // Create a directory which we are not permitted to search.
    dirName.create(Ci.nsIFile.DIRECTORY_TYPE, 0);

    // Try to create a socket in that directory. Because Linux returns EACCES
    // when a 'connect' fails because of a local firewall rule,
    // nsIServerSocket returns NS_ERROR_CONNECTION_REFUSED in this case.
    do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                                 "NS_ERROR_CONNECTION_REFUSED");

    // Grant read and execute permission, but not write permission on the directory.
    dirName.permissions = parseInt("0555", 8);

    // This should also fail; we need write permission.
    do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                                 "NS_ERROR_CONNECTION_REFUSED");

  } finally {
    // Make the directory writable, so the test harness can clean it up.
    dirName.permissions = allPermissions;
  }

  // This should succeed, since we now have all the permissions on the
  // directory we could want.
  do_check_instanceof(new UnixServerSocket(socketName, allPermissions, -1),
                      Ci.nsIServerSocket);

  run_next_test();
}

// To connect to a Unix domain socket, we need search permission on the
// directories containing it, and some kind of permission or other on the
// socket itself.
function test_connect_permission()
{
  // This test involves a lot of callbacks, but they're written out so that
  // the actual control flow proceeds from top to bottom.
  let log = '';

  // Create a directory which we are permitted to search - at first.
  let dirName = do_get_tempdir();
  dirName.append('inhospitable');
  dirName.create(Ci.nsIFile.DIRECTORY_TYPE, allPermissions);

  let socketName = dirName.clone();
  socketName.append('socket');

  // Create a server socket in that directory, listening for connections,
  // and accessible.
  let server = new UnixServerSocket(socketName, allPermissions, -1);
  server.asyncListen({ onSocketAccepted: socketAccepted, onStopListening: stopListening });

  // Make the directory unsearchable.
  dirName.permissions = 0;

  let client3;

  let client1 = socketTransportService.createUnixDomainTransport(socketName);
  let client1AsyncInput = client1.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
  client1AsyncInput.asyncWait(function (aStream) {
    do_print("called test_connect_permission's client1's onInputStreamReady");
    log += '1';

    // nsISocketTransport puts off actually creating sockets as long as
    // possible, so the error doesn't actually show up until this point.
    do_check_throws_nsIException(() => client1AsyncInput.available(),
                                 "NS_ERROR_CONNECTION_REFUSED");

    client1AsyncInput.close();
    client1.close(Cr.NS_OK);

    // Make the directory searchable, but make the socket inaccessible.
    dirName.permissions = allPermissions;
    socketName.permissions = 0;

    let client2 = socketTransportService.createUnixDomainTransport(socketName);
    let client2AsyncInput = client2.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
    client2AsyncInput.asyncWait(function (aStream) {
      do_print("called test_connect_permission's client2's onInputStreamReady");
      log += '2';

      do_check_throws_nsIException(() => client2AsyncInput.available(),
                                   "NS_ERROR_CONNECTION_REFUSED");

      client2AsyncInput.close();
      client2.close(Cr.NS_OK);

      // Now make everything accessible, and try one last time.
      socketName.permissions = allPermissions;

      client3 = socketTransportService.createUnixDomainTransport(socketName);

      let client3Output = client3.openOutputStream(0, 0, 0);
      client3Output.write("Hanratty", 8);

      let client3AsyncInput = client3.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
      client3AsyncInput.asyncWait(client3InputStreamReady, 0, 0, threadManager.currentThread);
    }, 0, 0, threadManager.currentThread);
  }, 0, 0, threadManager.currentThread);

  function socketAccepted(aServ, aTransport) {
    do_print("called test_connect_permission's onSocketAccepted");
    log += 'a';

    let serverInput = aTransport.openInputStream(0, 0, 0).QueryInterface(Ci.nsIAsyncInputStream);
    let serverOutput = aTransport.openOutputStream(0, 0, 0);

    serverInput.asyncWait(function (aStream) {
      do_print("called test_connect_permission's socketAccepted's onInputStreamReady");
      log += 'i';

      // Receive data from the client, and send back a response.
      let serverScriptableInput = new ScriptableInputStream(serverInput);
      Assert.equal(serverScriptableInput.readBytes(8), "Hanratty");
      serverOutput.write("Ferlingatti", 11);
    }, 0, 0, threadManager.currentThread);
  }

  function client3InputStreamReady(aStream) {
    do_print("called client3's onInputStreamReady");
    log += '3';

    let client3Input = new ScriptableInputStream(aStream);

    Assert.equal(client3Input.readBytes(11), "Ferlingatti");

    client3.close(Cr.NS_OK);
    server.close();
  }

  function stopListening(aServ, aStatus) {
    do_print("called test_connect_permission's server's stopListening");
    log += 's';

    Assert.equal(log, '12ai3s');

    run_next_test();
  }
}

// Creating a socket with a long filename doesn't crash.
function test_long_socket_name()
{
  let socketName = do_get_tempdir();
  socketName.append(new Array(10000).join('long'));

  // Try to create a server socket with the long name.
  do_check_throws_nsIException(() => new UnixServerSocket(socketName, allPermissions, -1),
                               "NS_ERROR_FILE_NAME_TOO_LONG");

  // Try to connect to a socket with the long name.
  do_check_throws_nsIException(() => socketTransportService.createUnixDomainTransport(socketName),
                               "NS_ERROR_FILE_NAME_TOO_LONG");

  run_next_test();
}

// Going offline should not shut down Unix domain sockets.
function test_keep_when_offline()
{
  let log = '';

  let socketName = do_get_tempdir();
  socketName.append('keep-when-offline');

  // Create a listening socket.
  let listener = new UnixServerSocket(socketName, allPermissions, -1);
  listener.asyncListen({ onSocketAccepted: onAccepted, onStopListening: onStopListening });

  // Connect a client socket to the listening socket.
  let client = socketTransportService.createUnixDomainTransport(socketName);
  let clientOutput = client.openOutputStream(0, 0, 0);
  let clientInput = client.openInputStream(0, 0, 0);
  clientInput.asyncWait(clientReady, 0, 0, threadManager.currentThread);
  let clientScriptableInput = new ScriptableInputStream(clientInput);

  let server, serverInput, serverScriptableInput, serverOutput;

  // How many times has the server invited the client to go first?
  let count = 0;

  // The server accepted connection callback.
  function onAccepted(aListener, aServer) {
    do_print("test_keep_when_offline: onAccepted called");
    log += 'a';
    Assert.equal(aListener, listener);
    server = aServer;

    // Prepare to receive messages from the client.
    serverInput = server.openInputStream(0, 0, 0);
    serverInput.asyncWait(serverReady, 0, 0, threadManager.currentThread);
    serverScriptableInput = new ScriptableInputStream(serverInput);

    // Start a conversation with the client.
    serverOutput = server.openOutputStream(0, 0, 0);
    serverOutput.write("After you, Alphonse!", 20);
    count++;
  }

  // The client has seen its end of the socket close.
  function clientReady(aStream) {
    log += 'c';
    do_print("test_keep_when_offline: clientReady called: " + log);
    Assert.equal(aStream, clientInput);

    // If the connection has been closed, end the conversation and stop listening.
    let available;
    try {
      available = clientInput.available();
    } catch (ex) {
      do_check_instanceof(ex, Ci.nsIException);
      Assert.equal(ex.result, Cr.NS_BASE_STREAM_CLOSED);

      do_print("client received end-of-stream; closing client output stream");
      log += ')';

      client.close(Cr.NS_OK);

      // Now both output streams have been closed, and both input streams
      // have received the close notification. Stop listening for
      // connections.
      listener.close();
    }

    if (available) {
      // Check the message from the server.
      Assert.equal(clientScriptableInput.readBytes(20), "After you, Alphonse!");

      // Write our response to the server.
      clientOutput.write("No, after you, Gaston!", 22);

      // Ask to be called again, when more input arrives.
      clientInput.asyncWait(clientReady, 0, 0, threadManager.currentThread);
    }
  }

  function serverReady(aStream) {
    log += 's';
    do_print("test_keep_when_offline: serverReady called: " + log);
    Assert.equal(aStream, serverInput);

    // Check the message from the client.
    Assert.equal(serverScriptableInput.readBytes(22), "No, after you, Gaston!");

    // This should not shut things down: Unix domain sockets should
    // remain open in offline mode.
    if (count == 5) {
      IOService.offline = true;
      log += 'o';
    }

    if (count < 10) {
      // Insist.
      serverOutput.write("After you, Alphonse!", 20);
      count++;

      // As long as the input stream is open, always ask to be called again
      // when more input arrives.
      serverInput.asyncWait(serverReady, 0, 0, threadManager.currentThread);
    } else if (count == 10) {
      // After sending ten times and receiving ten replies, we're not
      // going to send any more. Close the server's output stream; the
      // client's input stream should see this.
      do_print("closing server transport");
      server.close(Cr.NS_OK);
      log += '(';
    }
  }

  // We have stopped listening.
  function onStopListening(aServ, aStatus) {
    do_print("test_keep_when_offline: onStopListening called");
    log += 'L';
    Assert.equal(log, 'acscscscscsocscscscscs(c)L');

    Assert.equal(aServ, listener);
    Assert.equal(aStatus, Cr.NS_BINDING_ABORTED);

    run_next_test();
  }
}
