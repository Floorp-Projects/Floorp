/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from ../data/xpcshellUtilsAUS.js */
load("xpcshellUtilsAUS.js");
gIsServiceTest = false;

const { BackgroundTasksTestUtils } = ChromeUtils.import(
  "resource://testing-common/BackgroundTasksTestUtils.jsm"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);
const setupProfileService = BackgroundTasksTestUtils.setupProfileService.bind(
  BackgroundTasksTestUtils
);
