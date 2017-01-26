/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/service.js");

function run_test() {
  Service.serverURL = "http://example.com/sync";
  do_check_eq(Service.serverURL, "http://example.com/sync/");

  Service.serverURL = "http://example.com/sync/";
  do_check_eq(Service.serverURL, "http://example.com/sync/");
}

