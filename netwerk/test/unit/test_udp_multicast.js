// Bug 960397: UDP multicast options

const { Constructor: CC } = Components;

const UDPSocket = CC("@mozilla.org/network/udp-socket;1",
                     "nsIUDPSocket",
                     "init");
const { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

const ADDRESS = "224.0.0.255";
const TIMEOUT = 2000;

const ua = Cc["@mozilla.org/network/protocol;1?name=http"]
           .getService(Ci.nsIHttpProtocolHandler).userAgent;
const isWinXP = ua.indexOf("Windows NT 5.1") != -1;

let gConverter;

function run_test() {
  setup();
  run_next_test();
}

function setup() {
  gConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
               createInstance(Ci.nsIScriptableUnicodeConverter);
  gConverter.charset = "utf8";
}

function createSocketAndJoin() {
  let socket = new UDPSocket(-1, false);
  socket.joinMulticast(ADDRESS);
  return socket;
}

function sendPing(socket) {
  let ping = "ping";
  let rawPing = gConverter.convertToByteArray(ping);

  let deferred = promise.defer();

  socket.asyncListen({
    onPacketReceived: function(s, message) {
      do_print("Received on port " + socket.port);
      do_check_eq(message.data, ping);
      socket.close();
      deferred.resolve(message.data);
    },
    onStopListening: function(socket, status) {}
  });

  do_print("Multicast send to port " + socket.port);
  socket.send(ADDRESS, socket.port, rawPing, rawPing.length);

  // Timers are bad, but it seems like the only way to test *not* getting a
  // packet.
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(() => {
    socket.close();
    deferred.reject();
  }, TIMEOUT, Ci.nsITimer.TYPE_ONE_SHOT);

  return deferred.promise;
}

add_test(() => {
  do_print("Joining multicast group");
  let socket = createSocketAndJoin();
  sendPing(socket).then(
    run_next_test,
    () => do_throw("Joined group, but no packet received")
  );
});

add_test(() => {
  do_print("Disabling multicast loopback");
  let socket = createSocketAndJoin();
  socket.multicastLoopback = false;
  sendPing(socket).then(
    () => do_throw("Loopback disabled, but still got a packet"),
    run_next_test
  );
});

// The following multicast interface test doesn't work on Windows XP, as it
// appears to allow packets no matter what address is given, so we'll skip the
// test there.
if (!isWinXP) {
  add_test(() => {
    do_print("Changing multicast interface");
    let socket = createSocketAndJoin();
    socket.multicastInterface = "127.0.0.1";
    sendPing(socket).then(
      () => do_throw("Changed interface, but still got a packet"),
      run_next_test
    );
  });
}

add_test(() => {
  do_print("Leaving multicast group");
  let socket = createSocketAndJoin();
  socket.leaveMulticast(ADDRESS);
  sendPing(socket).then(
    () => do_throw("Left group, but still got a packet"),
    run_next_test
  );
});
