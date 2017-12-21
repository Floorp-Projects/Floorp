/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals __LOCATION__ */

function run_test() {
  Assert.equal(__LOCATION__.leafName, "test_location.js");
  // also check that __LOCATION__ works via load()
  load("location_load.js");
}
