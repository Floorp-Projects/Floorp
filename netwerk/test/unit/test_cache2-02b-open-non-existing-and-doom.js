"use strict";

add_task(async function test() {
  do_get_profile();
  do_test_pending();

  await new Promise(resolve => {
    // Open non-existing for read, should fail
    asyncOpenCacheEntry(
      "http://b/",
      "disk",
      Ci.nsICacheStorage.OPEN_READONLY,
      null,
      new OpenCallback(NOTFOUND, null, null, function (entry) {
        resolve(entry);
      })
    );
  });

  await new Promise(resolve => {
    // Open the same non-existing for read again, should fail second time
    asyncOpenCacheEntry(
      "http://b/",
      "disk",
      Ci.nsICacheStorage.OPEN_READONLY,
      null,
      new OpenCallback(NOTFOUND, null, null, function (entry) {
        resolve(entry);
      })
    );
  });

  await new Promise(resolve => {
    // Try it again normally, should go
    asyncOpenCacheEntry(
      "http://b/",
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      null,
      new OpenCallback(NEW, "b1m", "b1d", function (entry) {
        resolve(entry);
      })
    );
  });

  await new Promise(resolve => {
    // ...and check
    asyncOpenCacheEntry(
      "http://b/",
      "disk",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      null,
      new OpenCallback(NORMAL, "b1m", "b1d", function (entry) {
        resolve(entry);
      })
    );
  });

  Services.prefs.setBoolPref("network.cache.bug1708673", true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.cache.bug1708673");
  });

  let asyncDoomVisitor = new Promise(resolve => {
    let doomTasks = [];
    let visitor = {
      onCacheStorageInfo() {},
      async onCacheEntryInfo(
        aURI,
        aIdEnhance,
        aDataSize,
        aAltDataSize,
        aFetchCount,
        aLastModifiedTime,
        aExpirationTime,
        aPinned,
        aInfo
      ) {
        doomTasks.push(
          new Promise(resolve1 => {
            Services.cache2
              .diskCacheStorage(aInfo, false)
              .asyncDoomURI(aURI, aIdEnhance, {
                onCacheEntryDoomed() {
                  info("doomed");
                  resolve1();
                },
              });
          })
        );
      },
      onCacheEntryVisitCompleted() {
        Promise.allSettled(doomTasks).then(resolve);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    Services.cache2.asyncVisitAllStorages(visitor, true);
  });

  let asyncOpenVisitor = new Promise(resolve => {
    let openTasks = [];
    let visitor = {
      onCacheStorageInfo() {},
      async onCacheEntryInfo(
        aURI,
        aIdEnhance,
        aDataSize,
        aAltDataSize,
        aFetchCount,
        aLastModifiedTime,
        aExpirationTime,
        aPinned,
        aInfo
      ) {
        info(`found ${aURI.spec}`);
        openTasks.push(
          new Promise(r2 => {
            Services.cache2
              .diskCacheStorage(aInfo, false)
              .asyncOpenURI(
                aURI,
                "",
                Ci.nsICacheStorage.OPEN_READONLY |
                  Ci.nsICacheStorage.OPEN_SECRETLY,
                {
                  onCacheEntryCheck() {
                    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
                  },
                  onCacheEntryAvailable(entry, isnew, status) {
                    info("opened");
                    r2();
                  },
                  QueryInterface: ChromeUtils.generateQI([
                    "nsICacheEntryOpenCallback",
                  ]),
                }
              );
          })
        );
      },
      onCacheEntryVisitCompleted() {
        Promise.all(openTasks).then(resolve);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    Services.cache2.asyncVisitAllStorages(visitor, true);
  });

  await Promise.all([asyncDoomVisitor, asyncOpenVisitor]);

  info("finished visiting");

  await new Promise(resolve => {
    let entryCount = 0;
    let visitor = {
      onCacheStorageInfo() {},
      async onCacheEntryInfo(
        aURI,
        aIdEnhance,
        aDataSize,
        aAltDataSize,
        aFetchCount,
        aLastModifiedTime,
        aExpirationTime,
        aPinned,
        aInfo
      ) {
        entryCount++;
      },
      onCacheEntryVisitCompleted() {
        Assert.equal(entryCount, 0);
        resolve();
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    Services.cache2.asyncVisitAllStorages(visitor, true);
  });

  finish_cache2_test();
});
