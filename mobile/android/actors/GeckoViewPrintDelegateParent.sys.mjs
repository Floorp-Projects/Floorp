/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorParent } from "resource://gre/modules/GeckoViewActorParent.sys.mjs";

export class GeckoViewPrintDelegateParent extends GeckoViewActorParent {
  constructor() {
    super();
    this._browserStaticClone = null;
  }

  set browserStaticClone(staticClone) {
    this._browserStaticClone = staticClone;
  }

  get browserStaticClone() {
    return this._browserStaticClone;
  }

  clearStaticClone() {
    // Removes static browser element from DOM that was made for window.print
    this.browserStaticClone?.remove();
    this.browserStaticClone = null;
  }

  telemetryDotPrintRequested() {
    Glean.dotprint.requested.add(1);
  }

  telemetryDotPrintPdfCompleted(status) {
    if (status.isPdfSuccessful) {
      Glean.dotprint.androidDialogRequested.add(1);
    } else {
      var reason = "";
      switch (status.errorReason) {
        // ERROR_PRINT_SETTINGS_SERVICE_NOT_AVAILABLE
        case -1: {
          reason = "no_settings_service";
          break;
        }
        // ERROR_UNABLE_TO_CREATE_PRINT_SETTINGS
        case -2: {
          reason = "no_settings";
          break;
        }
        // ERROR_UNABLE_TO_RETRIEVE_CANONICAL_BROWSING_CONTEXT
        case -3: {
          reason = "no_canonical_context";
          break;
        }
        // ERROR_NO_ACTIVITY_CONTEXT_DELEGATE
        case -4: {
          reason = "no_activity_context_delegate";
          break;
        }
        // ERROR_NO_ACTIVITY_CONTEXT
        case -5: {
          reason = "no_activity_context";
          break;
        }
        default:
          reason = "unknown";
      }
      Glean.dotprint.failure[reason].add(1);
    }
  }

  printRequest() {
    if (this.browserStaticClone != null) {
      this.telemetryDotPrintRequested();
      this.eventDispatcher.sendRequest({
        type: "GeckoView:DotPrintRequest",
        canonicalBrowsingContextId: this.browserStaticClone.browsingContext.id,
      });
    }
  }
}
