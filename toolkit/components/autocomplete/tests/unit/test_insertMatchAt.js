function run_test() {
  let result = Cc["@mozilla.org/autocomplete/simple-result;1"]
                 .createInstance(Ci.nsIAutoCompleteSimpleResult);
  result.appendMatch("a", "");
  result.appendMatch("c", "");
  result.insertMatchAt(1, "b", "");
  result.insertMatchAt(3, "d", "");

  Assert.equal(result.matchCount, 4);
  Assert.equal(result.getValueAt(0), "a");
  Assert.equal(result.getValueAt(1), "b");
  Assert.equal(result.getValueAt(2), "c");
  Assert.equal(result.getValueAt(3), "d");
}
