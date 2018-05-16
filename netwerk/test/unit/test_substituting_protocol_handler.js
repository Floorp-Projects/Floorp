"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function test_case_insensitive_substitutions() {
  let resProto = Services.io.getProtocolHandler("resource")
    .QueryInterface(Ci.nsISubstitutingProtocolHandler);

  let uri = Services.io.newFileURI(do_get_file("data"));

  resProto.setSubstitution("FooBar", uri);
  resProto.setSubstitutionWithFlags("BarBaz", uri, 0);

  equal(resProto.resolveURI(Services.io.newURI("resource://foobar/")),
        uri.spec, "Got correct resolved URI for setSubstitution");

  equal(resProto.resolveURI(Services.io.newURI("resource://foobar/")),
        uri.spec, "Got correct resolved URI for setSubstitutionWithFlags");

  ok(resProto.hasSubstitution("foobar"), "hasSubstitution works with all-lower-case root");
  ok(resProto.hasSubstitution("FooBar"), "hasSubstitution works with mixed-case root");

  equal(resProto.getSubstitution("foobar").spec, uri.spec,
        "getSubstitution works with all-lower-case root");
  equal(resProto.getSubstitution("FooBar").spec, uri.spec,
        "getSubstitution works with mixed-case root");

  resProto.setSubstitution("foobar", null);
  resProto.setSubstitution("barbaz", null);

  Assert.throws(() => resProto.resolveURI(Services.io.newURI("resource://foobar/")),
                e => e.result == Cr.NS_ERROR_NOT_AVAILABLE,
                "Correctly unregistered case-insensitive substitution in setSubstitution");
  Assert.throws(() => resProto.resolveURI(Services.io.newURI("resource://barbaz/")),
                e => e.result == Cr.NS_ERROR_NOT_AVAILABLE,
                "Correctly unregistered case-insensitive substitution in setSubstitutionWithFlags");

  Assert.throws(() => resProto.getSubstitution("foobar"),
                e => e.result == Cr.NS_ERROR_NOT_AVAILABLE,
                "foobar substitution has been removed");
});
