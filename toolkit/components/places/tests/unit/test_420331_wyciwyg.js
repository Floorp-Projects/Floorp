/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  var testURI = uri("wyciwyg://nodontjudgeabookbyitscover");

  try
  {
    yield PlacesTestUtils.addVisits(testURI);
    do_throw("Should have generated an exception.");
  } catch (ex if ex && ex.result == Cr.NS_ERROR_ILLEGAL_VALUE) {
    // Adding wyciwyg URIs should raise NS_ERROR_ILLEGAL_VALUE.
  }
});
