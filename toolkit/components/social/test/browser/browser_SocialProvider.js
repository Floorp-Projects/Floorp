/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = {
    origin: 'http://example.com',
    name: "Example Provider",
    workerURL: "http://example.com/browser/toolkit/components/social/test/browser/worker_social.js"
  };

  ensureSocialEnabled();

  SocialService.addProvider(manifest, function (provider) {
    ok(provider.enabled, "provider is initially enabled");
    ok(provider.port, "should be able to get a port from enabled provider");
    ok(provider.workerAPI, "should be able to get a workerAPI from enabled provider");

    provider.enabled = false;

    ok(!provider.enabled, "provider is now disabled");
    ok(!provider.port, "shouldn't be able to get a port from disabled provider");
    ok(!provider.workerAPI, "shouldn't be able to get a workerAPI from disabled provider");

    provider.enabled = true;

    ok(provider.enabled, "provider is re-enabled");
    ok(provider.port, "should be able to get a port from re-enabled provider");
    ok(provider.workerAPI, "should be able to get a workerAPI from re-enabled provider");

    SocialService.removeProvider(provider.origin, finish);
  });
}
