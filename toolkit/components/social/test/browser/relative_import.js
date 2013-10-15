dump("relative_import file\n");

// Please keep 'causeError' on line 4; we test the error location.
function causeError() { does_not_exist(); }

testVar = "oh hai";
function testFunc() {
  return "oh hai";
}
