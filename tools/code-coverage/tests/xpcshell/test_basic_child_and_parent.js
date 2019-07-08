/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test_code_coverage_func1() {
  return 22;
}

async function run_test() {
  do_load_child_test_harness();
  do_test_pending();

  const codeCoverage = Cc["@mozilla.org/tools/code-coverage;1"].getService(
    Ci.nsICodeCoverage
  );

  const files_orig = getFiles();

  test_code_coverage_func1();

  await codeCoverage.flushCounters();

  const first_flush_files = getFiles();
  const first_flush_records = parseRecords(
    diffFiles(first_flush_files, files_orig)
  );

  Assert.ok(first_flush_records.has("test_basic_child_and_parent.js"));
  Assert.ok(!first_flush_records.has("support.js"));
  let fnRecords = first_flush_records
    .get("test_basic_child_and_parent.js")
    .filter(record => record.type == "FN");
  let fndaRecords = first_flush_records
    .get("test_basic_child_and_parent.js")
    .filter(record => record.type == "FNDA");
  Assert.ok(fnRecords.some(record => record.name == "top-level"));
  Assert.ok(fnRecords.some(record => record.name == "run_test"));
  Assert.ok(
    fnRecords.some(record => record.name == "test_code_coverage_func1")
  );
  Assert.ok(
    fndaRecords.some(record => record.name == "run_test" && record.hits == 1)
  );
  Assert.ok(
    !fndaRecords.some(record => record.name == "run_test" && record.hits != 1)
  );
  Assert.ok(
    fndaRecords.some(
      record => record.name == "test_code_coverage_func1" && record.hits == 1
    )
  );
  Assert.ok(
    !fndaRecords.some(
      record => record.name == "test_code_coverage_func1" && record.hits != 1
    )
  );

  sendCommand("load('support.js');", async function() {
    await codeCoverage.flushCounters();

    const second_flush_files = getFiles();
    const second_flush_records = parseRecords(
      diffFiles(second_flush_files, first_flush_files)
    );

    Assert.ok(second_flush_records.has("test_basic_child_and_parent.js"));
    fnRecords = second_flush_records
      .get("test_basic_child_and_parent.js")
      .filter(record => record.type == "FN");
    fndaRecords = second_flush_records
      .get("test_basic_child_and_parent.js")
      .filter(record => record.type == "FNDA");
    Assert.ok(fnRecords.some(record => record.name == "top-level"));
    Assert.ok(fnRecords.some(record => record.name == "run_test"));
    Assert.ok(
      fnRecords.some(record => record.name == "test_code_coverage_func1")
    );
    Assert.ok(
      fndaRecords.some(
        record => record.name == "test_code_coverage_func1" && record.hits == 0
      )
    );
    Assert.ok(
      !fndaRecords.some(
        record => record.name == "test_code_coverage_func1" && record.hits != 0
      )
    );
    Assert.ok(second_flush_records.has("support.js"));
    fnRecords = second_flush_records
      .get("support.js")
      .filter(record => record.type == "FN");
    fndaRecords = second_flush_records
      .get("support.js")
      .filter(record => record.type == "FNDA");
    Assert.ok(fnRecords.some(record => record.name == "top-level"));
    Assert.ok(
      fnRecords.some(record => record.name == "test_code_coverage_func2")
    );
    Assert.ok(
      fndaRecords.some(record => record.name == "top-level" && record.hits == 1)
    );
    Assert.ok(
      !fndaRecords.some(
        record => record.name == "top-level" && record.hits != 1
      )
    );
    Assert.ok(
      fndaRecords.some(
        record => record.name == "test_code_coverage_func2" && record.hits == 1
      )
    );
    Assert.ok(
      !fndaRecords.some(
        record => record.name == "test_code_coverage_func2" && record.hits != 1
      )
    );

    do_test_finished();
  });
}
