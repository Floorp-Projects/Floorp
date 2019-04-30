/* run some tests on the file:// protocol handler */


const PR_RDONLY = 0x1;  // see prio.h

const special_type = "application/x-our-special-type";

[
  test_read_file,
  test_read_dir_1,
  test_read_dir_2,
  test_upload_file,
  test_load_replace,
  do_test_finished
].forEach(f => add_test(f));

function getFile(key) {
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"]
                 .getService(Ci.nsIProperties);
  return dirSvc.get(key, Ci.nsIFile);
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
  return NetUtil.newChannel({
    uri: ios.newFileURI(file),
    loadUsingSystemPrincipal: true
  });
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

  _isDir: function(request) {
    request.QueryInterface(Ci.nsIFileChannel);
    return request.file.isDirectory();
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request) {
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

  onDataAvailable: function(request, stream, offset, count) {
    if (!this._got_onstartrequest)
      do_throw("onDataAvailable without onStartRequest event!");
    if (this._got_onstoprequest)
      do_throw("onDataAvailable after onStopRequest event!");
    if (!request.isPending())
      do_throw("request reports itself as not pending from onStartRequest!");

    this._buffer = this._buffer.concat(read_stream(stream, count));
  },

  onStopRequest: function(request, status) {
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

  var file = do_get_file("../unit/data/test_readline6.txt");
  var chan = new_file_channel(file);

  function on_read_complete(data) {
    dump("*** test_read_file.on_read_complete\n");

    // bug 326693
    if (chan.contentType != special_type)
      do_throw("Type mismatch! Is <" + chan.contentType + ">, should be <" +
               special_type + ">")

    /* read completed successfully.  now read data directly from file,
       and compare the result. */
    var stream = new_file_input_stream(file, false);   
    var result = read_stream(stream, stream.available());
    if (result != data)
      do_throw("Stream contents do not match with direct read!");
    run_next_test();
  }

  chan.contentType = special_type;
  chan.asyncOpen(new FileStreamListener(on_read_complete));
}

function do_test_read_dir(set_type, expected_type) {
  dump("*** test_read_dir(" + set_type + ", " + expected_type + ")\n");

  var file = do_get_tempdir();
  var chan = new_file_channel(file);

  function on_read_complete(data) {
    dump("*** test_read_dir.on_read_complete(" + set_type + ", " + expected_type + ")\n");

    // bug 326693
    if (chan.contentType != expected_type)
      do_throw("Type mismatch! Is <" + chan.contentType + ">, should be <" +
               expected_type + ">")

    run_next_test();
  }

  if (set_type)
    chan.contentType = expected_type;
  chan.asyncOpen(new FileStreamListener(on_read_complete));
}

function test_read_dir_1() {
  return do_test_read_dir(false, "application/http-index-format");
}

function test_read_dir_2() {
  return do_test_read_dir(true, special_type);
}

function test_upload_file() {
  dump("*** test_upload_file\n");

  var file = do_get_file("../unit/data/test_readline6.txt"); // file to upload
  var dest = do_get_tempdir();      // file upload destination
  dest.append("junk.dat");
  dest.createUnique(dest.NORMAL_FILE_TYPE, 0o600);

  var uploadstream = new_file_input_stream(file, true);

  var chan = new_file_channel(dest);
  chan.QueryInterface(Ci.nsIUploadChannel);
  chan.setUploadStream(uploadstream, "", file.fileSize);

  function on_upload_complete(data) {
    dump("*** test_upload_file.on_upload_complete\n");

    // bug 326693
    if (chan.contentType != special_type)
      do_throw("Type mismatch! Is <" + chan.contentType + ">, should be <" +
               special_type + ">")

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

  chan.contentType = special_type;
  chan.asyncOpen(new FileStreamListener(on_upload_complete));
}

function test_load_replace() {
  // lnk files should resolve to their targets
  if (mozinfo.os == "win") {
    dump("*** test_load_replace\n");
    file = do_get_file("data/system_root.lnk", false);
    var chan = new_file_channel(file);

    // The original URI path should differ from the URI path
    Assert.notEqual(chan.URI.pathQueryRef, chan.originalURI.pathQueryRef);

    // The original URI path should be the same as the lnk file path
    var ios = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    Assert.equal(chan.originalURI.pathQueryRef, ios.newFileURI(file).pathQueryRef);
  }
  run_next_test();
}

function run_test() {
  run_next_test();
}
