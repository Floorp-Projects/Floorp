/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic tests to verify that MacSharingService returns expected data

function test_getSharingProviders() {
  let sharingService = Cc["@mozilla.org/widget/macsharingservice;1"].getService(
    Ci.nsIMacSharingService
  );

  // Ensure these URL's are accepted without error by the getSharingProviders()
  // method. This does not test if the URL's are interpreted correctly by
  // the platform implementation and does not test that the URL will be
  // successfully shared to the target application if the shareURL method is
  // used. It does indicate the Mac API's used to get the sharing providers
  // successfully created a URL object for the URL provided and returned at
  // least one provider.
  let urls = [
    "http://example.org",
    "http://example.org/#",
    "http://example.org/dkl??",
    "http://example.org/dkl?a=b;c=d#thisisaref",
    "http://example.org/dkl?a=b;c=d#thisisaref#double",
    "http://example.org/#/",
    "http://example.org/#/#",
    "http://example.org/#/#/",
    "http://example.org/foo/bar/x|page.html#this_is_a_fragment",
    "http://example.org/page.html#this_is_a_fragment",
    "http://example.org/page.html#this_is_a_fragment#and_another",
    "http://example.org/foo/bar;#foo",
    "http://example.org/a file with spaces.html",
    "https://chat.mozilla.org/#/room/#macdev:mozilla.org",
    "https://chat.mozilla.org/#/room/%23macdev:mozilla.org",
  ];

  urls.forEach(url => testGetSharingProvidersForUrl(sharingService, url));
}

function testGetSharingProvidersForUrl(sharingService, url) {
  let providers = sharingService.getSharingProviders(url);
  Assert.greater(providers.length, 1, "There are providers returned");
  providers.forEach(provider => {
    Assert.ok("name" in provider, "Provider has name");
    Assert.ok("menuItemTitle" in provider, "Provider has menuItemTitle");
    Assert.ok("image" in provider, "Provider has image");

    Assert.notEqual(
      provider.title,
      "Mail",
      "Known filtered provider not returned"
    );
  });
}

function run_test() {
  test_getSharingProviders();
}
