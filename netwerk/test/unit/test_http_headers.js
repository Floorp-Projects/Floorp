
function check_request_header(chan, name, value) {
  var chanValue;
  try {
    chanValue = chan.getRequestHeader(name);
  } catch (e) {
    do_throw("Expected to find header '" + name + "' but didn't find it");
  }
  Assert.equal(chanValue, value);
}

function run_test() {

  var chan = NetUtil.newChannel ({
    uri: "http://www.mozilla.org/",
    loadUsingSystemPrincipal: true
  }).QueryInterface(Ci.nsIHttpChannel);

  check_request_header(chan, "host", "www.mozilla.org");
  check_request_header(chan, "Host", "www.mozilla.org");

  chan.setRequestHeader("foopy", "bar", false);
  check_request_header(chan, "foopy", "bar");

  chan.setRequestHeader("foopy", "baz", true);
  check_request_header(chan, "foopy", "bar, baz");

  for (var i = 0; i < 100; ++i)
    chan.setRequestHeader("foopy" + i, i, false);

  for (var i = 0; i < 100; ++i)
    check_request_header(chan, "foopy" + i, i);

  var x = false;
  try {
    chan.setRequestHeader("foo:py", "baz", false);
  } catch (e) {
    x = true;
  }
  if (!x)
    do_throw("header with colon not rejected");

  x = false;
  try {
    chan.setRequestHeader("foopy", "b\naz", false);
  } catch (e) {
    x = true;
  }
  if (!x)
    do_throw("header value with newline not rejected");

  x = false;
  try {
    chan.setRequestHeader("foopy\u0080", "baz", false);
  } catch (e) {
    x = true;
  }
  if (!x)
    do_throw("header name with non-ASCII not rejected");

  x = false;
  try {
    chan.setRequestHeader("foopy", "b\u0000az", false);
  } catch (e) {
    x = true;
  }
  if (!x)
    do_throw("header value with null-byte not rejected");
}
