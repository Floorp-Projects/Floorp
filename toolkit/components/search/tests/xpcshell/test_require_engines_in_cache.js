/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function ignore_cache_files_without_engines() {
  let commitPromise = promiseAfterCache();
  let engineCount = (await Services.search.getEngines()).length;
  Assert.equal(engineCount, 2);

  // Wait for the file to be saved to disk, so that we can mess with it.
  await commitPromise;

  // Remove all engines from the cache file.
  let cache = await promiseCacheData();
  cache.engines = [];
  await promiseSaveCacheData(cache);

  // Check that after an async re-initialization, we still have the same engine count.
  commitPromise = promiseAfterCache();
  await asyncReInit();
  Assert.equal(engineCount, (await Services.search.getEngines()).length);
  await commitPromise;

  // Check that after a sync re-initialization, we still have the same engine count.
  await promiseSaveCacheData(cache);
  let unInitPromise = SearchTestUtils.promiseSearchNotification(
    "uninit-complete"
  );
  let reInitPromise = asyncReInit();
  await unInitPromise;
  Assert.ok(!Services.search.isInitialized);
  // Synchronously check the engine count; will force a sync init.
  Assert.equal(engineCount, (await Services.search.getEngines()).length);
  Assert.ok(Services.search.isInitialized);
  await reInitPromise;
});
