// Regression test for bug 278262 - JAR URIs should resolve relative URIs in the base section.

const path = "data/test_bug333423.zip";

function test_relative_sub() {
  var ios = Services.io;
  var spec = "jar:" + ios.newFileURI(do_get_file(path)).spec + "!/";
  var base = ios.newURI(spec);
  var uri = ios.newURI("../modules/libjar", null, base);

  // This is the URI we expect to see.
  var expected =
    "jar:" + ios.newFileURI(do_get_file(path)).spec + "!/modules/libjar";

  Assert.equal(uri.spec, expected);
}

function test_relative_base() {
  var ios = Services.io;
  var base = ios.newFileURI(do_get_file("data/empty"));
  var uri = ios.newURI("jar:../" + path + "!/", null, base);

  // This is the URI we expect to see.
  var expected = "jar:" + ios.newFileURI(do_get_file(path)).spec + "!/";

  Assert.equal(uri.spec, expected);
}

function run_test() {
  test_relative_sub();
  test_relative_base();
}
