/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//Basic tests to verify that MacSharingService returns expected data

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

function test_getSharingProviders()
{
  let sharingService = Cc["@mozilla.org/widget/macsharingservice;1"].
      getService(Ci.nsIMacSharingService);
  let providers = sharingService.getSharingProviders("http://example.org");
  Assert.ok(providers.length > 1, "There are providers returned");
  providers.forEach(provider => {
    Assert.ok("title" in provider, "Provider has title");
    Assert.ok("menuItemTitle" in provider, "Provider has menuItemTitle");
    Assert.ok("image" in provider, "Provider has image");

    Assert.notEqual(provider.title, "Mail", "Known filtered provider not returned");
  });
}

function run_test()
{
  test_getSharingProviders();
}
