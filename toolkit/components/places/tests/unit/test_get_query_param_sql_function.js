add_task(async function test_get_query_param_sql_function() {
  let db = await PlacesUtils.promiseDBConnection();
  await Assert.rejects(
    db.execute(`SELECT get_query_param()`),
    /wrong number of arguments/
  );
  let rows = await db.execute(`SELECT
    get_query_param('a=b&c=d', 'a'),
    get_query_param('a=b&c=d', 'c'),
    get_query_param('a=b&a=c', 'a'),
    get_query_param('a=b&c=d', 'e'),
    get_query_param('a', 'a'),
    get_query_param(NULL, NULL),
    get_query_param('a=b&c=d', NULL),
    get_query_param(NULL, 'a')`);
  let results = ["b", "d", "b", null, "", null, null, null];
  equal(rows[0].numEntries, results.length);
  for (let i = 0; i < results.length; ++i) {
    equal(rows[0].getResultByIndex(i), results[i]);
  }
});
