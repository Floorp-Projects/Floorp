/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test_code_coverage_func1() {
  return 22;
}

function test_code_coverage_func2() {
  return 22;
}

async function run_test() {
  do_test_pending();

  Assert.ok("@mozilla.org/tools/code-coverage;1" in Cc);

  const codeCoverageCc = Cc["@mozilla.org/tools/code-coverage;1"];
  Assert.ok(!!codeCoverageCc);

  const codeCoverage = codeCoverageCc.getService(Ci.nsICodeCoverage);
  Assert.ok(!!codeCoverage);

  const files_orig = getFiles();

  test_code_coverage_func1();

  // Flush counters for the first time, we should see this function executed, but test_code_coverage_func not executed.
  await codeCoverage.flushCounters();

  const first_flush_files = getFiles();
  const first_flush_records = parseRecords(
    diffFiles(first_flush_files, files_orig)
  );

  Assert.ok(first_flush_records.has("test_basic.js"));
  let fnRecords = first_flush_records
    .get("test_basic.js")
    .filter(record => record.type == "FN");
  let fndaRecords = first_flush_records
    .get("test_basic.js")
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
  Assert.ok(
    !fndaRecords.some(record => record.name == "test_code_coverage_func2")
  );

  test_code_coverage_func2();

  // Flush counters for the second time, we should see this function not executed, but test_code_coverage_func executed.
  await codeCoverage.flushCounters();

  const second_flush_files = getFiles();
  const second_flush_records = parseRecords(
    diffFiles(second_flush_files, first_flush_files)
  );

  Assert.ok(second_flush_records.has("test_basic.js"));
  fnRecords = second_flush_records
    .get("test_basic.js")
    .filter(record => record.type == "FN");
  fndaRecords = second_flush_records
    .get("test_basic.js")
    .filter(record => record.type == "FNDA");
  Assert.ok(fnRecords.some(record => record.name == "top-level"));
  Assert.ok(fnRecords.some(record => record.name == "run_test"));
  Assert.ok(
    fnRecords.some(record => record.name == "test_code_coverage_func1")
  );
  Assert.ok(
    fnRecords.some(record => record.name == "test_code_coverage_func2")
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
}
