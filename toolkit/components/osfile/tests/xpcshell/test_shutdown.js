ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/Promise.jsm", this);
ChromeUtils.import("resource://gre/modules/osfile.jsm", this);

add_task(function init() {
  do_get_profile();
});

/**
 * Test logging of file descriptors leaks.
 */
add_task(async function system_shutdown() {
  // Test that unclosed files cause warnings
  // Test that unclosed directories cause warnings
  // Test that closed files do not cause warnings
  // Test that closed directories do not cause warnings
  function testLeaksOf(resource, topic) {
    return (async function() {
      let deferred = Promise.defer();

      // Register observer
      Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
      Services.prefs.setBoolPref("toolkit.osfile.log", true);
      Services.prefs.setBoolPref("toolkit.osfile.log.redirect", true);
      Services.prefs.setCharPref(
        "toolkit.osfile.test.shutdown.observer",
        topic
      );

      let observer = function(aMessage) {
        try {
          info("Got message: " + aMessage);
          if (!(aMessage instanceof Ci.nsIConsoleMessage)) {
            return;
          }
          let message = aMessage.message;
          info("Got message: " + message);
          if (!message.includes("TEST OS Controller WARNING")) {
            return;
          }
          info(
            "Got message: " + message + ", looking for resource " + resource
          );
          if (!message.includes(resource)) {
            return;
          }
          info("Resource: " + resource + " found");
          executeSoon(deferred.resolve);
        } catch (ex) {
          executeSoon(function() {
            deferred.reject(ex);
          });
        }
      };
      Services.console.registerListener(observer);
      Services.obs.notifyObservers(null, topic);
      do_timeout(1000, function() {
        info("Timeout while waiting for resource: " + resource);
        deferred.reject("timeout");
      });

      let resolved = false;
      try {
        await deferred.promise;
        resolved = true;
      } catch (ex) {
        if (ex == "timeout") {
          resolved = false;
        } else {
          throw ex;
        }
      }
      Services.console.unregisterListener(observer);
      Services.prefs.clearUserPref("toolkit.osfile.log");
      Services.prefs.clearUserPref("toolkit.osfile.log.redirect");
      Services.prefs.clearUserPref("toolkit.osfile.test.shutdown.observer");
      Services.prefs.clearUserPref("toolkit.async_shutdown.testing");

      return resolved;
    })();
  }

  let TEST_DIR = OS.Path.join(await OS.File.getCurrentDirectory(), "..");
  info("Testing for leaks of directory iterator " + TEST_DIR);
  let iterator = new OS.File.DirectoryIterator(TEST_DIR);
  info("At this stage, we leak the directory");
  Assert.ok(await testLeaksOf(TEST_DIR, "test.shutdown.dir.leak"));
  await iterator.close();
  info("At this stage, we don't leak the directory anymore");
  Assert.equal(false, await testLeaksOf(TEST_DIR, "test.shutdown.dir.noleak"));

  let TEST_FILE = OS.Path.join(OS.Constants.Path.profileDir, "test");
  info("Testing for leaks of file descriptor: " + TEST_FILE);
  let openedFile = await OS.File.open(TEST_FILE, { create: true });
  info("At this stage, we leak the file");
  Assert.ok(await testLeaksOf(TEST_FILE, "test.shutdown.file.leak"));
  await openedFile.close();
  info("At this stage, we don't leak the file anymore");
  Assert.equal(
    false,
    await testLeaksOf(TEST_FILE, "test.shutdown.file.leak.2")
  );
});
