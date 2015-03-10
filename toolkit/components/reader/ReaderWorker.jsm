/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling readability parsing.
 */

const Cu = Components.utils;

Cu.import("resource://gre/modules/PromiseWorker.jsm", this);

this.EXPORTED_SYMBOLS = ["ReaderWorker"];

this.ReaderWorker = new BasePromiseWorker("resource://gre/modules/reader/ReaderWorker.js");
