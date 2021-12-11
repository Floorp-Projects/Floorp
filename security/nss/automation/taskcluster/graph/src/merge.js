/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {recursive as merge} from "merge";

// We always want to clone.
export default function (...args) {
  return merge(true, ...args);
}
