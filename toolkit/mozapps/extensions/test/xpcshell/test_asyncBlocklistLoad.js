/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function() {
  let blocklist = Blocklist;
  let scope = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});

  // sync -> async. Check that async code doesn't try to read the file
  // once it's already been read synchronously.
  let read = scope.OS.File.read;
  let triedToRead = false;
  scope.OS.File.read = () => triedToRead = true;
  blocklist._loadBlocklist();
  Assert.ok(blocklist.isLoaded);
  await blocklist.loadBlocklistAsync();
  Assert.ok(!triedToRead);
  scope.OS.File.read = read;
  blocklist._clear();

  info("sync -> async complete");

  // async first. Check that once we preload the content, that is sufficient.
  await blocklist.loadBlocklistAsync();
  Assert.ok(blocklist.isLoaded);
  // Calling _loadBlocklist now would just re-load the list sync.

  info("async test complete");
  blocklist._clear();

  // async -> sync -> async
  scope.OS.File.read = function(...args) {
    return new Promise((resolve, reject) => {
      executeSoon(() => {
        blocklist._loadBlocklist();
        // Now do the async bit after all:
        resolve(read(...args));
      });
    });
  };

  await blocklist.loadBlocklistAsync();
  // We're mostly just checking this doesn't error out.
  Assert.ok(blocklist.isLoaded);
  info("mixed async/sync test complete");
});
