/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Interface to a dedicated thread handling readability parsing.
 */

import { BasePromiseWorker } from "resource://gre/modules/PromiseWorker.sys.mjs";

export var ReaderWorker = new BasePromiseWorker(
  "resource://gre/modules/reader/Reader.worker.js"
);
