// This file tests SQLITE_FCNTL_CHUNK_SIZE behaves as expected

function run_sql(d, sql) {
  var stmt = d.createStatement(sql);
  stmt.execute();
  stmt.finalize();
}

function new_file(name) {
  var file = dirSvc.get("ProfD", Ci.nsIFile);
  file.append(name);
  return file;
}

function get_size(name) {
  return new_file(name).fileSize;
}

function run_test() {
  const filename = "chunked.sqlite";
  const CHUNK_SIZE = 512 * 1024;
  var d = getDatabase(new_file(filename));
  try {
    d.setGrowthIncrement(CHUNK_SIZE, "");
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FILE_TOO_BIG) {
      throw e;
    }
    print("Too little free space to set CHUNK_SIZE!");
    return;
  }
  run_sql(d, "CREATE TABLE bloat(data varchar)");

  var orig_size = get_size(filename);
  /* Dump in at least 32K worth of data.
   * While writing ensure that the file size growth in chunksize set above.
   */
  const str1024 = new Array(1024).join("T");
  for (var i = 0; i < 32; i++) {
    run_sql(d, "INSERT INTO bloat VALUES('" + str1024 + "')");
    var size = get_size(filename);
    // Must not grow in small increments.
    do_check_true(size == orig_size || size >= CHUNK_SIZE);
  }
  /* In addition to growing in chunk-size increments, the db
   * should shrink in chunk-size increments too.
   */
  run_sql(d, "DELETE FROM bloat");
  run_sql(d, "VACUUM");
  do_check_true(get_size(filename) >= CHUNK_SIZE);
}

