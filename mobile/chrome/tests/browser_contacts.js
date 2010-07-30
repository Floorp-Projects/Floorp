
// pull in the Contacts service
Components.utils.import("resource:///modules/contacts.jsm");

function test() {
  ok(Contacts, "Contacts class exists");
  for (var fname in tests) {
    tests[fname]();
  }
}

let MockContactsProvider = {
  getContacts: function() {
    let contacts = [
      {
        fullName: "-Billy One",
        emails: [],
        phoneNumbers: ["999-888-7777"]
      },
      {
        fullName: "-Billy Two",
        emails: ["billy.two@fake.com", "btwo@work.com"],
        phoneNumbers: ["111-222-3333", "123-123-1234"]
      },
      {
        fullName: "-Joe Schmo",
        emails: ["joeschmo@foo.com"],
        phoneNumbers: ["555-555-5555"]
      }
    ];

    return contacts;
  }
};

Contacts.addProvider(MockContactsProvider);

let fac = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);

let tests = {
  testBasicMatch: function() {
    let results = fac.autoCompleteSearch("email", "-Billy", null, null);
    ok(results.matchCount == 2, "Found 2 emails '-Billy'");

    results = fac.autoCompleteSearch("tel", "-Billy", null, null);
    ok(results.matchCount == 3, "Found 3 phone numbers '-Billy'");

    results = fac.autoCompleteSearch("skip", "-Billy", null, null);
    ok(results.matchCount == 0, "Found nothing for a non-contact field");

    results = fac.autoCompleteSearch("phone", "-Jo", null, null);
    ok(results.matchCount == 1, "Found 1 phone number '-Jo'");
  }
};
