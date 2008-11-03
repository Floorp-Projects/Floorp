Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/auth.js");
Cu.import("resource://weave/identity.js");
Cu.import("resource://weave/base_records/wbo.js");

Function.prototype.async = Async.sugar;

let logger;
let Httpd = {};
Cu.import("resource://tests/lib/httpd.js", Httpd);

function server_handler(metadata, response) {
  let body = '{"guid": "asdf-1234-asdf-1234", "type": ["object"]}';
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.bodyOutputStream.write(body, body.length);
}

function async_test() {
  let self = yield;

  logger = Log4Moz.repository.getLogger('Test');
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  let server = new Httpd.nsHttpServer();
  server.registerPathHandler("/record", server_handler);
  server.start(8080);

  let res = new WBORecord("http://localhost:8080/record");
  let rec = yield res.get(self.cb);
  do_check_eq(rec.guid, "asdf-1234-asdf-1234");
  do_check_eq(rec.type[0], "object");
  do_check_eq(res.lastRequest.status, 200);

  do_test_finished();
  server.stop();
    
  self.done();
}

function run_test() {
  async_test.async(this);
  do_test_pending();
}
