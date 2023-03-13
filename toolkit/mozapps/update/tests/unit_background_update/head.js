/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from ../data/xpcshellUtilsAUS.js */
load("xpcshellUtilsAUS.js");
gIsServiceTest = false;

const { BackgroundTasksTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BackgroundTasksTestUtils.sys.mjs"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);
const setupProfileService = BackgroundTasksTestUtils.setupProfileService.bind(
  BackgroundTasksTestUtils
);

// Helper function to register a callback to catch a Glean ping before its
// submission. The function returns all string_list items, but not as they
// appear in the ping itself, but as full text representation, which is the
// value of the corresponding field. This makes the test more unique, because
// the values often contain chars, which are not allowed in glean metric labels
//
// @returns: an array which contains all glean metrics, but as full text
//           representation from the BackgroundUpdate.REASON object => its
//           values, see description for further details.
//
async function checkGleanPing() {
  let retval = ["EMPTY"];
  let ping_submitted = false;

  const { maybeSubmitBackgroundUpdatePing } = ChromeUtils.importESModule(
    "resource://gre/modules/backgroundtasks/BackgroundTask_backgroundupdate.sys.mjs"
  );
  const { BackgroundUpdate } = ChromeUtils.importESModule(
    "resource://gre/modules/BackgroundUpdate.sys.mjs"
  );

  GleanPings.backgroundUpdate.testBeforeNextSubmit(_ => {
    ping_submitted = true;
    retval = Glean.backgroundUpdate.reasonsToNotUpdate.testGetValue().map(v => {
      return BackgroundUpdate.REASON[v];
    });
    Assert.ok(Array.isArray(retval));
    return retval;
  });
  await maybeSubmitBackgroundUpdatePing();
  Assert.ok(ping_submitted, "Glean ping successfully submitted");

  // The metric has `lifetime: application` set, but when testing we do not
  // want to keep the results around and avoid, that one test can influence
  // another. That is why we clear this string_list.
  Glean.backgroundUpdate.reasonsToNotUpdate.set([]);

  return retval;
}
