"use strict";

function stringToDefaultURI(str) {
  return Cc["@mozilla.org/network/default-uri-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec(str)
    .finalize();
}

add_task(function test_getters() {
  let uri = stringToDefaultURI(
    "proto://user:password@hostname:123/path/to/file?query#hash"
  );
  equal(uri.spec, "proto://user:password@hostname:123/path/to/file?query#hash");
  equal(uri.prePath, "proto://user:password@hostname:123");
  equal(uri.scheme, "proto");
  equal(uri.userPass, "user:password");
  equal(uri.username, "user");
  equal(uri.password, "password");
  equal(uri.hasUserPass, true);
  equal(uri.hostPort, "hostname:123");
  equal(uri.host, "hostname");
  equal(uri.port, 123);
  equal(uri.pathQueryRef, "/path/to/file?query#hash");
  equal(uri.asciiSpec, uri.spec);
  equal(uri.asciiHostPort, uri.hostPort);
  equal(uri.asciiHost, uri.host);
  equal(uri.ref, "hash");
  equal(
    uri.specIgnoringRef,
    "proto://user:password@hostname:123/path/to/file?query"
  );
  equal(uri.hasRef, true);
  equal(uri.filePath, "/path/to/file");
  equal(uri.query, "query");
  equal(uri.displayHost, uri.host);
  equal(uri.displayHostPort, uri.hostPort);
  equal(uri.displaySpec, uri.spec);
  equal(uri.displayPrePath, uri.prePath);
});

add_task(function test_methods() {
  let uri = stringToDefaultURI(
    "proto://user:password@hostname:123/path/to/file?query#hash"
  );
  let uri_same = stringToDefaultURI(
    "proto://user:password@hostname:123/path/to/file?query#hash"
  );
  let uri_different = stringToDefaultURI(
    "proto://user:password@hostname:123/path/to/file?query"
  );
  let uri_very_different = stringToDefaultURI(
    "proto://user:password@hostname:123/path/to/file?query1#hash"
  );
  ok(uri.equals(uri_same));
  ok(!uri.equals(uri_different));
  ok(uri.schemeIs("proto"));
  ok(!uri.schemeIs("proto2"));
  ok(!uri.schemeIs("proto "));
  ok(!uri.schemeIs("proto\n"));
  equal(uri.resolve("/hello"), "proto://user:password@hostname:123/hello");
  equal(
    uri.resolve("hello"),
    "proto://user:password@hostname:123/path/to/hello"
  );
  equal(uri.resolve("proto2:otherhost"), "proto2:otherhost");
  ok(uri.equalsExceptRef(uri_same));
  ok(uri.equalsExceptRef(uri_different));
  ok(!uri.equalsExceptRef(uri_very_different));
});

add_task(function test_mutator() {
  let uri = stringToDefaultURI(
    "proto://user:pass@host:123/path/to/file?query#hash"
  );

  let check = (callSetters, verify) => {
    let m = uri.mutate();
    callSetters(m);
    verify(m.finalize());
  };

  check(
    m => m.setSpec("test:bla"),
    u => equal(u.spec, "test:bla")
  );
  check(
    m => m.setSpec("test:bla"),
    u => equal(u.spec, "test:bla")
  );
  check(
    m => m.setScheme("some"),
    u => equal(u.spec, "some://user:pass@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setUserPass("u"),
    u => equal(u.spec, "proto://u@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setUserPass("u:p"),
    u => equal(u.spec, "proto://u:p@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setUserPass(":p"),
    u => equal(u.spec, "proto://:p@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setUserPass(""),
    u => equal(u.spec, "proto://host:123/path/to/file?query#hash")
  );
  check(
    m => m.setUsername("u"),
    u => equal(u.spec, "proto://u:pass@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setPassword("p"),
    u => equal(u.spec, "proto://user:p@host:123/path/to/file?query#hash")
  );
  check(
    m => m.setHostPort("h"),
    u => equal(u.spec, "proto://user:pass@h:123/path/to/file?query#hash")
  );
  check(
    m => m.setHostPort("h:456"),
    u => equal(u.spec, "proto://user:pass@h:456/path/to/file?query#hash")
  );
  check(
    m => m.setHost("bla"),
    u => equal(u.spec, "proto://user:pass@bla:123/path/to/file?query#hash")
  );
  check(
    m => m.setPort(987),
    u => equal(u.spec, "proto://user:pass@host:987/path/to/file?query#hash")
  );
  check(
    m => m.setPathQueryRef("/p?q#r"),
    u => equal(u.spec, "proto://user:pass@host:123/p?q#r")
  );
  check(
    m => m.setRef("r"),
    u => equal(u.spec, "proto://user:pass@host:123/path/to/file?query#r")
  );
  check(
    m => m.setFilePath("/my/path"),
    u => equal(u.spec, "proto://user:pass@host:123/my/path?query#hash")
  );
  check(
    m => m.setQuery("q"),
    u => equal(u.spec, "proto://user:pass@host:123/path/to/file?q#hash")
  );
});

add_task(function test_ipv6() {
  let uri = stringToDefaultURI("non-special://[2001::1]/");
  equal(uri.hostPort, "[2001::1]");
  // Hopefully this will change after bug 1603199.
  equal(uri.host, "2001::1");
});

add_task(function test_serialization() {
  let uri = stringToDefaultURI("http://example.org/path");
  let str = serialize_to_escaped_string(uri);
  let other = deserialize_from_escaped_string(str).QueryInterface(Ci.nsIURI);
  equal(other.spec, uri.spec);
});

// This test assumes the serialization never changes, which might not be true.
// It's OK to change the test if we ever make changes to the serialization
// code and this starts failing.
add_task(function test_deserialize_from_string() {
  let payload =
    "%04DZ%A0%FD%27L%99%BDAk%E61%8A%E9%2C%00%00%00%00%00%00%00" +
    "%00%C0%00%00%00%00%00%00F%00%00%00%13scheme%3Astuff/to/say";
  equal(
    deserialize_from_escaped_string(payload).QueryInterface(Ci.nsIURI).spec,
    stringToDefaultURI("scheme:stuff/to/say").spec
  );

  let payload2 =
    "%04DZ%A0%FD%27L%99%BDAk%E61%8A%E9%2C%00%00%00%00%00%00%00" +
    "%00%C0%00%00%00%00%00%00F%00%00%00%17http%3A//example.org/path";
  equal(
    deserialize_from_escaped_string(payload2).QueryInterface(Ci.nsIURI).spec,
    stringToDefaultURI("http://example.org/path").spec
  );
});
