/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MainProcessSession"];

const { Session } = ChromeUtils.import(
  "chrome://remote/content/sessions/Session.jsm"
);

/**
 * A session, dedicated to the main process target.
 * For some reason, it doesn't need any specific code and can share the base Session class
 * aside TabSession.
 */
class MainProcessSession extends Session {}
