var rawData = new Uint8Array([65,66,67,68]);
var data = String.fromCharCode.apply(null, rawData);

function UDPSocketListener(){}

UDPSocketListener.prototype = {
  QueryInterface : XPCOMUtils.generateQI([Ci.nsIUDPSocketListener]),

  onPacketReceived : function(aSocket, aMessage){
    var mData = String.fromCharCode.apply(null, aMessage.rawData);
    do_check_eq(mData, data);
    do_check_eq(mData, aMessage.data);
    do_test_finished();
  },

  onStopListening: function(aSocket, aStatus){}
};


function run_test(){
  var socket = Cc["@mozilla.org/network/udp-socket;1"].createInstance(Ci.nsIUDPSocket);

  socket.init(-1, true);
  do_print("Port assigned : " + socket.port);
  socket.asyncListen(new UDPSocketListener());

  var written = socket.send("127.0.0.1", socket.port, rawData, rawData.length);
  do_check_eq(written, data.length);
  do_test_pending();
}

