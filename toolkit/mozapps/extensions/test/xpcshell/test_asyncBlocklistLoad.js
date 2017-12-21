/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function() {
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService().wrappedJSObject;
  let scope = Components.utils.import("resource://gre/modules/osfile.jsm", {});

  // sync -> async
  blocklist._loadBlocklist();
  Assert.ok(blocklist._isBlocklistLoaded());
  await blocklist._preloadBlocklist();
  Assert.ok(!blocklist._isBlocklistPreloaded());
  blocklist._clear();

  // async -> sync
  await blocklist._preloadBlocklist();
  Assert.ok(!blocklist._isBlocklistLoaded());
  Assert.ok(blocklist._isBlocklistPreloaded());
  blocklist._loadBlocklist();
  Assert.ok(blocklist._isBlocklistLoaded());
  Assert.ok(!blocklist._isBlocklistPreloaded());
  blocklist._clear();

  // async -> sync -> async
  let read = scope.OS.File.read;
  scope.OS.File.read = function(...args) {
    return new Promise((resolve, reject) => {
      executeSoon(() => {
        blocklist._loadBlocklist();
        resolve(read(...args));
      });
    });
  };

  await blocklist._preloadBlocklist();
  Assert.ok(blocklist._isBlocklistLoaded());
  Assert.ok(!blocklist._isBlocklistPreloaded());
});
