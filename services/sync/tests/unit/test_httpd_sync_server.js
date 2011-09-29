function run_test() {
  run_next_test();
}

add_test(function test_creation() {
  // Explicit callback for this one.
  let s = new SyncServer({
    __proto__: SyncServerCallback,
  });
  do_check_true(!!s);       // Just so we have a check.
  s.start(null, function () {
    _("Started on " + s.port);
    s.stop(run_next_test);
  });
});

add_test(function test_url_parsing() {
  let s = new SyncServer();
  let parts = s.pathRE.exec("/1.1/johnsmith/storage/crypto/keys");
  let [all, version, username, first, rest] = parts;
  do_check_eq(version, "1.1");
  do_check_eq(username, "johnsmith");
  do_check_eq(first, "storage");
  do_check_eq(rest, "crypto/keys");
  do_check_eq(null, s.pathRE.exec("/nothing/else"));
  run_next_test();
});

Cu.import("resource://services-sync/rest.js");
function localRequest(path) {
  _("localRequest: " + path);
  let url = "http://127.0.0.1:8080" + path;
  _("url: " + url);
  return new RESTRequest(url);
}

add_test(function test_basic_http() {
  let s = new SyncServer();
  s.registerUser("john", "password");
  do_check_true(s.userExists("john"));
  s.start(8080, function () {
    _("Started on " + s.port);
    do_check_eq(s.port, 8080);
    Utils.nextTick(function () {
      let req = localRequest("/1.1/john/storage/crypto/keys");
      _("req is " + req);
      req.get(function (err) {
        do_check_eq(null, err);
        Utils.nextTick(function () {
          s.stop(run_next_test);
        });
      });
    });
  });
});

add_test(function test_info_collections() {
  let s = new SyncServer({
    __proto__: SyncServerCallback
  });
  function responseHasCorrectHeaders(r) {
    do_check_eq(r.status, 200);
    do_check_eq(r.headers["content-type"], "application/json");
    do_check_true("x-weave-timestamp" in r.headers);
  }

  s.registerUser("john", "password");
  s.start(8080, function () {
    do_check_eq(s.port, 8080);
    Utils.nextTick(function () {
      let req = localRequest("/1.1/john/info/collections");
      req.get(function (err) {
        // Initial info/collections fetch is empty.
        do_check_eq(null, err);
        responseHasCorrectHeaders(this.response);

        do_check_eq(this.response.body, "{}");
        Utils.nextTick(function () {
          // When we PUT something to crypto/keys, "crypto" appears in the response.
          function cb(err) {
            do_check_eq(null, err);
            responseHasCorrectHeaders(this.response);
            let putResponseBody = this.response.body;
            _("PUT response body: " + JSON.stringify(putResponseBody));

            req = localRequest("/1.1/john/info/collections");
            req.get(function (err) {
              do_check_eq(null, err);
              responseHasCorrectHeaders(this.response);
              let expectedColl = s.getCollection("john", "crypto");
              do_check_true(!!expectedColl);
              let modified = expectedColl.timestamp;
              do_check_true(modified > 0);
              do_check_eq(putResponseBody, modified);
              do_check_eq(JSON.parse(this.response.body).crypto, modified);
              Utils.nextTick(function () {
                s.stop(run_next_test);
              });
            });
          }
          let payload = JSON.stringify({foo: "bar"});
          localRequest("/1.1/john/storage/crypto/keys").put(payload, cb);
        });
      });
    });
  });
});

add_test(function test_storage_request() {
  let keysURL = "/1.1/john/storage/crypto/keys?foo=bar";
  let foosURL = "/1.1/john/storage/crypto/foos";
  let s = new SyncServer();
  let creation = s.timestamp();
  s.registerUser("john", "password");

  s.createContents("john", {
    crypto: {foos: {foo: "bar"}}
  });
  let coll = s.user("john").collection("crypto");
  do_check_true(!!coll);

  _("We're tracking timestamps.");
  do_check_true(coll.timestamp >= creation);

  function retrieveWBONotExists(next) {
    let req = localRequest(keysURL);
    req.get(function (err) {
      _("Body is " + this.response.body);
      _("Modified is " + this.response.newModified);
      do_check_eq(null, err);
      do_check_eq(this.response.status, 404);
      do_check_eq(this.response.body, "Not found");
      Utils.nextTick(next);
    });
  }
  function retrieveWBOExists(next) {
    let req = localRequest(foosURL);
    req.get(function (err) {
      _("Body is " + this.response.body);
      _("Modified is " + this.response.newModified);
      let parsedBody = JSON.parse(this.response.body);
      do_check_eq(parsedBody.id, "foos");
      do_check_eq(parsedBody.modified, coll.wbo("foos").modified);
      do_check_eq(JSON.parse(parsedBody.payload).foo, "bar");
      Utils.nextTick(next);
    });
  }
  s.start(8080, function () {
    retrieveWBONotExists(
      retrieveWBOExists.bind(this, function () {
        s.stop(run_next_test);
      })
    );
  });
});
