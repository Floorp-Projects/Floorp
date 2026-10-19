/* SPDX-License-Identifier: MPL-2.0 */
"use strict";

add_task(function floorp_qa_current_debug() {
  const { AppConstants } = ChromeUtils.importESModule(
    "resource://gre/modules/AppConstants.sys.mjs"
  );
  Assert.strictEqual(
    Cu.getJSTestingFunctions().getBuildConfiguration().debug,
    true,
    "The running JavaScript engine was compiled with DEBUG"
  );
  Assert.strictEqual(AppConstants.DEBUG, true, "The running application uses DEBUG");
  Assert.strictEqual(
    AppConstants.MOZ_APP_VERSION,
    "158.0.1",
    "The running application matches the fixed current source"
  );
  info("FLOORP_QA_COMPILED_DEBUG_158_0_1_V1");
});
