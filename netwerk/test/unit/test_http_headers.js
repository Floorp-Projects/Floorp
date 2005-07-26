function check_header(chan, name, value) {
  do_check_eq(chan.getRequestHeader(name), value);
}

function run_test() {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel("http://www.mozilla.org/", null, null)
                .QueryInterface(Components.interfaces.nsIHttpChannel);

  check_header(chan, "host", "www.mozilla.org");
  check_header(chan, "Host", "www.mozilla.org");

  chan.setRequestHeader("foopy", "bar", false);
  check_header(chan, "foopy", "bar");

  chan.setRequestHeader("foopy", "baz", true);
  check_header(chan, "foopy", "bar, baz");

  for (var i = 0; i < 100; ++i)
    chan.setRequestHeader("foopy" + i, i, false);

  for (var i = 0; i < 100; ++i)
    check_header(chan, "foopy" + i, i);

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
}
