/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/MobileIdentityClient.jsm");

/* Setup */

do_get_profile();

const PREF_FORCE_HTTPS = "services.mobileid.forcehttps";
Services.prefs.setBoolPref(PREF_FORCE_HTTPS, false);

let client;
let server = new HttpServer();

function httpd_setup(handlers, port = -1) {
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }

  try {
    server.start(port);
  } catch (ex) {
    dump("ERROR starting server on port " + port + ".  Already a process listening?");
    do_throw(ex);
  }

  // Set the base URI for convenience.
  let i = server.identity;
  server.baseURI = i.primaryScheme + "://" + i.primaryHost + ":" + i.primaryPort;

  return server;
}

function writeResp(response, msg) {
  if (typeof msg === "object") {
    msg = JSON.stringify(msg);
  }
  response.bodyOutputStream.write(msg, msg.length);
}

function getBody(request) {
  let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
  return JSON.parse(body);
}

function run_test() {
  httpd_setup({
    "/discover": function(request, response) {
      let body = getBody(request);
      response.setStatusLine(null, 200, "Ok");
      writeResp(response, {
        msisdn: body.msisdn,
        mcc: body.mcc,
        mnc: body.mnc,
        roaming: body.roaming
      });
    },
    "/register": function(request, response) {
      response.setStatusLine(null, 204);
    },
    "/sms/mt/verify": function(request, response) {
      let body = getBody(request);
      response.setStatusLine(null, 200, "Ok");
      writeResp(response, {
        msisdn: body.msisdn,
        mcc: body.mcc,
        mnc: body.mnc,
        shortVerificationCode: body.shortVerificationCode
      });
    },
    "/sms/verify_code": function(request, response) {
      let body = getBody(request);
      response.setStatusLine(null, 200, "Ok");
      writeResp(response, {
        code: body.code
      });
    },
    "/certificate/sign": function(request, response) {
      let body = getBody(request);
      response.setStatusLine(null, 200, "Ok");
      writeResp(response, {
        duration: body.duration,
        publicKey: body.publicKey
      });
    },
    "/unregister": function(request, response) {
      response.setStatusLine(null, 500);
    }
  });


  client = new MobileIdentityClient(server.baseURI);

  run_next_test();
}

/* Tests */

add_test(function test_discover() {
  do_print("= Test /discover =");

  do_test_pending();

  let msisdn = "msisdn";
  let mcc = "mcc";
  let mnc = "mnc";
  let roaming = true;

  client.discover(msisdn, mcc, mnc, roaming).then(
    (response) => {
      // The request should succeed and a response should be given.
      // We simply check that the parameters given with the request are
      // successfully progressed to the server.
      do_check_eq(response.msisdn, msisdn);
      do_check_eq(response.mcc, mcc);
      do_check_eq(response.mnc, mnc);
      do_check_eq(response.roaming, roaming);
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_throw("Should not throw error when handling a 200 response");
      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_sms_mt_verify() {
  do_print("= Test /sms/mt/verify =");

  do_test_pending();

  let msisdn = "msisdn";
  let mcc = "mcc";
  let mnc = "mnc";
  let shortCode = true;

  client.smsMtVerify("aToken", msisdn, mcc, mnc, shortCode).then(
    (response) => {
      // The request should succeed and a response should be given.
      // We simply check that the parameters given with the request are
      // successfully progressed to the server.
      do_check_eq(response.msisdn, msisdn);
      do_check_eq(response.mcc, mcc);
      do_check_eq(response.mnc, mnc);
      do_check_eq(response.shortVerificationCode, shortCode);
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_throw("Should not throw error when handling a 200 response");
      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_verify_code() {
  do_print("= Test /sms/verify_code =");

  do_test_pending();

  let code = "code";

  client.verifyCode("aToken", code).then(
    (response) => {
      // The request should succeed and a response should be given.
      // We simply check that the parameters given with the request are
      // successfully progressed to the server.
      do_check_eq(response.code, code);
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_throw("Should not throw error when handling a 200 response");
      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_certificate_sign() {
  do_print("= Test /certificate/sign =");

  do_test_pending();

  let duration = "duration";
  let publicKey = "publicKey";

  client.sign("aToken", duration, publicKey).then(
    (response) => {
      // The request should succeed and a response should be given.
      // We simply check that the parameters given with the request are
      // successfully progressed to the server.
      do_check_eq(response.duration, duration);
      do_check_eq(response.publicKey, publicKey);
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_throw("Should not throw error when handling a 200 response");
      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_204_response() {
  do_print("= Test /register and 204 No Content response =");

  do_test_pending();

  client.register().then(
    (response) => {
      // The request should succeed but no response body should be returned.
      do_check_eq(response, undefined);
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_throw("Should not throw error when handling a 204 response");
      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_500_response() {
  do_print("= Test /unregister and 500 response =");

  do_test_pending();

  client.unregister("aToken").then(
    (response) => {
      // The request shouldn't succeed.
      do_throw("We are supposed to get an error");
      do_test_finished();
      run_next_test();
    },
    (error) => {
      do_check_eq(error, "UNKNOWN");
      do_test_finished();
      run_next_test();
    }
  );
});
