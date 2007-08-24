/* run some tests on the data: protocol handler */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// The behaviour wrt spaces is:
// - Textual content keeps all spaces
// - Other content strips unescaped spaces
// - Base64 content strips escaped and unescaped spaces
var urls = [
  ["data:,foo",                                     "text/plain",               "foo"],
  ["data:application/octet-stream,foo bar",         "application/octet-stream", "foobar"],
  ["data:application/octet-stream,foo%20bar",       "application/octet-stream", "foo bar"],
  ["data:application/xhtml+xml,foo bar",            "application/xhtml+xml",    "foo bar"],
  ["data:application/xhtml+xml,foo%20bar",          "application/xhtml+xml",    "foo bar"],
  ["data:text/plain,foo%00 bar",                    "text/plain",               "foo\x00 bar"],
  ["data:text/plain;base64,Zm9 vI%20GJ%0Dhc%0Ag==", "text/plain",               "foo bar"]
];

function run_next_test() {
  test_array[test_index++]();
}

/* read count bytes from stream and return as a String object */
function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper =
      Cc["@mozilla.org/binaryinputstream;1"].
      createInstance(Ci.nsIBinaryInputStream);
  wrapper.setInputStream(stream);
  /* JS methods can be called with a maximum of 65535 arguments, and input
     streams don't have to return all the data they make .available() when
     asked to .read() that number of bytes. */
  var data = [];
  while (count > 0) {
    var bytes = wrapper.readByteArray(Math.min(65535, count));
    data.push(String.fromCharCode.apply(null, bytes));
    count -= bytes.length;
    if (bytes.length == 0)
      do_throw("Nothing read from input stream!");
  }
  return data.join('');
}

/* stream listener */
function Listener(closure, ctx) {
  this._closure = closure;
  this._closurectx = ctx;
}
Listener.prototype = {
  _closure: null,
  _closurectx: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,

  QueryInterface: function(iid) {
    if (iid.Equals(Ci.nsIStreamListener) ||
        iid.Equals(Ci.nsIRequestObserver) ||
        iid.Equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    if (this._got_onstartrequest)
      do_throw("Got second onStartRequest event!");
    this._got_onstartrequest = true;

    request.QueryInterface(Ci.nsIChannel);
    this._contentLen = request.contentLength;
    if (this._contentLen == -1)
      do_throw("Content length is unknown in onStartRequest!");
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    if (!this._got_onstartrequest)
      do_throw("onDataAvailable without onStartRequest event!");
    if (this._got_onstoprequest)
      do_throw("onDataAvailable after onStopRequest event!");
    if (!request.isPending())
      do_throw("request reports itself as not pending from onStartRequest!");

    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    if (!this._got_onstartrequest)
      do_throw("onStopRequest without onStartRequest event!");
    if (this._got_onstoprequest)
      do_throw("Got second onStopRequest event!");
    this._got_onstoprequest = true;
    if (!Components.isSuccessCode(status))
      do_throw("Failed to load URL: " + status.toString(16));
    if (status != request.status)
      do_throw("request.status does not match status arg to onStopRequest!");
    if (request.isPending())
      do_throw("request reports itself as pending from onStopRequest!");
    if (this._contentLen != -1 && this._buffer.length != this._contentLen)
      do_throw("did not read nsIChannel.contentLength number of bytes!");

    this._closure(request, this._buffer, this._closurectx);
  }
};

function run_test() {
  dump("*** run_test\n");

  function on_read_complete(request, data, idx) {
    dump("*** run_test.on_read_complete\n");

    if (request.nsIChannel.contentType != urls[idx][1])
      do_throw("Type mismatch! Is <" + chan.contentType + ">, should be <" + urls[idx][1] + ">");

    /* read completed successfully.  now compare the data. */
    if (data != urls[idx][2])
      do_throw("Stream contents do not match with direct read!");
    do_test_finished();
  }

  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  for (var i = 0; i < urls.length; ++i) {
    dump("*** opening channel " + i + "\n");
    do_test_pending();
    var chan = ios.newChannel(urls[i][0], "", null);
    chan.contentType = "foo/bar"; // should be ignored
    chan.asyncOpen(new Listener(on_read_complete, i), null);
  }
}

