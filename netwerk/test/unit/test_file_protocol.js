/* run some tests on the file:// protocol handler */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const PR_RDONLY = 0x1;  // see prio.h

var test_index = 0;
var test_array = [
  test_read_file,
  test_read_dir,
  test_upload_file,
  do_test_finished
];

function run_next_test() {
  test_array[test_index++]();
}

function getFile(key) {
  var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  return dirSvc.get(key, Components.interfaces.nsILocalFile);
}

/* read count bytes from stream and return as a String object */
function read_stream(stream, count) {
  /* assume stream has non-ASCII data */
  var wrapper =
      Cc["@mozilla.org/binaryinputstream;1"].
      createInstance(Ci.nsIBinaryInputStream);
  wrapper.setInputStream(stream);
  var bytes = wrapper.readByteArray(count);
  /* assume that the stream is going to give us all of the data.
     not all streams conform to this. */
  if (bytes.length != count)
    do_throw("Partial read from input stream!");
  /* convert array of bytes to a String object (by no means efficient) */  
  return eval("String.fromCharCode(" + bytes.toString() + ")");
}

function new_file_input_stream(file, buffered) {
  var stream =
      Cc["@mozilla.org/network/file-input-stream;1"].
      createInstance(Ci.nsIFileInputStream);
  stream.init(file, PR_RDONLY, 0, 0);
  if (!buffered)
    return stream;

  var buffer =
      Cc["@mozilla.org/network/buffered-input-stream;1"].
      createInstance(Ci.nsIBufferedInputStream);
  buffer.init(stream, 4096);
  return buffer;
}

function new_file_channel(file) {
  var ios =
      Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
  return ios.newChannelFromURI(ios.newFileURI(file));
}

/* stream listener */
function Listener(closure) {
  this._closure = closure;
}
Listener.prototype = {
  _closure: null,
  _buffer: "",
  _got_onstartrequest: false,
  _got_onstoprequest: false,
  _contentLen: -1,

  _isDir: function(request) {
    request.QueryInterface(Ci.nsIFileChannel);
    return request.file.isDirectory();
  },

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

    if (!this._isDir(request)) {
      request.QueryInterface(Ci.nsIChannel);
      this._contentLen = request.contentLength;
      if (this._contentLen == -1)
        do_throw("Content length is unknown in onStartRequest!");
    }
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
      do_throw("Failed to load file: " + status.toString(16));
    if (status != request.status)
      do_throw("request.status does not match status arg to onStopRequest!");
    if (request.isPending())
      do_throw("request reports itself as pending from onStopRequest!");
    if (this._contentLen != -1 && this._buffer.length != this._contentLen)
      do_throw("did not read nsIChannel.contentLength number of bytes!");

    this._closure(this._buffer);
  }
};

function test_read_file() {
  dump("*** test_read_file\n");

  var file = getFile("XpcomLib");
  var chan = new_file_channel(file);

  function on_read_complete(data) {
    dump("*** test_read_file.on_read_complete\n");

    /* read completed successfully.  now read data directly from file,
       and compare the result. */
    var stream = new_file_input_stream(file, false);   
    var result = read_stream(stream, stream.available());
    if (result != data)
      do_throw("Stream contents do not match with direct read!");
    run_next_test();
  }

  chan.asyncOpen(new Listener(on_read_complete), null);
}

function test_read_dir() {
  dump("*** test_read_dir\n");

  var file = getFile("TmpD");
  var chan = new_file_channel(file);

  function on_read_complete(data) {
    dump("*** test_read_dir.on_read_complete\n");

    /* read of directory completed successfully. */
    if (chan.contentType != "application/http-index-format")
      do_throw("Unexpected content type!");
    run_next_test();
  }

  chan.asyncOpen(new Listener(on_read_complete), null);
}

function test_upload_file() {
  dump("*** test_upload_file\n");

  var file = getFile("XpcomLib");  // file to upload
  var dest = getFile("TmpD");      // file upload destination
  dest.append("junk.dat");
  dest.createUnique(dest.NORMAL_FILE_TYPE, 0600);

  var uploadstream = new_file_input_stream(file, true);

  var chan = new_file_channel(dest);
  chan.QueryInterface(Ci.nsIUploadChannel);
  chan.setUploadStream(uploadstream, "", file.fileSize);

  function on_upload_complete(data) {
    dump("*** test_upload_file.on_upload_complete\n");

    /* upload of file completed successfully. */
    if (data.length != 0)
      do_throw("Upload resulted in data!");

    var oldstream = new_file_input_stream(file, false);   
    var newstream = new_file_input_stream(dest, false);   
    var olddata = read_stream(oldstream, oldstream.available());
    var newdata = read_stream(newstream, newstream.available());
    if (olddata != newdata)
      do_throw("Stream contents do not match after file copy!");
    oldstream.close();
    newstream.close();

    /* cleanup... also ensures that the destination file is not in 
       use when OnStopRequest is called. */
    try {
      dest.remove(false);
    } catch (e) {
      dump(e + "\n");
      do_throw("Unable to remove uploaded file!\n");
    }
    
    run_next_test();
  }

  chan.asyncOpen(new Listener(on_upload_complete), null);
}

function run_test() {
  do_test_pending();
  run_next_test();
}
