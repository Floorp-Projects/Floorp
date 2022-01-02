// Bug 960397: UDP multicast options
"use strict";

var { Constructor: CC } = Components;

const UDPSocket = CC(
  "@mozilla.org/network/udp-socket;1",
  "nsIUDPSocket",
  "init"
);
const ADDRESS_TEST1 = "224.0.0.200";
const ADDRESS_TEST2 = "224.0.0.201";
const ADDRESS_TEST3 = "224.0.0.202";
const ADDRESS_TEST4 = "224.0.0.203";

const TIMEOUT = 2000;

const ua = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
  Ci.nsIHttpProtocolHandler
).userAgent;

var gConverter;

function run_test() {
  setup();
  run_next_test();
}

function setup() {
  gConverter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  gConverter.charset = "utf8";
}

function createSocketAndJoin(addr) {
  let socket = new UDPSocket(
    -1,
    false,
    Services.scriptSecurityManager.getSystemPrincipal()
  );
  socket.joinMulticast(addr);
  return socket;
}

function sendPing(socket, addr) {
  let ping = "ping";
  let rawPing = gConverter.convertToByteArray(ping);

  return new Promise((resolve, reject) => {
    socket.asyncListen({
      onPacketReceived(s, message) {
        info("Received on port " + socket.port);
        Assert.equal(message.data, ping);
        socket.close();
        resolve(message.data);
      },
      onStopListening(socket, status) {},
    });

    info("Multicast send to port " + socket.port);
    socket.send(addr, socket.port, rawPing, rawPing.length);

    // Timers are bad, but it seems like the only way to test *not* getting a
    // packet.
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      () => {
        socket.close();
        reject();
      },
      TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  });
}

add_test(() => {
  info("Joining multicast group");
  let socket = createSocketAndJoin(ADDRESS_TEST1);
  sendPing(socket, ADDRESS_TEST1).then(run_next_test, () =>
    do_throw("Joined group, but no packet received")
  );
});

add_test(() => {
  info("Disabling multicast loopback");
  let socket = createSocketAndJoin(ADDRESS_TEST2);
  socket.multicastLoopback = false;
  sendPing(socket, ADDRESS_TEST2).then(
    () => do_throw("Loopback disabled, but still got a packet"),
    run_next_test
  );
});

add_test(() => {
  info("Changing multicast interface");
  let socket = createSocketAndJoin(ADDRESS_TEST3);
  socket.multicastInterface = "127.0.0.1";
  sendPing(socket, ADDRESS_TEST3).then(
    () => do_throw("Changed interface, but still got a packet"),
    run_next_test
  );
});

add_test(() => {
  info("Leaving multicast group");
  let socket = createSocketAndJoin(ADDRESS_TEST4);
  socket.leaveMulticast(ADDRESS_TEST4);
  sendPing(socket, ADDRESS_TEST4).then(
    () => do_throw("Left group, but still got a packet"),
    run_next_test
  );
});
