// vim:set sw=2 ts=2 sts=2 cin:

function nsTestServ(port) {
  dump(">>> creating nsTestServ instance\n");
  this.port = port;
}

nsTestServ.prototype =
{
  handler: {
    // This function generates standard headers for a specific HTTP result
    // code. The code can include text like "Not Found". The return value does
    // not include a terminating empty line, so new headers can be added after
    // it.
    headers: function(code) {
      return "HTTP/1.0 " + code + "\r\n" +
             "Connection: close\r\n" +
             "Server: Necko unit test server\r\n" +
             "Content-Type: text/plain\r\n";
    },

    // This object contains the handlers for the various request URIs
    // numeric properties are HTTP result codes
    400: function(stream)
    {
      var response = this.headers("400 Bad Request") +
                     "\r\n" +
                     "Bad request";
      stream.write(response, response.length);
    },
    404: function(stream)
    {
      var response = this.headers("404 Not Found") +
                     "\r\n" +
                     "No such path";
      stream.write(response, response.length);
    },

    "/": function(stream)
    {
      var response = this.headers("200 OK") +
                     "\r\n" +
                     "200 OK";
      stream.write(response, response.length);
    }
  },

  QueryInterface: function(iid)
  {
    if (iid.equals(Components.interfaces.nsIServerSocketListener) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  /* this function is called when we receive a new connection */
  onSocketAccepted: function(serverSocket, clientSocket)
  {
    dump(">>> accepted connection on "+clientSocket.host+":"+clientSocket.port+"\n");

    const nsITransport = Components.interfaces.nsITransport;
    var input = clientSocket.openInputStream(nsITransport.OPEN_BLOCKING, 0, 0);
    var output = clientSocket.openOutputStream(nsITransport.OPEN_BLOCKING, 0, 0);

    var request = this.parseInput(input);

    // Strip away query parameters, then unescape the request
    var path = request.replace(/\?.*/, "");
    try {
      path = decodeURI(path);
    } catch (ex) {
      path = 400;
    }

    if (path in this.handler)
      this.handler[path](output);
    else
      this.handler[404](output);

    input.close();
    output.close();
  },

  onStopListening: function(serverSocket, status)
  {
    dump(">>> shutting down server socket\n");
  },

  startListening: function()
  {
    const nsIServerSocket = Components.interfaces.nsIServerSocket;
    const SERVERSOCKET_CONTRACTID = "@mozilla.org/network/server-socket;1";
    var socket = Components.classes[SERVERSOCKET_CONTRACTID].createInstance(nsIServerSocket);
    socket.init(this.port, true /* loopback only */, 5);
    dump(">>> listening on port "+socket.port+"\n");
    socket.asyncListen(this);
  },

  parseInput: function(input)
  {
    // We read the input line by line. The first line tells us the requested
    // path.
    var is = Components.classes["@mozilla.org/intl/converter-input-stream;1"]
                       .createInstance(Components.interfaces.nsIConverterInputStream);
    is.init(input, "ISO-8859-1", 1024, 0xFFFD);

    var lis =
      is.QueryInterface(Components.interfaces.nsIUnicharLineInputStream);
    var line = {};
    var cont = lis.readLine(line);

    var request = line.value.split(/ +/);
    if (request[0] != "GET" && request[0] != "POST")
      return 400;

    // This server doesn't support HTTP 0.9
    if (request[2] != "HTTP/1.0" && request[2] != "HTTP/1.1")
      return 400;

    while (line.value != "") {
      lis.readLine(line);
    }

    if (request[1][0] != "/")
      return 400;

    return request[1];
  }
}



function start_server(port) {
  new nsTestServ(port).startListening();
}
