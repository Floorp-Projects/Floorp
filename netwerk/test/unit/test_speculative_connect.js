const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");

var serv;

function TestServer() {
    this.listener = ServerSocket(-1, true, -1);
    this.listener.asyncListen(this);
}

TestServer.prototype = {
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsIServerSocket) ||
            iid.equals(Ci.nsISupports))
            return this;
        throw Components.results.NS_ERROR_NO_INTERFACE;
    },
    onSocketAccepted: function(socket, trans) {
        try { this.listener.close(); } catch(e) {}
        do_check_true(true);
        do_test_finished();
    },

    onStopListening: function(socket) {}
}

function run_test() {
    var ios = Cc["@mozilla.org/network/io-service;1"]
        .getService(Ci.nsIIOService);    

    serv = new TestServer();
    URI = ios.newURI("http://localhost:" + serv.listener.port + "/just/a/test", null, null);
    ios.QueryInterface(Components.interfaces.nsISpeculativeConnect)
        .speculativeConnect(URI, null);
    do_test_pending();
}

