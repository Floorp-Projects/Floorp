/* run some tests on the gvfs/gio protocol handler */

"use strict";

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

const PR_RDONLY = 0x1; // see prio.h

[
  do_test_read_data_dir,
  do_test_read_recent,
  test_read_file,
  do_test_finished,
].forEach(f => add_test(f));

function setup() {
  // Allowing some protocols to get a channel
  if (!inChildProcess()) {
    Services.prefs.setCharPref(
      "network.gio.supported-protocols",
      "localtest:,recent:"
    );
  } else {
    do_send_remote_message("gio-allow-test-protocols");
    do_await_remote_message("gio-allow-test-protocols-done");
  }
}

setup();

registerCleanupFunction(() => {
  // Resetting the protocols to None
  if (!inChildProcess()) {
    Services.prefs.clearUserPref("network.gio.supported-protocols");
  }
});

function new_file_input_stream(file, buffered) {
  var stream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  stream.init(file, PR_RDONLY, 0, 0);
  if (!buffered) {
    return stream;
  }

  var buffer = Cc[
    "@mozilla.org/network/buffered-input-stream;1"
  ].createInstance(Ci.nsIBufferedInputStream);
  buffer.init(stream, 4096);
  return buffer;
}

function new_file_channel(file) {
  var chan = NetUtil.newChannel({
    uri: file,
    loadUsingSystemPrincipal: true,
  });

  return chan;
}

/*
 * stream listener
 * this listener has some additional file-specific tests, so we can't just use
 * ChannelListener here.
 */
function FileStreamListener(closure) {
  this._closure = closure;
}
FileStreamListener.prototype = {
  _closure: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,

  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  onStartRequest(request) {
    if (this._got_onstartrequest) {
      do_throw("Got second onStartRequest event!");
    }
    this._got_onstartrequest = true;
  },

  onDataAvailable(request, stream, offset, count) {
    if (!this._got_onstartrequest) {
      do_throw("onDataAvailable without onStartRequest event!");
    }
    if (this._got_onstoprequest) {
      do_throw("onDataAvailable after onStopRequest event!");
    }
    if (!request.isPending()) {
      do_throw("request reports itself as not pending from onStartRequest!");
    }

    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest(request, status) {
    if (!this._got_onstartrequest) {
      do_throw("onStopRequest without onStartRequest event!");
    }
    if (this._got_onstoprequest) {
      do_throw("Got second onStopRequest event!");
    }
    this._got_onstoprequest = true;
    if (!Components.isSuccessCode(status)) {
      do_throw("Failed to load file: " + status.toString(16));
    }
    if (status != request.status) {
      do_throw("request.status does not match status arg to onStopRequest!");
    }
    if (request.isPending()) {
      do_throw("request reports itself as pending from onStopRequest!");
    }
    if (this._contentLen != -1 && this._buffer.length != this._contentLen) {
      do_throw("did not read nsIChannel.contentLength number of bytes!");
    }

    this._closure(this._buffer);
  },
};

function test_read_file() {
  dump("*** test_read_file\n");
  // Going via parent path, because this is opended from test/unit/ and test/unit_ipc/
  var file = do_get_file("../unit/data/test_readline4.txt");
  var chan = new_file_channel("localtest://" + file.path);

  function on_read_complete(data) {
    dump("*** test_read_file.on_read_complete()\n");
    /* read completed successfully.  now read data directly from file,
       and compare the result. */
    var stream = new_file_input_stream(file, false);
    var result = read_stream(stream, stream.available());
    if (result != data) {
      do_throw("Stream contents do not match with direct read!");
    }
    run_next_test();
  }

  chan.asyncOpen(new FileStreamListener(on_read_complete));
}

function do_test_read_data_dir() {
  dump('*** test_read_data_dir("../data/")\n');

  var dir = do_get_file("../unit/data/");
  var chan = new_file_channel("localtest://" + dir.path);

  function on_read_complete(data) {
    dump("*** test_read_data_dir.on_read_complete()\n");

    // The data-directory should be listed, containing a header-line and the files therein
    if (
      !(
        data.includes("200: filename content-length last-modified file-type") &&
        data.includes("201: test_readline1.txt") &&
        data.includes("201: test_readline2.txt")
      )
    ) {
      do_throw(
        "test_read_data_dir() - Bad data! Does not contain needles! Is <" +
          data +
          ">"
      );
    }
    run_next_test();
  }
  chan.asyncOpen(new FileStreamListener(on_read_complete));
}

function do_test_read_recent() {
  dump('*** test_read_recent("recent://")\n');

  var chan = new_file_channel("recent:///");

  function on_read_complete(data) {
    dump("*** test_read_recent.on_read_complete()\n");

    // The data-directory should be listed, containing a header-line and the files therein
    if (
      !data.includes("200: filename content-length last-modified file-type")
    ) {
      do_throw(
        "do_test_read_recent() - Bad data! Does not contain header! Is <" +
          data +
          ">"
      );
    }
    run_next_test();
  }
  chan.asyncOpen(new FileStreamListener(on_read_complete));
}
