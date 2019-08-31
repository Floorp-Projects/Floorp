/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.getinfooffline = class extends ExtensionAPI {
  onStartup() {
    Services.io.offline = true;
  }
};
