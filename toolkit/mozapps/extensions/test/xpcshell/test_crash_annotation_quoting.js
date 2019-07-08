/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that strange characters in an add-on version don't break the
// crash annotation.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

add_task(async function run_test() {
  await promiseStartupManager();

  let n = 1;
  for (let version in ["1,0", "1:0"]) {
    let id = `addon${n++}@tests.mozilla.org`;
    await promiseInstallWebExtension({
      manifest: {
        version,
        applications: { gecko: { id } },
      },
    });

    do_check_in_crash_annotation(id, version);
  }
});
