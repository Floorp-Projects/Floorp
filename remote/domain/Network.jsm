/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Network"];

const {t} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

var Network = {
  MonotonicTime: {schema: t.Number},
  LoaderId: {schema: t.String},
  RequestId: {schema: t.Number},
};
