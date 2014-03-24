/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://testing-common/services/sync/utils.js");

const JAPANESE = "\u34ff\u35ff\u36ff\u37ff";
const APPLES = "\uf8ff\uf8ff\uf8ff\uf8ff";
const LOWBYTES = "\xff\xff\xff\xff";

// Poor man's /etc/passwd.  Static since there's no btoa()/atob() in xpcshell.
let basicauth = {};
basicauth[LOWBYTES] = "Basic am9obmRvZTr/////";
basicauth[Utils.encodeUTF8(JAPANESE)] = "Basic am9obmRvZTrjk7/jl7/jm7/jn78=";

// Global var for the server password, read by info_collections(),
// modified by change_password().
let server_password;

function login_handling(handler) {
  return function (request, response) {
    let basic = basicauth[server_password];

    if (basic && (request.getHeader("Authorization") == basic)) {
      handler(request, response);
    } else {
      let body = "Unauthorized";
      response.setStatusLine(request.httpVersion, 401, "Unauthorized");
      response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

function change_password(request, response) {
  let body, statusCode, status;
  let basic = basicauth[server_password];

  if (basic && (request.getHeader("Authorization") == basic)) {
    server_password = readBytesFromInputStream(request.bodyInputStream);
    body = "";
    statusCode = 200;
    status = "OK";
  } else {
    statusCode = 401;
    body = status = "Unauthorized";
  }
  response.setStatusLine(request.httpVersion, statusCode, status);
  response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  response.bodyOutputStream.write(body, body.length);
}

function run_test() {
  initTestLogging("Trace");
  let collectionsHelper = track_collections_helper();
  let upd = collectionsHelper.with_updated_collection;
  let collections = collectionsHelper.collections;

  ensureLegacyIdentityManager();

  do_test_pending();
  let server = httpd_setup({
    "/1.1/johndoe/info/collections":    login_handling(collectionsHelper.handler),
    "/1.1/johndoe/storage/meta/global": upd("meta",   new ServerWBO("global").handler()),
    "/1.1/johndoe/storage/crypto/keys": upd("crypto", new ServerWBO("keys").handler()),
    "/user/1.0/johndoe/password":       change_password
  });

  setBasicCredentials("johndoe", JAPANESE, "irrelevant");
  Service.serverURL = server.baseURI;

  try {
    _("Try to log in with the password.");
    server_password = "foobar";
    do_check_false(Service.verifyLogin());
    do_check_eq(server_password, "foobar");

    _("Make the server password the low byte version of our password.");
    server_password = LOWBYTES;
    do_check_false(Service.verifyLogin());
    do_check_eq(server_password, LOWBYTES);

    _("Can't use a password that has the same low bytes as ours.");
    server_password = Utils.encodeUTF8(JAPANESE);
    Service.identity.basicPassword = APPLES;
    do_check_false(Service.verifyLogin());
    do_check_eq(server_password, Utils.encodeUTF8(JAPANESE));

  } finally {
    server.stop(do_test_finished);
    Svc.Prefs.resetBranch("");
  }
}
