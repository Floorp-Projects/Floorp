const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const CC = Components.Constructor;

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

function TestServer() {
  this.reset();

  // start server.
  // any port (-1), loopback only (true), default backlog (-1)
  this.listener = ServerSocket(-1, true, -1);
  this.port = this.listener.port;
  print('server: listening on', this.port);
  this.listener.asyncListen(this);
}

TestServer.prototype = {
  onSocketAccepted: function(socket, trans) {
    print('server: got client connection');

    // one connection at a time.
    if (this.input !== null) {
      do_throw("Test written to handle one connection at a time");
      socket.close();
      return;
    }

    this.input = trans.openInputStream(0, 0, 0);
    this.output = trans.openOutputStream(0, 0, 0);
    this.selfAddr = trans.getScriptableSelfAddr();
    this.peerAddr = trans.getScriptablePeerAddr();

    this.acceptCallback();

    this.reset();
  } ,
  
  onStopListening: function(socket) {} ,

  reset: function() {
    print('server: reset');
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

  stop: function() {
    this.reset();
    try { this.listener.close(); } catch(ignore) {}
  }
};

var sts, serv;

function checkAddrEqual(lhs, rhs) {
  do_check_eq(lhs.family, rhs.family);

  if (lhs.family === Ci.nsINetAddr.FAMILY_INET) {
    do_check_eq(lhs.address, rhs.address);
    do_check_eq(lhs.port, rhs.port);
  } else if (lhs.family === Ci.nsINetAddr.FAMILY_IPV6) {
    do_check_eq(lhs.address, rhs.address);
    do_check_eq(lhs.port, rhs.port);
    do_check_eq(lhs.flow, rhs.flow);
    do_check_eq(lhs.scope, rhs.scope);
  }
}

function testIpv4() {
  var transport;

  serv.acceptCallback = function() {
    var selfAddr = transport.getScriptableSelfAddr();
    var peerAddr = transport.getScriptablePeerAddr();

    // check peerAddr against expected values
    do_check_eq(peerAddr.family, Ci.nsINetAddr.FAMILY_INET);
    do_check_eq(peerAddr.port, transport.port);
    do_check_eq(peerAddr.port, serv.port);
    do_check_eq(peerAddr.address, "127.0.0.1");

    // check selfAddr against expected values
    do_check_eq(selfAddr.family, Ci.nsINetAddr.FAMILY_INET);
    do_check_eq(selfAddr.address, "127.0.0.1");

    // check that selfAddr = server.peerAddr and vice versa.
    checkAddrEqual(selfAddr, serv.peerAddr);
    checkAddrEqual(peerAddr, serv.selfAddr);

    do_execute_soon(run_next_test);
  };

  transport = sts.createTransport(null, 0, '127.0.0.1', serv.port, null);
  transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING,0,0);
}

function testIpv6() {
  var transport;

  serv.acceptCallback = function() {
    var selfAddr = transport.getScriptableSelfAddr();
    var peerAddr = transport.getScriptablePeerAddr();

    // check peerAddr against expected values
    do_check_eq(peerAddr.family, Ci.nsINetAddr.FAMILY_INET6);
    do_check_eq(peerAddr.port, transport.port);
    do_check_eq(peerAddr.port, serv.port);
    // peerAddr is specifically requested as ::1 when connecting.
    do_check_eq(peerAddr.address, "::1");

    // check selfAddr against expected values
    do_check_eq(selfAddr.family, Ci.nsINetAddr.FAMILY_INET6);

    /* XXX: There are differences in the results here depending on whether the
     * environment prefers to interpret ipv6 addresses as compatible ipv4
     * address.
     *
     * do_check_true(selfAddr.address === "::1");
     *    - selfAddr.address might be "::ffff:127.0.0.1"
     *
     * checkAddrEqual(peerAddr, serv.selfAddr);
     *    - serv.selfAddr.family may be FAMILY_INET
     *    - serv.selfAddr.address might be any of:
     *          INET:  "127.0.0.1"
     *          INET6: "::f", "::ffff:127.0.0.1"
     *    - the ports should match.
     *
     * checkAddrEqual(selfAddr, serv.peerAddr);
     *    - serv.peerAddr.family might be FAMILY_INET
     *    - serv.peerAddr.address might be any of:
     *          INET:  "127.0.0.1"
     *          INET6: "::f", "::ffff:127.0.0.1"
     *    - the ports should match.
     */
    do_check_eq(peerAddr.port, serv.selfAddr.port);
    do_check_eq(selfAddr.port, serv.peerAddr.port);

    do_execute_soon(run_next_test);
  };

  transport = sts.createTransport(null, 0, '::1', serv.port, null);
  transport.openOutputStream(Ci.nsITransport.OPEN_BLOCKING,0,0);
}


function run_test() {
  sts = Cc["@mozilla.org/network/socket-transport-service;1"]
            .getService(Ci.nsISocketTransportService);

  serv = new TestServer();
  transport = null;

  add_test(testIpv4);
  add_test(testIpv6);
  // XXX: no testing for local / unix
  run_next_test();
}
