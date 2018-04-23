/* -*- Mode: Javascript; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

const HELLO_WORLD = "Hello World";
const EMPTY_MESSAGE = "";

add_test(function test_udp_message_raw_data() {
  info("test for nsIUDPMessage.rawData");

  let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true, Services.scriptSecurityManager.getSystemPrincipal());
  info("Port assigned : " + socket.port);
  socket.asyncListen({
    QueryInterface : ChromeUtils.generateQI([Ci.nsIUDPSocketListener]),
    onPacketReceived : function(aSocket, aMessage){
      let recv_data = String.fromCharCode.apply(null, aMessage.rawData);
      Assert.equal(recv_data, HELLO_WORLD);
      Assert.equal(recv_data, aMessage.data);
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
  Assert.equal(written, HELLO_WORLD.length);
});

add_test(function test_udp_send_stream() {
  info("test for nsIUDPSocket.sendBinaryStream");

  let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true, Services.scriptSecurityManager.getSystemPrincipal());
  socket.asyncListen({
    QueryInterface : ChromeUtils.generateQI([Ci.nsIUDPSocketListener]),
    onPacketReceived : function(aSocket, aMessage){
      let recv_data = String.fromCharCode.apply(null, aMessage.rawData);
      Assert.equal(recv_data, HELLO_WORLD);
      socket.close();
      run_next_test();
    },
    onStopListening: function(aSocket, aStatus){}
  });

  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
  stream.setData(HELLO_WORLD, HELLO_WORLD.length);
  socket.sendBinaryStream("127.0.0.1", socket.port, stream);
});

add_test(function test_udp_message_zero_length() {
  info("test for nsIUDPMessage with zero length");

  let socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true, Services.scriptSecurityManager.getSystemPrincipal());
  info("Port assigned : " + socket.port);
  socket.asyncListen({
    QueryInterface : ChromeUtils.generateQI([Ci.nsIUDPSocketListener]),
    onPacketReceived : function(aSocket, aMessage){
      let recv_data = String.fromCharCode.apply(null, aMessage.rawData);
      Assert.equal(recv_data, EMPTY_MESSAGE);
      Assert.equal(recv_data, aMessage.data);
      socket.close();
      run_next_test();
    },
    onStopListening: function(aSocket, aStatus){}
  });

  let rawData = new Uint8Array(EMPTY_MESSAGE.length);
  let written = socket.send("127.0.0.1", socket.port, rawData, rawData.length);
  Assert.equal(written, EMPTY_MESSAGE.length);
});

function run_test(){
  run_next_test();
}

