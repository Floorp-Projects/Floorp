/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make sure that there are telemetry entries created by sqlite io

function run_sql(d, sql) {
  var stmt = d.createStatement(sql)
  stmt.execute()
  stmt.finalize();
}

function new_file(name)
{
  var file = dirSvc.get("ProfD", Ci.nsIFile);
  file.append(name);
  return file;
}
function run_test()
{
  const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  let read_hgram = Telemetry.getHistogramById("MOZ_SQLITE_OTHER_READ_B");
  let old_sum = read_hgram.snapshot().sum;
  const file = new_file("telemetry.sqlite");
  var d = getDatabase(file);
  run_sql(d, "CREATE TABLE bloat(data varchar)");
  run_sql(d, "DROP TABLE bloat")
  do_check_true(read_hgram.snapshot().sum > old_sum)
}

