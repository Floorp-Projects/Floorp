"use strict";

/* globals TCPServerSocket */

const CC = Components.Constructor;

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

const currentThread = Cc["@mozilla.org/thread-manager;1"]
                      .getService().currentThread;

// Most of the socks logic here is copied and upgraded to support authentication
// for socks5. The original test is from netwerk/test/unit/test_socks.js

// Socks 4 support was left in place for future tests.

const STATE_WAIT_GREETING = 1;
const STATE_WAIT_SOCKS4_REQUEST = 2;
const STATE_WAIT_SOCKS4_USERNAME = 3;
const STATE_WAIT_SOCKS4_HOSTNAME = 4;
const STATE_WAIT_SOCKS5_GREETING = 5;
const STATE_WAIT_SOCKS5_REQUEST = 6;
const STATE_WAIT_SOCKS5_AUTH = 7;
const STATE_WAIT_INPUT = 8;
const STATE_FINISHED = 9;

/**
 * A basic socks proxy setup that handles a single http response page.  This
 * is used for testing socks auth with webrequest.  We don't bother making
 * sure we buffer ondata, etc., we'll never get anything but tiny chunks here.
 */
class SocksClient {
  constructor(server, socket) {
    this.server = server;
    this.type = "";
    this.username = "";
    this.dest_name = "";
    this.dest_addr = [];
    this.dest_port = [];

    this.inbuf = [];
    this.state = STATE_WAIT_GREETING;
    this.socket = socket;

    socket.onclose = (event) => {
      this.server.requestCompleted(this);
    };
    socket.ondata = (event) => {
      let len = event.data.byteLength;

      if (len == 0 && this.state == STATE_FINISHED) {
        this.close();
        this.server.requestCompleted(this);
        return;
      }

      this.inbuf = new Uint8Array(event.data);
      Promise.resolve().then(() => { this.callState(); });
    };
  }

  callState() {
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
      case STATE_WAIT_SOCKS5_AUTH:
        this.checkSocks5Auth();
        break;
      case STATE_WAIT_INPUT:
        this.checkRequest();
        break;
      default:
        do_throw("server: read in invalid state!");
    }
  }

  write(buf) {
    this.socket.send(new Uint8Array(buf).buffer);
  }

  checkSocksGreeting() {
    if (this.inbuf.length == 0) {
      return;
    }

    if (this.inbuf[0] == 4) {
      this.type = "socks4";
      this.state = STATE_WAIT_SOCKS4_REQUEST;
      this.checkSocks4Request();
    } else if (this.inbuf[0] == 5) {
      this.type = "socks";
      this.state = STATE_WAIT_SOCKS5_GREETING;
      this.checkSocks5Greeting();
    } else {
      do_throw("Unknown socks protocol!");
    }
  }

  checkSocks4Request() {
    if (this.inbuf.length < 8) {
      return;
    }

    this.dest_port = this.inbuf.slice(2, 4);
    this.dest_addr = this.inbuf.slice(4, 8);

    this.inbuf = this.inbuf.slice(8);
    this.state = STATE_WAIT_SOCKS4_USERNAME;
    this.checkSocks4Username();
  }

  readString() {
    let i = this.inbuf.indexOf(0);
    let str = null;

    if (i >= 0) {
      let decoder = new TextDecoder();
      str = decoder.decode(this.inbuf.slice(0, i));
      this.inbuf = this.inbuf.slice(i + 1);
    }

    return str;
  }

  checkSocks4Username() {
    let str = this.readString();

    if (str == null) {
      return;
    }

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
  }

  checkSocks4Hostname() {
    let str = this.readString();

    if (str == null) {
      return;
    }

    this.dest_name = str;
    this.sendSocks4Response();
  }

  sendSocks4Response() {
    this.state = STATE_WAIT_INPUT;
    this.inbuf = [];
    this.write([0, 0x5A, 0, 0, 0, 0, 0, 0]);
  }

  /**
   * checks authentication information.
   *
   * buf[0] socks version
   * buf[1] number of auth methods supported
   * buf[2+nmethods] value for each auth method
   *
   * Response is
   * byte[0] socks version
   * byte[1] desired auth method
   *
   * For whatever reason, Firefox does not present auth method 0x02 however
   * responding with that does cause Firefox to send authentication if
   * the nsIProxyInfo instance has the data.  IUUC Firefox should send
   * supported methods, but I'm no socks expert.
   */
  checkSocks5Greeting() {
    if (this.inbuf.length < 2) {
      return;
    }
    let nmethods = this.inbuf[1];
    if (this.inbuf.length < 2 + nmethods) {
      return;
    }

    // See comment above, keeping for future update.
    // let methods = this.inbuf.slice(2, 2 + nmethods);

    this.inbuf = [];
    if (this.server.password || this.server.username) {
      this.state = STATE_WAIT_SOCKS5_AUTH;
      this.write([5, 2]);
    } else {
      this.state = STATE_WAIT_SOCKS5_REQUEST;
      this.write([5, 0]);
    }
  }

  checkSocks5Auth() {
    equal(this.inbuf[0], 0x01, "subnegotiation version");
    let uname_len = this.inbuf[1];
    let pass_len = this.inbuf[2 + uname_len];
    let unnamebuf = this.inbuf.slice(2, 2 + uname_len);
    let pass_start = 2 + uname_len + 1;
    let pwordbuf = this.inbuf.slice(pass_start, pass_start + pass_len);
    let decoder = new TextDecoder();
    let username = decoder.decode(unnamebuf);
    let password = decoder.decode(pwordbuf);
    this.inbuf = [];
    equal(username, this.server.username, "socks auth username");
    equal(password, this.server.password, "socks auth password");
    if (username == this.server.username && password == this.server.password) {
      this.state = STATE_WAIT_SOCKS5_REQUEST;
      // x00 is success, any other value closes the connection
      this.write([1, 0]);
      return;
    }
    this.state = STATE_FINISHED;
    this.write([1, 1]);
  }

  checkSocks5Request() {
    if (this.inbuf.length < 4) {
      return;
    }

    let atype = this.inbuf[3];
    let len;
    let name = false;

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
      if (this.inbuf.length < 4 + len + 1 + 2) {
        return;
      }

      let buf = this.inbuf.slice(5, 5 + len);
      let decoder = new TextDecoder();
      this.dest_name = decoder.decode(buf);
      len += 1;
    } else {
      if (this.inbuf.length < 4 + len + 2) {
        return;
      }

      this.dest_addr = this.inbuf.slice(4, 4 + len);
    }

    len += 4;
    this.dest_port = this.inbuf.slice(len, len + 2);
    this.inbuf = this.inbuf.slice(len + 2);
    this.sendSocks5Response();
  }

  sendSocks5Response() {
    let buf;
    if (this.dest_addr.length == 16) {
      // send a successful response with the address, [::1]:80
      buf = [5, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 80];
    } else {
      // send a successful response with the address, 127.0.0.1:80
      buf = [5, 0, 0, 1, 127, 0, 0, 1, 0, 80];
    }
    this.state = STATE_WAIT_INPUT;
    this.inbuf = [];
    this.write(buf);
  }

  checkRequest() {
    let decoder = new TextDecoder();
    let request = decoder.decode(this.inbuf);

    if (request == "PING!") {
      this.state = STATE_FINISHED;
      this.socket.send("PONG!");
    } else if (request.startsWith("GET / HTTP/1.1")) {
      this.socket.send("HTTP/1.1 200 OK\r\n" +
                       "Content-Length: 2\r\n" +
                       "Content-Type: text/html\r\n" +
                       "\r\nOK");
      this.state = STATE_FINISHED;
    }
  }

  close() {
    this.socket.close();
  }
}

class SocksTestServer {
  constructor() {
    this.client_connections = new Set();
    this.listener = new TCPServerSocket(-1, {binaryType: "arraybuffer"}, -1);
    this.listener.onconnect = (event) => {
      let client = new SocksClient(this, event.socket);
      this.client_connections.add(client);
    };
  }

  requestCompleted(client) {
    this.client_connections.delete(client);
  }

  close() {
    for (let client of this.client_connections) {
      client.close();
    }
    this.client_connections = new Set();
    if (this.listener) {
      this.listener.close();
      this.listener = null;
    }
  }

  setUserPass(username, password) {
    this.username = username;
    this.password = password;
  }
}

/**
 * Tests the basic socks logic using a simple socket connection and the
 * protocol proxy service.  It seems TCPSocket has no way to tie proxy
 * data to it, so we go old school here.
 */
class SocksTestClient {
  constructor(socks, dest, resolve, reject) {
    let pps = Cc["@mozilla.org/network/protocol-proxy-service;1"]
              .getService(Ci.nsIProtocolProxyService);
    let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);

    let pi_flags = 0;
    if (socks.dns == "remote") {
      pi_flags = Ci.nsIProxyInfo.TRANSPARENT_PROXY_RESOLVES_HOST;
    }

    let pi = pps.newProxyInfoWithAuth(socks.version, socks.host, socks.port,
                                      socks.username, socks.password,
                                      pi_flags, -1, null);

    this.trans = sts.createTransport(null, 0, dest.host, dest.port, pi);
    this.input = this.trans.openInputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
    this.output = this.trans.openOutputStream(Ci.nsITransport.OPEN_BLOCKING, 0, 0);
    this.outbuf = String();
    this.resolve = resolve;
    this.reject = reject;

    this.write("PING!");
    this.input.asyncWait(this, 0, 0, currentThread);
  }

  onInputStreamReady(stream) {
    let len = 0;
    try {
      len = stream.available();
    } catch (e) {
      // This will happen on auth failure.
      this.reject(e);
      return;
    }
    let bin = new BinaryInputStream(stream);
    let data = bin.readByteArray(len);
    let decoder = new TextDecoder();
    let result = decoder.decode(data);
    if (result == "PONG!") {
      this.resolve(result);
    } else {
      this.reject();
    }
  }

  write(buf) {
    this.outbuf += buf;
    this.output.asyncWait(this, 0, 0, currentThread);
  }

  onOutputStreamReady(stream) {
    let len = stream.write(this.outbuf, this.outbuf.length);
    if (len != this.outbuf.length) {
      this.outbuf = this.outbuf.substring(len);
      stream.asyncWait(this, 0, 0, currentThread);
    } else {
      this.outbuf = String();
    }
  }

  close() {
    this.output.close();
  }
}

const socksServer = new SocksTestServer();
socksServer.setUserPass("foo", "bar");
do_register_cleanup(() => {
  socksServer.close();
});

// A simple ping/pong to test the socks server.
add_task(async function test_socks_server() {
  let socks = {
    version: "socks",
    host: "127.0.0.1",
    port: socksServer.listener.localPort,
    username: "foo",
    password: "bar",
    dns: false,
  };
  let dest = {
    host: "localhost",
    port: 8888,
  };

  new Promise((resolve, reject) => {
    new SocksTestClient(socks, dest, resolve, reject);
  }).then(result => {
    equal("PONG!", result, "socks test ok");
  }).catch(result => {
    ok(false, `socks test failed ${result}`);
  });
});

add_task(async function test_webRequest_socks_proxy() {
  async function background(port) {
    function checkProxyData(details) {
      browser.test.assertEq("127.0.0.1", details.proxyInfo.host, "proxy host");
      browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
      browser.test.assertEq("socks", details.proxyInfo.type, "proxy type");
      browser.test.assertEq("foo", details.proxyInfo.username, "proxy username not set");
      browser.test.assertEq(undefined, details.proxyInfo.password, "no proxy password passed to webrequest");
    }
    browser.webRequest.onBeforeRequest.addListener(details => {
      checkProxyData(details);
    }, {urls: ["<all_urls>"]});
    browser.webRequest.onAuthRequired.addListener(details => {
      // We should never get onAuthRequired for socks proxy
      browser.test.fail("onAuthRequired");
    }, {urls: ["<all_urls>"]}, ["blocking"]);
    browser.webRequest.onCompleted.addListener(details => {
      checkProxyData(details);
      browser.test.sendMessage("done");
    }, {urls: ["<all_urls>"]});

    await browser.proxy.register("proxy.js");
    browser.test.sendMessage("pac-ready");
  }

  let handlingExt = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "proxy",
        "webRequest",
        "webRequestBlocking",
        "<all_urls>",
      ],
    },
    background: `(${background})(${socksServer.listener.localPort})`,
    files: {
      "proxy.js": `
        function FindProxyForURL(url, host) {
          return [{
            type: "socks",
            host: "127.0.0.1",
            port: ${socksServer.listener.localPort},
            username: "foo",
            password: "bar",
          }];
        }`,
    },
  });

  await handlingExt.startup();
  await handlingExt.awaitMessage("pac-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(`http://localhost/`);

  await handlingExt.awaitMessage("done");
  await contentPage.close();
  await handlingExt.unload();
});
