"use strict";

// Test that the test harness is sane.

// Test that the temporary directory is actually overridden in the
// directory service.
add_task(async function test_TmpD_override() {
  equal(
    FileUtils.getDir("TmpD", []).path,
    AddonTestUtils.tempDir.path,
    "Should get the correct temporary directory from the directory service"
  );
});
