/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

// Tests source usage in the form history API.

add_task(async function () {
  // Shorthands to improve test readability.
  const count = FormHistoryTestUtils.count.bind(FormHistoryTestUtils);
  async function search(fieldname, filters) {
    let results1 = (await FormHistoryTestUtils.search(fieldname, filters)).map(
      f => f.value
    );
    // Check autocomplete returns the same value.
    let results2 = (
      await FormHistoryTestUtils.autocomplete("va", fieldname, filters)
    ).map(f => f.text);
    Assert.deepEqual(results1, results2);
    return results1;
  }

  info("Sanity checks");
  Assert.equal(await count("field-A"), 0);
  Assert.equal(await count("field-B"), 0);
  await FormHistoryTestUtils.add("field-A", [{ value: "value-A" }]);
  Assert.equal(await FormHistoryTestUtils.count("field-A"), 1);
  Assert.deepEqual(await search("field-A"), ["value-A"]);
  await FormHistoryTestUtils.remove("field-A", [{ value: "value-A" }]);
  Assert.equal(await count("field-A"), 0);

  info("Test source for field-A");
  await FormHistoryTestUtils.add("field-A", [
    { value: "value-A", source: "test" },
  ]);
  Assert.equal(await count("field-A"), 1);
  Assert.deepEqual(await search("field-A"), ["value-A"]);
  Assert.equal(await count("field-A", { source: "test" }), 1);
  Assert.deepEqual(await search("field-A", { source: "test" }), ["value-A"]);
  Assert.equal(await count("field-A", { source: "test2" }), 0);
  Assert.deepEqual(await search("field-A", { source: "test2" }), []);

  info("Test source for field-B");
  await FormHistoryTestUtils.add("field-B", [
    { value: "value-B", source: "test" },
  ]);
  Assert.equal(await count("field-B", { source: "test" }), 1);
  Assert.equal(await count("field-B", { source: "test2" }), 0);

  info("Remove source");
  await FormHistoryTestUtils.add("field-B", [
    { value: "value-B", source: "test2" },
  ]);
  Assert.equal(await count("field-B", { source: "test2" }), 1);
  Assert.deepEqual(await search("field-B", { source: "test2" }), ["value-B"]);
  await FormHistoryTestUtils.remove("field-B", [{ source: "test2" }]);
  Assert.equal(await count("field-B", { source: "test2" }), 0);
  Assert.deepEqual(await search("field-B", { source: "test2" }), []);
  Assert.equal(await count("field-A"), 1);
  Assert.deepEqual(await search("field-A"), ["value-A"]);
  Assert.deepEqual(await search("field-A", { source: "test" }), ["value-A"]);
  info("The other source should be untouched");
  Assert.equal(await count("field-B", { source: "test" }), 1);
  Assert.deepEqual(await search("field-B", { source: "test" }), ["value-B"]);
  Assert.equal(await count("field-B"), 1);
  Assert.deepEqual(await search("field-B"), ["value-B"]);

  info("Clear field-A");
  await FormHistoryTestUtils.clear("field-A");
  Assert.equal(await count("field-A", { source: "test" }), 0);
  Assert.equal(await count("field-A", { source: "test2" }), 0);
  Assert.equal(await count("field-A"), 0);
  Assert.equal(await count("field-B", { source: "test" }), 1);
  Assert.equal(await count("field-B", { source: "test2" }), 0);
  Assert.equal(await count("field-B"), 1);

  info("Clear All");
  await FormHistoryTestUtils.clear();
  Assert.equal(await count("field-B", { source: "test" }), 0);
  Assert.equal(await count("field-B"), 0);
  Assert.deepEqual(await search("field-A"), []);
  Assert.deepEqual(await search("field-A", { source: "test" }), []);
  Assert.deepEqual(await search("field-B"), []);
  Assert.deepEqual(await search("field-B", { source: "test" }), []);

  info("Check there's no orphan sources");
  let db = await FormHistory.db;
  let rows = await db.execute(`SELECT count(*) FROM moz_sources`);
  Assert.equal(rows[0].getResultByIndex(0), 0, "There should be no orphans");
});
