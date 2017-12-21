// Testcase for bug 365166 - crash [@ strlen] calling
// mozIStorageStatement::getColumnName of a statement created with
// "PRAGMA user_version" or "PRAGMA schema_version"
function run_test() {
  test("user");
  test("schema");

  function test(param) {
    var colName = param + "_version";
    var sql = "PRAGMA " + colName;

    var file = getTestDB();
    var conn = Services.storage.openDatabase(file);
    var statement = conn.createStatement(sql);
    try {
      // This shouldn't crash:
      Assert.equal(statement.getColumnName(0), colName);
    } finally {
      statement.reset();
      statement.finalize();
    }
  }
}
