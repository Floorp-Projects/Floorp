Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/identity.js");
Cu.import("resource://services-sync/jpakeclient.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const JPAKE_LENGTH_SECRET     = 8;
const JPAKE_LENGTH_CLIENTID   = 256;
const KEYEXCHANGE_VERSION     = 3;

/*
 * Simple server.
 */

const SERVER_MAX_GETS = 6;

function check_headers(request) {
  let stack = Components.stack.caller;

  // There shouldn't be any Basic auth
  do_check_false(request.hasHeader("Authorization"), stack);

  // Ensure key exchange ID is set and the right length
  do_check_true(request.hasHeader("X-KeyExchange-Id"), stack);
  do_check_eq(request.getHeader("X-KeyExchange-Id").length,
              JPAKE_LENGTH_CLIENTID, stack);
}

function new_channel() {
  // Create a new channel and register it with the server.
  let cid = Math.floor(Math.random() * 10000);
  while (channels[cid]) {
    cid = Math.floor(Math.random() * 10000);
  }
  let channel = channels[cid] = new ServerChannel();
  server.registerPathHandler("/" + cid, channel.handler());
  return cid;
}

let server;
let channels = {};  // Map channel -> ServerChannel object
function server_new_channel(request, response) {
  check_headers(request);
  let cid = new_channel();
  let body = JSON.stringify("" + cid);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
}

let error_report;
function server_report(request, response) {
  check_headers(request);

  if (request.hasHeader("X-KeyExchange-Log")) {
    error_report = request.getHeader("X-KeyExchange-Log");
  }

  if (request.hasHeader("X-KeyExchange-Cid")) {
    let cid = request.getHeader("X-KeyExchange-Cid");
    let channel = channels[cid];
    if (channel) {
      channel.clear();
    }
  }

  response.setStatusLine(request.httpVersion, 200, "OK");
}

// Hook for test code.
let hooks = {};
function initHooks() {
  hooks.onGET = function onGET(request) {};
}
initHooks();

function ServerChannel() {
  this.data = "";
  this.etag = "";
  this.getCount = 0;
}
ServerChannel.prototype = {

  GET: function GET(request, response) {
    if (!this.data) {
      response.setStatusLine(request.httpVersion, 404, "Not Found");
      return;
    }

    if (request.hasHeader("If-None-Match")) {
      let etag = request.getHeader("If-None-Match");
      if (etag == this.etag) {
        response.setStatusLine(request.httpVersion, 304, "Not Modified");
        hooks.onGET(request);
        return;
      }
    }
    response.setHeader("ETag", this.etag);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(this.data, this.data.length);

    // Automatically clear the channel after 6 successful GETs.
    this.getCount += 1;
    if (this.getCount == SERVER_MAX_GETS) {
      this.clear();
    }
    hooks.onGET(request);
  },

  PUT: function PUT(request, response) {
    if (this.data) {
      do_check_true(request.hasHeader("If-Match"));
      let etag = request.getHeader("If-Match");
      if (etag != this.etag) {
        response.setHeader("ETag", this.etag);
        response.setStatusLine(request.httpVersion, 412, "Precondition Failed");
        return;
      }
    } else {
      do_check_true(request.hasHeader("If-None-Match"));
      do_check_eq(request.getHeader("If-None-Match"), "*");
    }

    this.data = readBytesFromInputStream(request.bodyInputStream);
    this.etag = '"' + Utils.sha1(this.data) + '"';
    response.setHeader("ETag", this.etag);
    response.setStatusLine(request.httpVersion, 200, "OK");
  },

  clear: function clear() {
    delete this.data;
  },

  handler: function handler() {
    let self = this;
    return function(request, response) {
      check_headers(request);
      let method = self[request.method];
      return method.apply(self, arguments);
    };
  }

};


/**
 * Controller that throws for everything.
 */
let BaseController = {
  displayPIN: function displayPIN() {
    do_throw("displayPIN() shouldn't have been called!");
  },
  onPairingStart: function onPairingStart() {
    do_throw("onPairingStart shouldn't have been called!");
  },
  onAbort: function onAbort(error) {
    do_throw("Shouldn't have aborted with " + error + "!");
  },
  onPaired: function onPaired() {
    do_throw("onPaired() shouldn't have been called!");
  },
  onComplete: function onComplete(data) {
    do_throw("Shouldn't have completed with " + data + "!");
  }
};


const DATA = {"msg": "eggstreamly sekrit"};
const POLLINTERVAL = 50;

function run_test() {
  server = httpd_setup({"/new_channel": server_new_channel,
                        "/report":      server_report});
  Svc.Prefs.set("jpake.serverURL", server.baseURI + "/");
  Svc.Prefs.set("jpake.pollInterval", POLLINTERVAL);
  Svc.Prefs.set("jpake.maxTries", 2);
  Svc.Prefs.set("jpake.firstMsgMaxTries", 5);
  Svc.Prefs.set("jpake.lastMsgMaxTries", 5);
  // Ensure clean up
  Svc.Obs.add("profile-before-change", function() {
    Svc.Prefs.resetBranch("");
  });

  // Ensure PSM is initialized.
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

  // Simulate Sync setup with credentials in place. We want to make
  // sure the J-PAKE requests don't include those data.
  setBasicCredentials("johndoe", "ilovejane");

  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.JPAKEClient").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Common.RESTRequest").level =
    Log4Moz.Level.Trace;
  run_next_test();
}


add_test(function test_success_receiveNoPIN() {
  _("Test a successful exchange started by receiveNoPIN().");

  let snd = new JPAKEClient({
    __proto__: BaseController,
    onPaired: function onPaired() {
      _("Pairing successful, sending final payload.");
      do_check_true(pairingStartCalledOnReceiver);
      Utils.nextTick(function() { snd.sendAndComplete(DATA); });
    },
    onComplete: function onComplete() {}
  });

  let pairingStartCalledOnReceiver = false;
  let rec = new JPAKEClient({
    __proto__: BaseController,
    displayPIN: function displayPIN(pin) {
      _("Received PIN " + pin + ". Entering it in the other computer...");
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      Utils.nextTick(function() { snd.pairWithPIN(pin, false); });
    },
    onPairingStart: function onPairingStart() {
      pairingStartCalledOnReceiver = true;
    },
    onComplete: function onComplete(data) {
      do_check_true(Utils.deepEquals(DATA, data));
      // Ensure channel was cleared, no error report.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, undefined);
      run_next_test();
    }
  });
  rec.receiveNoPIN();
});


add_test(function test_firstMsgMaxTries_timeout() {
  _("Test abort when sender doesn't upload anything.");

  let rec = new JPAKEClient({
    __proto__: BaseController,
    displayPIN: function displayPIN(pin) {
      _("Received PIN " + pin + ". Doing nothing...");
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
    },
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_TIMEOUT);
      // Ensure channel was cleared, error report was sent.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, JPAKE_ERROR_TIMEOUT);
      error_report = undefined;
      run_next_test();
    }
  });
  rec.receiveNoPIN();
});


add_test(function test_firstMsgMaxTries() {
  _("Test that receiver can wait longer for the first message.");

  let snd = new JPAKEClient({
    __proto__: BaseController,
    onPaired: function onPaired() {
      _("Pairing successful, sending final payload.");
      Utils.nextTick(function() { snd.sendAndComplete(DATA); });
    },
    onComplete: function onComplete() {}
  });

  let rec = new JPAKEClient({
    __proto__: BaseController,
    displayPIN: function displayPIN(pin) {
      // For the purpose of the tests, the poll interval is 50ms and
      // we're polling up to 5 times for the first exchange (as
      // opposed to 2 times for most of the other exchanges). So let's
      // pretend it took 150ms to enter the PIN on the sender, which should
      // require 3 polls.
      // Rather than using an imprecise timer, we hook into the channel's
      // GET handler to know how long to wait.
      _("Received PIN " + pin + ". Waiting for three polls before entering it into sender...");
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      let count = 0;
      hooks.onGET = function onGET(request) {
        if (++count == 3) {
          _("Third GET. Triggering pair.");
          Utils.nextTick(function() { snd.pairWithPIN(pin, false); });
        }
      };
    },
    onPairingStart: function onPairingStart(pin) {},
    onComplete: function onComplete(data) {
      do_check_true(Utils.deepEquals(DATA, data));
      // Ensure channel was cleared, no error report.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, undefined);

      // Clean up.
      initHooks();
      run_next_test();
    }
  });
  rec.receiveNoPIN();
});


add_test(function test_lastMsgMaxTries() {
  _("Test that receiver can wait longer for the last message.");

 let snd = new JPAKEClient({
    __proto__: BaseController,
    onPaired: function onPaired() {
      // For the purpose of the tests, the poll interval is 50ms and
      // we're polling up to 5 times for the last exchange (as opposed
      // to 2 times for other exchanges). So let's pretend it took
      // 150ms to come up with the final payload, which should require
      // 3 polls.
      // Rather than using an imprecise timer, we hook into the channel's
      // GET handler to know how long to wait.
      let count = 0;
      hooks.onGET = function onGET(request) {
        if (++count == 3) {
          _("Third GET. Triggering send.");
          Utils.nextTick(function() { snd.sendAndComplete(DATA); });
        }
      };
    },
    onComplete: function onComplete() {}
  });

  let rec = new JPAKEClient({
    __proto__: BaseController,
    displayPIN: function displayPIN(pin) {
      _("Received PIN " + pin + ". Entering it in the other computer...");
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      Utils.nextTick(function() { snd.pairWithPIN(pin, false); });
    },
    onPairingStart: function onPairingStart(pin) {},
    onComplete: function onComplete(data) {
      do_check_true(Utils.deepEquals(DATA, data));
      // Ensure channel was cleared, no error report.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, undefined);

      // Clean up.
      initHooks();
      run_next_test();
    }
  });

  rec.receiveNoPIN();
});


add_test(function test_wrongPIN() {
  _("Test abort when PINs don't match.");

  let snd = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_KEYMISMATCH);
      do_check_eq(error_report, JPAKE_ERROR_KEYMISMATCH);
      error_report = undefined;
    }
  });

  let pairingStartCalledOnReceiver = false;
  let rec = new JPAKEClient({
    __proto__: BaseController,
    displayPIN: function displayPIN(pin) {
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      let secret = pin.slice(0, JPAKE_LENGTH_SECRET);
      secret = [char for each (char in secret)].reverse().join("");
      let new_pin = secret + this.cid;
      _("Received PIN " + pin + ", but I'm entering " + new_pin);

      Utils.nextTick(function() { snd.pairWithPIN(new_pin, false); });
    },
    onPairingStart: function onPairingStart() {
      pairingStartCalledOnReceiver = true;
    },
    onAbort: function onAbort(error) {
      do_check_true(pairingStartCalledOnReceiver);
      do_check_eq(error, JPAKE_ERROR_NODATA);
      // Ensure channel was cleared.
      do_check_eq(channels[this.cid].data, undefined);
      run_next_test();
    }
  });
  rec.receiveNoPIN();
});


add_test(function test_abort_receiver() {
  _("Test user abort on receiving side.");

  let rec = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      // Manual abort = userabort.
      do_check_eq(error, JPAKE_ERROR_USERABORT);
      // Ensure channel was cleared.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, JPAKE_ERROR_USERABORT);
      error_report = undefined;
      run_next_test();
    },
    displayPIN: function displayPIN(pin) {
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      Utils.nextTick(function() { rec.abort(); });
    }
  });
  rec.receiveNoPIN();
});


add_test(function test_abort_sender() {
  _("Test user abort on sending side.");

  let snd = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      // Manual abort == userabort.
      do_check_eq(error, JPAKE_ERROR_USERABORT);
      do_check_eq(error_report, JPAKE_ERROR_USERABORT);
      error_report = undefined;
    }
  });

  let rec = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_NODATA);
      // Ensure channel was cleared, no error report.
      do_check_eq(channels[this.cid].data, undefined);
      do_check_eq(error_report, undefined);
      initHooks();
      run_next_test();
    },
    displayPIN: function displayPIN(pin) {
      _("Received PIN " + pin + ". Entering it in the other computer...");
      this.cid = pin.slice(JPAKE_LENGTH_SECRET);
      Utils.nextTick(function() { snd.pairWithPIN(pin, false); });

      // Abort after the first poll.
      let count = 0;
      hooks.onGET = function onGET(request) {
        if (++count >= 1) {
          _("First GET. Aborting.");
          Utils.nextTick(function() { snd.abort(); });
        }
      };
    },
    onPairingStart: function onPairingStart(pin) {}
  });
  rec.receiveNoPIN();
});


add_test(function test_wrongmessage() {
  let cid = new_channel();
  let channel = channels[cid];
  channel.data = JSON.stringify({type: "receiver2",
                                 version: KEYEXCHANGE_VERSION,
                                 payload: {}});
  channel.etag = '"fake-etag"';
  let snd = new JPAKEClient({
    __proto__: BaseController,
    onComplete: function onComplete(data) {
      do_throw("onComplete shouldn't be called.");
    },
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_WRONGMESSAGE);
      run_next_test();
    }
  });
  snd.pairWithPIN("01234567" + cid, false);
});


add_test(function test_error_channel() {
  let serverURL = Svc.Prefs.get("jpake.serverURL");
  Svc.Prefs.set("jpake.serverURL", "http://localhost:12345/");

  let rec = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_CHANNEL);
      Svc.Prefs.set("jpake.serverURL", serverURL);
      run_next_test();
    },
    onPairingStart: function onPairingStart(pin) {},
    displayPIN: function displayPIN(pin) {}
  });
  rec.receiveNoPIN();
});


add_test(function test_error_network() {
  let serverURL = Svc.Prefs.get("jpake.serverURL");
  Svc.Prefs.set("jpake.serverURL", "http://localhost:12345/");

  let snd = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_NETWORK);
      Svc.Prefs.set("jpake.serverURL", serverURL);
      run_next_test();
    }
  });
  snd.pairWithPIN("0123456789ab", false);
});


add_test(function test_error_server_noETag() {
  let cid = new_channel();
  let channel = channels[cid];
  channel.data = JSON.stringify({type: "receiver1",
                                 version: KEYEXCHANGE_VERSION,
                                 payload: {}});
  // This naughty server doesn't supply ETag (well, it supplies empty one).
  channel.etag = "";
  let snd = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_SERVER);
      run_next_test();
    }
  });
  snd.pairWithPIN("01234567" + cid, false);
});


add_test(function test_error_delayNotSupported() {
  let cid = new_channel();
  let channel = channels[cid];
  channel.data = JSON.stringify({type: "receiver1",
                                 version: 2,
                                 payload: {}});
  channel.etag = '"fake-etag"';
  let snd = new JPAKEClient({
    __proto__: BaseController,
    onAbort: function onAbort(error) {
      do_check_eq(error, JPAKE_ERROR_DELAYUNSUPPORTED);
      run_next_test();
    }
  });
  snd.pairWithPIN("01234567" + cid, true);
});


add_test(function test_sendAndComplete_notPaired() {
  let snd = new JPAKEClient({__proto__: BaseController});
  do_check_throws(function () {
    snd.sendAndComplete(DATA);
  });
  run_next_test();
});


add_test(function tearDown() {
  server.stop(run_next_test);
});
