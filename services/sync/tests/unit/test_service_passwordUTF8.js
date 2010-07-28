Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/service.js");

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

function info_collections(request, response) {
  let body, statusCode, status;
  let basic = basicauth[server_password];

  if (basic && (request.getHeader("Authorization") == basic)) {
    body = "{}";
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
  let server = httpd_setup({
    "/1.0/johndoe/info/collections": info_collections,
    "/user/1.0/johndoe/password": change_password
  });

  Weave.Service.username = "johndoe";
  Weave.Service.password = JAPANESE;
  Weave.Service.passphrase = "Must exist, but contents irrelevant.";
  Weave.Service.serverURL = "http://localhost:8080/";

  try {
    _("Try to log in with the password.");
    server_password = "foobar";
    do_check_false(Weave.Service.verifyLogin());
    do_check_eq(server_password, "foobar");

    _("Make the server password the low byte version of our password.  Login should work and have transparently changed the password to the UTF8 version.");
    server_password = LOWBYTES;
    do_check_true(Weave.Service.verifyLogin());
    do_check_eq(server_password, Utils.encodeUTF8(JAPANESE));

    _("Can't use a password that has the same low bytes as ours.");
    Weave.Service.password = APPLES;
    do_check_false(Weave.Service.verifyLogin());
    do_check_eq(server_password, Utils.encodeUTF8(JAPANESE));

  } finally {
    server.stop(function() {});
    Weave.Svc.Prefs.resetBranch("");
  }
}
