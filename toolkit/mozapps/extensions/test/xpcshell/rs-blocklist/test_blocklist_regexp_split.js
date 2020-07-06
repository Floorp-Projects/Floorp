/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// useMLBF=true only supports blocking by version+ID, not by regexp.
Services.prefs.setBoolPref("extensions.blocklist.useMLBF", false);

const BLOCKLIST_DATA = [
  {
    guid: "/^abcd.*/",
    versionRange: [],
    expectedType: "RegExp",
  },
  {
    guid: "test@example.com",
    versionRange: [],
    expectedType: "string",
  },
  {
    guid: "/^((a)|(b)|(c))$/",
    versionRange: [],
    expectedType: "Set",
  },
  {
    guid: "/^((a@b)|(\\{6d9ddd6e-c6ee-46de-ab56-ce9080372b3\\})|(c@d.com))$/",
    versionRange: [],
    expectedType: "Set",
  },
  // The same as the above, but with escape sequences that disqualify it from
  // being treated as a set (and a different guid)
  {
    guid:
      "/^((s@t)|(\\{6d9eee6e-c6ee-46de-ab56-ce9080372b3\\})|(c@d\\w.com))$/",
    versionRange: [],
    expectedType: "RegExp",
  },
  // Also the same, but with other magical regex characters.
  // (and a different guid)
  {
    guid:
      "/^((u@v)|(\\{6d9fff6e*-c6ee-46de-ab56-ce9080372b3\\})|(c@dee?.com))$/",
    versionRange: [],
    expectedType: "RegExp",
  },
];

/**
 * Verify that both IDs being OR'd in a regex work,
 * and that other regular expressions continue being
 * used as regular expressions.
 */
add_task(async function test_check_matching_works() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  await promiseStartupManager();
  await AddonTestUtils.loadBlocklistRawData({
    extensions: BLOCKLIST_DATA,
  });

  let blocklistGlobal = ChromeUtils.import(
    "resource://gre/modules/Blocklist.jsm",
    null
  );
  let parsedEntries = blocklistGlobal.ExtensionBlocklistRS._entries;

  // Unfortunately, the parsed entries aren't in the same order as the original.
  function strForTypeOf(val) {
    if (typeof val == "string") {
      return "string";
    }
    if (val) {
      return val.constructor.name;
    }
    return "other";
  }
  for (let type of ["Set", "RegExp", "string"]) {
    let numberParsed = parsedEntries.filter(parsedEntry => {
      return type == strForTypeOf(parsedEntry.matches.id);
    }).length;
    let expected = BLOCKLIST_DATA.filter(entry => {
      return type == entry.expectedType;
    }).length;
    Assert.equal(
      numberParsed,
      expected,
      type + " should have expected number of entries"
    );
  }
  // Shouldn't block everything.
  Assert.ok(
    !(await Blocklist.getAddonBlocklistEntry({ id: "nonsense", version: "1" }))
  );
  // Should block IDs starting with abcd
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "abcde", version: "1" })
  );
  // Should block the literal string listed
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "test@example.com",
      version: "1",
    })
  );
  // Should block the IDs in (a)|(b)|(c)
  Assert.ok(await Blocklist.getAddonBlocklistEntry({ id: "a", version: "1" }));
  Assert.ok(await Blocklist.getAddonBlocklistEntry({ id: "b", version: "1" }));
  Assert.ok(await Blocklist.getAddonBlocklistEntry({ id: "c", version: "1" }));
  // Should block all the items processed to a set:
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "a@b", version: "1" })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "{6d9ddd6e-c6ee-46de-ab56-ce9080372b3}",
      version: "1",
    })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "c@d.com", version: "1" })
  );
  // Should block items that remained a regex:
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "s@t", version: "1" })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "{6d9eee6e-c6ee-46de-ab56-ce9080372b3}",
      version: "1",
    })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "c@dx.com", version: "1" })
  );

  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "u@v", version: "1" })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "{6d9fff6eeeeeeee-c6ee-46de-ab56-ce9080372b3}",
      version: "1",
    })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({ id: "c@dee.com", version: "1" })
  );
});

// We should be checking all properties, not just the first one we come across.
add_task(async function check_all_properties() {
  await AddonTestUtils.loadBlocklistRawData({
    extensions: [
      {
        guid: "literal@string.com",
        creator: "Foo",
        versionRange: [],
      },
      {
        guid: "/regex.*@regex\\.com/",
        creator: "Foo",
        versionRange: [],
      },
      {
        guid:
          "/^((set@set\\.com)|(anotherset@set\\.com)|(reallyenoughsetsalready@set\\.com))$/",
        creator: "Foo",
        versionRange: [],
      },
    ],
  });

  let { Blocklist } = ChromeUtils.import(
    "resource://gre/modules/Blocklist.jsm"
  );
  // Check 'wrong' creator doesn't match.
  Assert.ok(
    !(await Blocklist.getAddonBlocklistEntry({
      id: "literal@string.com",
      version: "1",
      creator: { name: "Bar" },
    }))
  );
  Assert.ok(
    !(await Blocklist.getAddonBlocklistEntry({
      id: "regexaaaaa@regex.com",
      version: "1",
      creator: { name: "Bar" },
    }))
  );
  Assert.ok(
    !(await Blocklist.getAddonBlocklistEntry({
      id: "set@set.com",
      version: "1",
      creator: { name: "Bar" },
    }))
  );

  // Check 'wrong' ID doesn't match.
  Assert.ok(
    !(await Blocklist.getAddonBlocklistEntry({
      id: "someotherid@foo.com",
      version: "1",
      creator: { name: "Foo" },
    }))
  );

  // Check items matching all filters do match
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "literal@string.com",
      version: "1",
      creator: { name: "Foo" },
    })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "regexaaaaa@regex.com",
      version: "1",
      creator: { name: "Foo" },
    })
  );
  Assert.ok(
    await Blocklist.getAddonBlocklistEntry({
      id: "set@set.com",
      version: "1",
      creator: { name: "Foo" },
    })
  );
});
