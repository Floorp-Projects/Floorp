// Test nsIThrottledInputChannel interface.

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

function test_handler(metadata, response) {
  const originalBody = "the response";
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(originalBody, originalBody.length);
}

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true})
                .QueryInterface(Ci.nsIHttpChannel);
}

function run_test() {
  let httpserver = new HttpServer();
  httpserver.registerPathHandler("/testdir", test_handler);
  httpserver.start(-1);

  const PORT = httpserver.identity.primaryPort;
  const size = 4096;

  let sstream = Cc["@mozilla.org/io/string-input-stream;1"].
                  createInstance(Ci.nsIStringInputStream);
  sstream.data = 'x'.repeat(size);

  let mime = Cc["@mozilla.org/network/mime-input-stream;1"].
               createInstance(Ci.nsIMIMEInputStream);
  mime.addHeader("Content-Type", "multipart/form-data; boundary=zzzzz");
  mime.setData(sstream);

  let tq = Cc["@mozilla.org/network/throttlequeue;1"]
      .createInstance(Ci.nsIInputChannelThrottleQueue);
  // Make sure the request takes more than one read.
  tq.init(100 + size / 2, 100 + size / 2);

  let channel = make_channel("http://localhost:" + PORT + "/testdir");
  channel.QueryInterface(Ci.nsIUploadChannel)
         .setUploadStream(mime, "", mime.available());
  channel.requestMethod = "POST";

  let tic = channel.QueryInterface(Ci.nsIThrottledInputChannel);
  tic.throttleQueue = tq;

  let startTime = Date.now();
  channel.asyncOpen(new ChannelListener(() => {
    ok(Date.now() - startTime > 1000, "request took more than one second");

    httpserver.stop(do_test_finished);
  }));

  do_test_pending();
}
