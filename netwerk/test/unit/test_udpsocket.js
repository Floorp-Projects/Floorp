/* -*- Mode: Javascript; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

Cu.import("resource://gre/modules/Services.jsm");

const HELLO_WORLD = "Hello World";

add_test(function test_udp_message_raw_data() {
  do_print("test for nsIUDPMessage.rawData");

  let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true, Services.scriptSecurityManager.getSystemPrincipal());
  do_print("Port assigned : " + socket.port);
  socket.asyncListen({
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIUDPSocketListener]),
    onPacketReceived : function(aSocket, aMessage){
      let recv_data = String.fromCharCode.apply(null, aMessage.rawData);
      do_check_eq(recv_data, HELLO_WORLD);
      do_check_eq(recv_data, aMessage.data);
      socket.close();
      run_next_test();
    },
    onStopListening: function(aSocket, aStatus){}
  });

  let rawData = new Uint8Array(HELLO_WORLD.length);
  for (let i = 0; i < HELLO_WORLD.length; i++) {
    rawData[i] = HELLO_WORLD.charCodeAt(i);
  }
  let written = socket.send("127.0.0.1", socket.port, rawData, rawData.length);
  do_check_eq(written, HELLO_WORLD.length);
});

add_test(function test_udp_send_stream() {
  do_print("test for nsIUDPSocket.sendBinaryStream");

  let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true, Services.scriptSecurityManager.getSystemPrincipal());
  socket.asyncListen({
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIUDPSocketListener]),
    onPacketReceived : function(aSocket, aMessage){
      let recv_data = String.fromCharCode.apply(null, aMessage.rawData);
      do_check_eq(recv_data, HELLO_WORLD);
      socket.close();
      run_next_test();
    },
    onStopListening: function(aSocket, aStatus){}
  });

  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
  stream.setData(HELLO_WORLD, HELLO_WORLD.length);
  socket.sendBinaryStream("127.0.0.1", socket.port, stream);
});

function run_test(){
  run_next_test();
}

