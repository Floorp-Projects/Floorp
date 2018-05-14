/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the origin-related SQL functions, which are:
// * get_host_and_port
// * get_prefix
// * strip_prefix_and_userinfo

add_task(async function test() {
  let sets = [
    ["http:"],
    ["", "//"],
    ["", "user@", "user:@", "user:pass@", "user:pass:word@"],
    ["example.com"],
    ["", ":8888"],
    ["", "/", "/foo"],
    ["", "?", "?bar"],
    ["", "#", "#baz"],
  ];
  let db = await PlacesUtils.promiseDBConnection();
  for (let parts of permute(sets)) {
    let spec = parts.join("");
    let funcs = {
      "get_prefix": parts.slice(0, 2).join(""),
      "get_host_and_port": parts.slice(3, 5).join(""),
      "strip_prefix_and_userinfo": parts.slice(3).join(""),
    };
    for (let [func, expectedValue] of Object.entries(funcs)) {
      let rows = await db.execute(`
        SELECT ${func}("${spec}");
      `);
      let value = rows[0].getString(0);
      Assert.equal(value, expectedValue, `function=${func} spec="${spec}"`);
    }
  }
});

function permute(sets = []) {
  if (!sets.length) {
    return [[]];
  }
  let firstSet = sets[0];
  let otherSets = sets.slice(1);
  let permutedSequences = [];
  let otherPermutedSequences = permute(otherSets);
  for (let other of otherPermutedSequences) {
    for (let value of firstSet) {
      permutedSequences.push([value].concat(other));
    }
  }
  return permutedSequences;
}
