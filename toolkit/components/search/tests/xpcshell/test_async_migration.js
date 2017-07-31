/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that legacy metadata from search-metadata.json is correctly
 * transferred to the new metadata storage. */

function run_test() {
  installTestEngine();

  do_get_file("data/metadata.json").copyTo(gProfD, "search-metadata.json");

  run_next_test();
}

add_task(async function test_async_metadata_migration() {
  await asyncInit();
  await promiseAfterCache();

  // Check that the entries are placed as specified correctly
  let metadata = await promiseEngineMetadata();
  do_check_eq(metadata["engine"].order, 1);
  do_check_eq(metadata["engine"].alias, "foo");

  metadata = await promiseGlobalMetadata();
  do_check_eq(metadata["searchDefaultExpir"], 1471013469846);
});
