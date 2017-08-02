Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/status.js");

function run_test() {

  // Check initial states
  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);

  do_check_eq(Status.service, STATUS_OK);
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_eq(Status.login, LOGIN_SUCCEEDED);
  if (Status.engines.length) {
    do_throw("Status.engines should be empty.");
  }
  do_check_eq(Status.partial, false);


  // Check login status
  for (let code of [LOGIN_FAILED_NO_USERNAME,
                    LOGIN_FAILED_NO_PASSPHRASE]) {
    Status.login = code;
    do_check_eq(Status.login, code);
    do_check_eq(Status.service, CLIENT_NOT_CONFIGURED);
    Status.resetSync();
  }

  Status.login = LOGIN_FAILED;
  do_check_eq(Status.login, LOGIN_FAILED);
  do_check_eq(Status.service, LOGIN_FAILED);
  Status.resetSync();

  Status.login = LOGIN_SUCCEEDED;
  do_check_eq(Status.login, LOGIN_SUCCEEDED);
  do_check_eq(Status.service, STATUS_OK);
  Status.resetSync();


  // Check sync status
  Status.sync = SYNC_FAILED;
  do_check_eq(Status.sync, SYNC_FAILED);
  do_check_eq(Status.service, SYNC_FAILED);

  Status.sync = SYNC_SUCCEEDED;
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  do_check_eq(Status.service, STATUS_OK);

  Status.resetSync();


  // Check engine status
  Status.engines = ["testEng1", ENGINE_SUCCEEDED];
  do_check_eq(Status.engines.testEng1, ENGINE_SUCCEEDED);
  do_check_eq(Status.service, STATUS_OK);

  Status.engines = ["testEng2", ENGINE_DOWNLOAD_FAIL];
  do_check_eq(Status.engines.testEng1, ENGINE_SUCCEEDED);
  do_check_eq(Status.engines.testEng2, ENGINE_DOWNLOAD_FAIL);
  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);

  Status.engines = ["testEng3", ENGINE_SUCCEEDED];
  do_check_eq(Status.engines.testEng1, ENGINE_SUCCEEDED);
  do_check_eq(Status.engines.testEng2, ENGINE_DOWNLOAD_FAIL);
  do_check_eq(Status.engines.testEng3, ENGINE_SUCCEEDED);
  do_check_eq(Status.service, SYNC_FAILED_PARTIAL);


  // Check resetSync
  Status.sync = SYNC_FAILED;
  Status.resetSync();

  do_check_eq(Status.service, STATUS_OK);
  do_check_eq(Status.sync, SYNC_SUCCEEDED);
  if (Status.engines.length) {
    do_throw("Status.engines should be empty.");
  }


  // Check resetBackoff
  Status.enforceBackoff = true;
  Status.backOffInterval = 4815162342;
  Status.backOffInterval = 42;
  Status.resetBackoff();

  do_check_false(Status.enforceBackoff);
  do_check_eq(Status.backoffInterval, 0);
  do_check_eq(Status.minimumNextSync, 0);

}
