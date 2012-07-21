const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const CC = Components.Constructor;

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init");
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                           "nsIBinaryInputStream",
                           "setInputStream");
const DirectoryService = CC("@mozilla.org/file/directory_service;1",
                            "nsIProperties");
const Process = CC("@mozilla.org/process/util;1", "nsIProcess", "init");

const currentThread = Cc["@mozilla.org/thread-manager;1"]
                      .getService().currentThread;

var socks_test_server = null;
var socks_listen_port = -1;

function getAvailableBytes(input)
{
  var len = 0;

  try {
    len = input.available();  
  } catch (e) {
  }
  
  return len;
}

function runScriptSubprocess(script, args)
{
  // logic copied from ted's crashreporter unit test
  var ds = new DirectoryService();
  var bin = ds.get("CurProcD", Ci.nsILocalFile);

  bin.append("xpcshell");
  if (!bin.exists()) {
    bin.leafName = "xpcshell.exe";
    do_check_true(bin.exists());
    if (!bin.exists())
      do_throw("Can't find xpcshell binary");
  }

  var script = do_get_file(script);
  var proc = new Process(bin);
  var args = [script.path].concat(args);

  proc.run(false, args, args.length);

  return proc;
}

function buf2ip(buf)
{
  if (buf.length == 16) {
    var ip = (buf[0]  << 4 | buf[1]).toString(16) + ':' +
             (buf[2]  << 4 | buf[3]).toString(16) + ':' +
             (buf[4]  << 4 | buf[5]).toString(16) + ':' +
             (buf[6]  << 4 | buf[7]).toString(16) + ':' +
             (buf[8]  << 4 | buf[9]).toString(16) + ':' +
             (buf[10] << 4 | buf[11]).toString(16) + ':' +
             (buf[12] << 4 | buf[13]).toString(16) + ':' +
             (buf[14] << 4 | buf[15]).toString(16);
    for (var i = 8; i >= 2; i--) {
      var re = new RegExp("(^|:)(0(:|$)){" + i + "}");
      var shortip = ip.replace(re, '::');
      if (shortip != ip) {
        return shortip;
      }
    }
    return ip;
  } else {
    return buf.join('.');
  }
}

function buf2int(buf)
{
  var n = 0;
  
  for (var i in buf) {
    n |= buf[i] << ((buf.length - i - 1) * 8);
  }

  return n;
}

function buf2str(buf)
{
  return String.fromCharCode.apply(null, buf);
}

const STATE_WAIT_GREETING = 1;
const STATE_WAIT_SOCKS4_REQUEST = 2;
const STATE_WAIT_SOCKS4_USERNAME = 3;
const STATE_WAIT_SOCKS4_HOSTNAME = 4;
const STATE_WAIT_SOCKS5_GREETING = 5;
const STATE_WAIT_SOCKS5_REQUEST = 6;
const STATE_WAIT_PONG = 7;
const STATE_GOT_PONG = 8;

function SocksClient(server, client_in, client_out)
{
  this.server = server;
  this.type = '';
  this.username = '';
  this.dest_name = '';
  this.dest_addr = [];
  this.dest_port = [];

  this.client_in = client_in;
  this.client_out = client_out;
  this.inbuf = [];
  this.outbuf = String();
  this.state = STATE_WAIT_GREETING;
  this.waitRead(this.client_in);
}
SocksClient.prototype = {
  onInputStreamReady: function(input)
  {
    var len = getAvailableBytes(input);

    if (len == 0) {
      print('server: client closed!');
      do_check_eq(this.state, STATE_GOT_PONG);
      this.server.testCompleted(this);
      return;
    }

    var bin = new BinaryInputStream(input);
    var data = bin.readByteArray(len);
    this.inbuf = this.inbuf.concat(data);

    switch (this.state) {
      case STATE_WAIT_GREETING:
        this.checkSocksGreeting();
        break;
      case STATE_WAIT_SOCKS4_REQUEST:
        this.checkSocks4Request();
        break;
      case STATE_WAIT_SOCKS4_USERNAME:
        this.checkSocks4Username();
        break;
      case STATE_WAIT_SOCKS4_HOSTNAME:
        this.checkSocks4Hostname();
        break;
      case STATE_WAIT_SOCKS5_GREETING:
        this.checkSocks5Greeting();
        break;
      case STATE_WAIT_SOCKS5_REQUEST:
        this.checkSocks5Request();
        break;
      case STATE_WAIT_PONG:
        this.checkPong();
        break;
      default:
        do_throw("server: read in invalid state!");
    }

    this.waitRead(input);
  },

  onOutputStreamReady: function(output)
  {
    var len = output.write(this.outbuf, this.outbuf.length);
    if (len != this.outbuf.length) {
      this.outbuf = this.outbuf.substring(len);
      this.waitWrite(output);
    } else
      this.outbuf = String();
  },

  waitRead: function(input)
  {
    input.asyncWait(this, 0, 0, currentThread);
  },

  waitWrite: function(output)
  {
    output.asyncWait(this, 0, 0, currentThread);
  },

  write: function(buf)
  {
    this.outbuf += buf;
    this.waitWrite(this.client_out);
  },

  checkSocksGreeting: function()
  {
    if (this.inbuf.length == 0)
      return;

    if (this.inbuf[0] == 4) {
      print('server: got socks 4');
      this.type = 'socks4';
      this.state = STATE_WAIT_SOCKS4_REQUEST;
      this.checkSocks4Request();
    } else if (this.inbuf[0] == 5) {
      print('server: got socks 5');
      this.type = 'socks';
      this.state = STATE_WAIT_SOCKS5_GREETING;
      this.checkSocks5Greeting();
    } else {
      do_throw("Unknown socks protocol!");
    }
  },

  checkSocks4Request: function()
  {
    if (this.inbuf.length < 8)
      return;

    do_check_eq(this.inbuf[1], 0x01);
    
    this.dest_port = this.inbuf.slice(2, 4);
    this.dest_addr = this.inbuf.slice(4, 8);

    this.inbuf = this.inbuf.slice(8);
    this.state = STATE_WAIT_SOCKS4_USERNAME;
    this.checkSocks4Username();
  },

  readString: function()
  {
    var i = this.inbuf.indexOf(0);
    var str = null;

    if (i >= 0) {
      var buf = this.inbuf.slice(0,i);
      str = buf2str(buf);
      this.inbuf = this.inbuf.slice(i+1);
    }

    return str;
  },

  checkSocks4Username: function()
  {
    var str = this.readString();

    if (str == null)
      return;

    this.username = str;
    if (this.dest_addr[0] == 0 &&
        this.dest_addr[1] == 0 &&
        this.dest_addr[2] == 0 &&
        this.dest_addr[3] != 0) {
      this.state = STATE_WAIT_SOCKS4_HOSTNAME;
      this.checkSocks4Hostname();
    } else {
      this.sendSocks4Response();
    }
  },

  checkSocks4Hostname: function()
  {
    var str = this.readString();

    if (str == null)
      return;

    this.dest_name = str;
    this.sendSocks4Response();
  },

  sendSocks4Response: function()
  {
    this.outbuf = '\x00\x5a\x00\x00\x00\x00\x00\x00';
    this.sendPing();
  },

  checkSocks5Greeting: function()
  {
    if (this.inbuf.length < 2)
      return;
    var nmethods = this.inbuf[1];
    if (this.inbuf.length < 2 + nmethods)
      return;

    do_check_true(nmethods >= 1);  
    var methods = this.inbuf.slice(2, 2 + nmethods);
    do_check_true(0 in methods);

    this.inbuf = [];
    this.state = STATE_WAIT_SOCKS5_REQUEST;
    this.write('\x05\x00');
  },
  
  checkSocks5Request: function()
  {
    if (this.inbuf.length < 4)
      return;

    do_check_eq(this.inbuf[0], 0x05);
    do_check_eq(this.inbuf[1], 0x01);
    do_check_eq(this.inbuf[2], 0x00);

    var atype = this.inbuf[3];
    var len;
    var name = false;

    switch (atype) {
      case 0x01:
        len = 4;
        break;
      case 0x03:
        len = this.inbuf[4];
        name = true;
        break;
      case 0x04:
        len = 16;
        break;  
      default:
        do_throw("Unknown address type " + atype);
    }
    
    if (name) {
      if (this.inbuf.length < 4 + len + 1 + 2)
        return;

      buf = this.inbuf.slice(5, 5 + len);
      this.dest_name = buf2str(buf);
      len += 1;
    } else {
      if (this.inbuf.length < 4 + len + 2)
        return;

      this.dest_addr = this.inbuf.slice(4, 4 + len);
    }

    len += 4;
    this.dest_port = this.inbuf.slice(len, len + 2);
    this.inbuf = this.inbuf.slice(len + 2);
    this.sendSocks5Response();
  },

  sendSocks5Response: function()
  {
    if (this.dest_addr.length == 16) {
      // send a successful response with the address, [::1]:80
      this.outbuf += '\x05\x00\x00\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x80';
    } else {
      // send a successful response with the address, 127.0.0.1:80
      this.outbuf += '\x05\x00\x00\x01\x7f\x00\x00\x01\x00\x80';
    }
    this.sendPing();
  },

  sendPing: function()
  {
    print('server: sending ping');
    this.state = STATE_WAIT_PONG;
    this.outbuf += "PING!";
    this.inbuf = [];
    this.waitWrite(this.client_out);
    this.waitRead(this.client_in);
  },

  checkPong: function()
  {
    var pong = buf2str(this.inbuf);
    do_check_eq(pong, "PONG!");
    this.state = STATE_GOT_PONG;
    this.waitRead(this.client_in);
  },
  
  close: function()
  {
    this.client_in.close();
    this.client_out.close();
  }
};

function SocksTestServer()
{
  this.listener = ServerSocket(-1, true, -1);
  socks_listen_port = this.listener.port;
  print('server: listening on', socks_listen_port);
  this.listener.asyncListen(this);
  this.test_cases = [];
  this.client_connections = [];
  this.client_subprocess = null;
  // port is used as the ID for test cases
  this.test_port_id = 8000;
  this.tests_completed = 0;
}
SocksTestServer.prototype = {
  addTestCase: function(test)
  {
    test.finished = false;
    test.port = this.test_port_id++;
    this.test_cases.push(test);
  },

  pickTest: function(id)
  {
    for (var i in this.test_cases) {
      var test = this.test_cases[i];
      if (test.port == id) {
        this.tests_completed++;
        return test;
      }
    }
    do_throw("No test case with id " + id);
  },

  testCompleted: function(client)
  {
    var port_id = buf2int(client.dest_port);
    var test = this.pickTest(port_id);
    
    print('server: test finished', test.port);
    do_check_true(test != null);
    do_check_eq(test.expectedType || test.type, client.type);
    do_check_eq(test.port, port_id);

    if (test.remote_dns)
      do_check_eq(test.host, client.dest_name);
    else
      do_check_eq(test.host, buf2ip(client.dest_addr));
    
    if (this.test_cases.length == this.tests_completed) {
      print('server: all tests completed');
      this.close();
      do_test_finished();
    }
  },

  runClientSubprocess: function()
  {
    var argv = [];

    // marshaled: socks_ver|server_port|dest_host|dest_port|<remote|local>
    for each (var test in this.test_cases) {
      var arg = test.type + '|' +
        String(socks_listen_port) + '|' +
        test.host + '|' + test.port + '|';
      if (test.remote_dns)
        arg += 'remote';
      else
        arg += 'local';
      print('server: using test case', arg);  
      argv.push(arg);      
    }

    this.client_subprocess = runScriptSubprocess(
        'socks_client_subprocess.js', argv);
  },

  onSocketAccepted: function(socket, trans)
  {
    print('server: got client connection');
    var input = trans.openInputStream(0, 0, 0);
    var output = trans.openOutputStream(0, 0, 0);
    var client = new SocksClient(this, input, output);
    this.client_connections.push(client);
  },
  
  onStopListening: function(socket)
  {  
  },

  close: function()
  {
    if (this.client_subprocess) {
      try {
        this.client_subprocess.kill();      
      } catch (x) {
        do_note_exception(x, 'Killing subprocess failed');
      }
      this.client_subprocess = null;
    }
    for each (var client in this.client_connections)
      client.close();
    this.client_connections = [];
    if (this.listener) {
      this.listener.close();
      this.listener = null;
    }
  }
};

function test_timeout()
{
  socks_test_server.close();
  do_throw("SOCKS test took too long!");
}

function run_test()
{
  socks_test_server = new SocksTestServer();

  socks_test_server.addTestCase({
        type: "socks4",
        host: '127.0.0.1',
        remote_dns: false,
  });
  socks_test_server.addTestCase({
        type: "socks4",
        host: '12345.xxx',
        remote_dns: true,
  });
  socks_test_server.addTestCase({
        type: "socks4",
        expectedType: "socks",
        host: '::1',
        remote_dns: false,
  });
  socks_test_server.addTestCase({
        type: "socks",
        host: '127.0.0.1',
        remote_dns: false,
  });
  socks_test_server.addTestCase({
        type: "socks",
        host: 'abcdefg.xxx',
        remote_dns: true,
  });
  socks_test_server.addTestCase({
        type: "socks",
        host: '::1',
        remote_dns: false,
  });
  socks_test_server.runClientSubprocess();

  do_timeout(120 * 1000, test_timeout);
  do_test_pending();
}
