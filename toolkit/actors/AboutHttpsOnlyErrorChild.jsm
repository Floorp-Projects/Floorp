/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutHttpsOnlyErrorChild"];

const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

class AboutHttpsOnlyErrorChild extends RemotePageChild {}
