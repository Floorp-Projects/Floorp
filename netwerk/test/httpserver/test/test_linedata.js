/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// test that the LineData internal data structure works correctly

function run_test()
{
  var data = new LineData();
  data.appendBytes(["a".charCodeAt(0), CR]);

  var out = { value: "" };
  do_check_false(data.readLine(out));

  data.appendBytes([LF]);
  do_check_true(data.readLine(out));
  do_check_eq(out.value, "a");
}
