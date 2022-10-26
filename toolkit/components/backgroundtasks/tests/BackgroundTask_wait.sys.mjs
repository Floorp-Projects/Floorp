/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";

export function runBackgroundTask(commandLine) {
  let delay = 10;
  if (commandLine.length) {
    delay = Number.parseInt(commandLine.getArgument(0));
  }

  console.error(`runBackgroundTask: wait ${delay} seconds`);

  return new Promise(resolve => setTimeout(resolve, delay * 1000));
}
