/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test() {
  print("Init the fake idle service and check its identity.");
  let fakeIdleService = Cc["@mozilla.org/widget/useridleservice;1"].getService(
    Ci.nsIUserIdleService
  );
  try {
    fakeIdleService.QueryInterface(Ci.nsIFactory);
  } catch (ex) {
    do_throw("The fake idle service implements nsIFactory.");
  }
  // We need at least one PASS, thus sanity check the idle time.
  Assert.equal(fakeIdleService.idleTime, 0);

  print("Init the real idle service and check its identity.");
  let realIdleService = do_get_idle();
  try {
    realIdleService.QueryInterface(Ci.nsIFactory);
    do_throw("The real idle service does not implement nsIFactory.");
  } catch (ex) {}
}
