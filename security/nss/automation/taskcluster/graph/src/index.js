/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as try_syntax from "./try_syntax";
import extend from "./extend";

const main = async () => {
  // Init try syntax filter.
  if (process.env.TC_PROJECT == "nss-try") {
    await try_syntax.initFilter();
  }

  // Extend the task graph.
  await extend();
};

main().catch(err => {
  console.error(err);
  process.exit(1);
});
