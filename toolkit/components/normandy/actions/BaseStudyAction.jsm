/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { BaseAction } = ChromeUtils.import(
  "resource://normandy/actions/BaseAction.jsm"
);

var EXPORTED_SYMBOLS = ["BaseStudyAction"];

const OPT_OUT_STUDIES_ENABLED_PREF = "app.shield.optoutstudies.enabled";

/**
 * Base class for local study actions.
 *
 * This should be subclassed. Subclasses must implement _run() for
 * per-recipe behavior, and may implement _finalize for actions to be
 * taken once after recipes are run.
 *
 * For actions that need to be taken once before recipes are run
 * _preExecution may be overriden but the overridden method must
 * call the parent method to ensure the appropriate checks occur.
 *
 * Other methods should be overridden with care, to maintain the life
 * cycle events and error reporting implemented by this class.
 */
class BaseStudyAction extends BaseAction {
  _preExecution() {
    if (!Services.policies.isAllowed("Shield")) {
      this.log.debug("Disabling Shield because it's blocked by policy.");
      this.disable();
    }

    if (!Services.prefs.getBoolPref(OPT_OUT_STUDIES_ENABLED_PREF, true)) {
      this.log.debug(
        "User has opted-out of opt-out experiments, disabling action."
      );
      this.disable();
    }
  }
}
