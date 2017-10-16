/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function() {
  let blocklist = AM_Cc["@mozilla.org/extensions/blocklist;1"].
                  getService().wrappedJSObject;
  let scope = Components.utils.import("resource://gre/modules/osfile.jsm", {});

  // sync -> async
  blocklist._loadBlocklist();
  do_check_true(blocklist._isBlocklistLoaded());
  await blocklist._preloadBlocklist();
  do_check_false(blocklist._isBlocklistPreloaded());
  blocklist._clear();

  // async -> sync
  await blocklist._preloadBlocklist();
  do_check_false(blocklist._isBlocklistLoaded());
  do_check_true(blocklist._isBlocklistPreloaded());
  blocklist._loadBlocklist();
  do_check_true(blocklist._isBlocklistLoaded());
  do_check_false(blocklist._isBlocklistPreloaded());
  blocklist._clear();

  // async -> sync -> async
  let read = scope.OS.File.read;
  scope.OS.File.read = function(...args) {
    return new Promise((resolve, reject) => {
      do_execute_soon(() => {
        blocklist._loadBlocklist();
        resolve(read(...args));
      });
    });
  };

  await blocklist._preloadBlocklist();
  do_check_true(blocklist._isBlocklistLoaded());
  do_check_false(blocklist._isBlocklistPreloaded());
});
