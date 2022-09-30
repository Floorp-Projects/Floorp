/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ContentProcessDomain } from "chrome://remote/content/cdp/domains/ContentProcessDomain.sys.mjs";

export class Network extends ContentProcessDomain {
  // commands

  /**
   * Internal methods: the following methods are not part of CDP;
   * note the _ prefix.
   */

  _updateLoadFlags(flags) {
    this.docShell.defaultLoadFlags = flags;
  }
}
