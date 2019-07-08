function run_test() {
  let result = Cc["@mozilla.org/autocomplete/simple-result;1"].createInstance(
    Ci.nsIAutoCompleteSimpleResult
  );
  result.appendMatch("a", "");
  result.appendMatch("b", "");
  result.removeMatchAt(0);
  Assert.equal(result.matchCount, 1);
  Assert.equal(result.getValueAt(0), "b");
  result.appendMatch("c", "");
  result.removeMatchAt(1);
  Assert.equal(result.matchCount, 1);
  Assert.equal(result.getValueAt(0), "b");
  result.removeMatchAt(0);
  Assert.equal(result.matchCount, 0);
}
