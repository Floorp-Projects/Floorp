/**
 * Read count bytes from stream and return as a String object
 */
function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper =
      Components.classes["@mozilla.org/binaryinputstream;1"]
                .createInstance(Components.interfaces.nsIBinaryInputStream);
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

const CL_EXPECT_FAILURE = 0x1;

/**
 * A stream listener that calls a callback function with a specified
 * context and the received data when the channel is loaded.
 *
 * Signature of the closure:
 *   void closure(in nsIRequest request, in ACString data, in JSObject context);
 *
 * This listener makes sure that various parts of the channel API are
 * implemented correctly and that the channel's status is a success code
 * (you can pass CL_EXPECT_FAILURE as flags to allow a failure code)
 *
 * Note that it also requires a valid content length on the channel and
 * is thus not fully generic.
 */
function ChannelListener(closure, ctx, flags) {
  this._closure = closure;
  this._closurectx = ctx;
  this._flags = flags;
}
ChannelListener.prototype = {
  _closure: null,
  _closurectx: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,

  QueryInterface: function(iid) {
    if (iid.Equals(Components.interfaces.nsIStreamChannelListener) ||
        iid.Equals(Components.interfaces.nsIRequestObserver) ||
        iid.Equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, context) {
    if (this._got_onstartrequest)
      do_throw("Got second onStartRequest event!");
    this._got_onstartrequest = true;

    request.QueryInterface(Components.interfaces.nsIChannel);
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
    if (this._flags & CL_EXPECT_FAILURE)
      do_throw("Got data despite expecting a failure");

    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, context, status) {
    if (!this._got_onstartrequest)
      do_throw("onStopRequest without onStartRequest event!");
    if (this._got_onstoprequest)
      do_throw("Got second onStopRequest event!");
    this._got_onstoprequest = true;
    var success = Components.isSuccessCode(status);
    if ((this._flags & CL_EXPECT_FAILURE) && success)
      do_throw("Should have failed to load URL (status is " + status.toString(16) + ")");
    else if (!(this._flags & CL_EXPECT_FAILURE) && !success)
      do_throw("Failed to load URL: " + status.toString(16));
    if (status != request.status)
      do_throw("request.status does not match status arg to onStopRequest!");
    if (request.isPending())
      do_throw("request reports itself as pending from onStopRequest!");
    if (!(this._flags & CL_EXPECT_FAILURE) &&
        this._contentLen != -1 && this._buffer.length != this._contentLen)
      do_throw("did not read nsIChannel.contentLength number of bytes!");

    this._closure(request, this._buffer, this._closurectx);
  }
};
