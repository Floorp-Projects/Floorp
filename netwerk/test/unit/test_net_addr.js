const CC = Components.Constructor;

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

/**
 * TestServer: A single instance of this is created as |serv|.  When created,
 * it starts listening on the loopback address on port |serv.port|.  Tests will
 * connect to it after setting |serv.acceptCallback|, which is invoked after it
 * accepts a connection.
 *
 * Within |serv.acceptCallback|, various properties of |serv| can be used to
 * run checks. After the callback, the connection is closed, but the server
 * remains listening until |serv.stop|
 *
 * Note: TestServer can only handle a single connection at a time.  Tests
 * should use run_next_test at the end of |serv.acceptCallback| to start the
 * following test which creates a connection.
 */
function TestServer() {
  this.reset();

  // start server.
  // any port (-1), loopback only (true), default backlog (-1)
  this.listener = ServerSocket(-1, true, -1);
  this.port = this.listener.port;
  do_print('server: listening on ' + this.port);
  this.listener.asyncListen(this);
}

TestServer.prototype = {
  onSocketAccepted: function(socket, trans) {
    do_print('server: got client connection');

    // one connection at a time.
    if (this.input !== null) {
      try { socket.close(); } catch(ignore) {}
      do_throw("Test written to handle one connection at a time.");
    }

    try {
      this.input = trans.openInputStream(0, 0, 0);
      this.output = trans.openOutputStream(0, 0, 0);
      this.selfAddr = trans.getScriptableSelfAddr();
      this.peerAddr = trans.getScriptablePeerAddr();

      this.acceptCallback();
    } catch(e) {
      /* In a native callback such as onSocketAccepted, exceptions might not
       * get output correctly or logged to test output. Send them through
       * do_throw, which fails the test immediately. */
      do_report_unexpected_exception(e, "in TestServer.onSocketAccepted");
    }

    this.reset();
  } ,
  
  onStopListening: function(socket) {} ,

  /**
   * Called to close a connection and clean up properties.
   */
  reset: function() {
    if (this.input)
      try { this.input.close(); } catch(ignore) {}
    if (this.output)
      try { this.output.close(); } catch(ignore) {}

    this.input = null;
    this.output = null;
    this.acceptCallback = null;
    this.selfAddr = null;
    this.peerAddr = null;
  } ,

  /**
   * Cleanup for TestServer and this test case.
   */
  stop: function() {
    this.reset();
    try { this.listener.close(); } catch(ignore) {}
  }
};


/**
 * Helper function.
 * Compares two nsINetAddr objects and ensures they are logically equivalent.
 */
function checkAddrEqual(lhs, rhs) {
  do_check_eq(lhs.family, rhs.family);

  if (lhs.family === Ci.nsINetAddr.FAMILY_INET) {
    do_check_eq(lhs.address, rhs.address);
    do_check_eq(lhs.port, rhs.port);
  }
  
  /* TODO: fully support ipv6 and local */
}


/**
 * An instance of SocketTransportService, used to create connections.
 */
var sts;

/**
 * Single instance of TestServer
 */
var serv;

/**
 * Connections have 5 seconds to be made, or a timeout function fails this
 * test.  This prevents the test from hanging and bringing down the entire
 * xpcshell test chain.
 */
var connectTimeout = 5*1000;

/**
 * A place for individual tests to place Objects of importance for access
 * throughout asynchronous testing.  Particularly important for any output or
 * input streams opened, as cleanup of those objects (by the garbage collector)
 * causes the stream to close and may have other side effects.
 */
var testDataStore = null;

/**
 * IPv4 test.
 */
function testIpv4() {
  testDataStore = {
    transport : null ,
    ouput : null
  }

  serv.acceptCallback = function() {
    // disable the timeoutCallback
    serv.timeoutCallback = function(){};

    var selfAddr = testDataStore.transport.getScriptableSelfAddr();
    var peerAddr = testDataStore.transport.getScriptablePeerAddr();

    // check peerAddr against expected values
    do_check_eq(peerAddr.family, Ci.nsINetAddr.FAMILY_INET);
    do_check_eq(peerAddr.port, testDataStore.transport.port);
    do_check_eq(peerAddr.port, serv.port);
    do_check_eq(peerAddr.address, "127.0.0.1");

    // check selfAddr against expected values
    do_check_eq(selfAddr.family, Ci.nsINetAddr.FAMILY_INET);
    do_check_eq(selfAddr.address, "127.0.0.1");

    // check that selfAddr = server.peerAddr and vice versa.
    checkAddrEqual(selfAddr, serv.peerAddr);
    checkAddrEqual(peerAddr, serv.selfAddr);

    testDataStore = null;
    do_execute_soon(run_next_test);
  };

  // Useful timeout for debugging test hangs
  /*serv.timeoutCallback = function(tname) {
    if (tname === 'testIpv4')
      do_throw('testIpv4 never completed a connection to TestServ');
  };
  do_timeout(connectTimeout, function(){ serv.timeoutCallback('testIpv4'); });*/

  testDataStore.transport = sts.createTransport(null, 0, '127.0.0.1', serv.port, null);
  /*
   * Need to hold |output| so that the output stream doesn't close itself and
   * the associated connection.
   */
  testDataStore.output = testDataStore.transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING,0,0);

  /* NEXT:
   * openOutputStream -> onSocketAccepted -> acceptedCallback -> run_next_test
   * OR (if the above timeout is uncommented)
   * <connectTimeout lapses> -> timeoutCallback -> do_throw
   */
}


/**
 * Running the tests.
 */
function run_test() {
  sts = Cc["@mozilla.org/network/socket-transport-service;1"]
            .getService(Ci.nsISocketTransportService);
  serv = new TestServer();

  do_register_cleanup(function(){ serv.stop(); });

  add_test(testIpv4);
  /* TODO: testIpv6 */
  /* TODO: testLocal */
    
  run_next_test();
}
