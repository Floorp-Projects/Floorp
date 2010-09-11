Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function send(body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  };
}

function run_test() {
  let collection_usage = {steam:  65.11328,
                          petrol: 82.488281,
                          diesel: 2.25488281};
  let quota = [2169.65136, 8192];

  do_test_pending();
  let server = httpd_setup({
    "/1.0/johndoe/info/collection_usage": send(JSON.stringify(collection_usage)),
    "/1.0/johndoe/info/quota":            send(JSON.stringify(quota)),
    "/1.0/janedoe/info/collection_usage": send("gargabe"),
    "/1.0/janedoe/info/quota":            send("more garbage")
  });

  try {
    Weave.Service.clusterURL = "http://localhost:8080/";
    Weave.Service.username = "johndoe";

    _("Test getCollectionUsage().");
    let res = Weave.Service.getCollectionUsage();
    do_check_true(Utils.deepEquals(res, collection_usage));

    _("Test getQuota().");
    res = Weave.Service.getQuota();
    do_check_true(Utils.deepEquals(res, quota));

    _("Both return 'null' for non-200 responses.");
    Weave.Service.username = "nonexistent";
    do_check_eq(Weave.Service.getCollectionUsage(), null);
    do_check_eq(Weave.Service.getQuota(), null);

    _("Both return nothing (undefined) if the return value can't be parsed.");
    Weave.Service.username = "janedoe";
    do_check_eq(Weave.Service.getCollectionUsage(), undefined);
    do_check_eq(Weave.Service.getQuota(), undefined);

  } finally {
    server.stop(do_test_finished);
  }
}
