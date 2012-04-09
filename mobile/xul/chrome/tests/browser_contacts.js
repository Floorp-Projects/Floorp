
// pull in the Contacts service
let tmp = {};
Components.utils.import("resource:///modules/contacts.jsm", tmp);
let Contacts = tmp.Contacts;

let fac = Cc["@mozilla.org/satchel/form-autocomplete;1"].getService(Ci.nsIFormAutoComplete);
let fh = Cc["@mozilla.org/satchel/form-history;1"].getService(Ci.nsIFormHistory2);

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

// In case there are real contacts that could mess up our test counts
let preEmailCount = fac.autoCompleteSearch("email", "", null, null).matchCount;
let prePhoneCount = fac.autoCompleteSearch("tel", "", null, null).matchCount;

Contacts.addProvider(MockContactsProvider);

let tests = {
  testBasicMatch: function() {
    // Search for any emails
    let results = fac.autoCompleteSearch("email", "", null, null);
    is(results.matchCount, 3 + preEmailCount, "Found 3 emails for un-filtered search");

    // Do some filtered searches
    results = fac.autoCompleteSearch("email", "-Billy", null, null);
    is(results.matchCount, 2, "Found 2 emails '-Billy'");

    results = fac.autoCompleteSearch("tel", "-Billy", null, null);
    is(results.matchCount, 3, "Found 3 phone numbers '-Billy'");

    results = fac.autoCompleteSearch("skip", "-Billy", null, null);
    is(results.matchCount, 0, "Found nothing for a non-contact field");

    results = fac.autoCompleteSearch("phone", "-Jo", null, null);
    is(results.matchCount, 1, "Found 1 phone number '-Jo'");
  },

  testMixingData: function() {
    // Add a simple value to the non-contact system
    fh.addEntry("email", "super.cool@place.com");

    let results = fac.autoCompleteSearch("email", "", null, null);
    is(results.matchCount, 4 + preEmailCount, "Found 4 emails for un-filtered search");

    let firstEmail = results.getValueAt(0);
    is(firstEmail, "super.cool@place.com", "The non-contact entry is first");

    fh.removeAllEntries();
  },

  testFakeInputField: function() {
    let attributes = ["type", "id", "class"];
    for (let i = 0; i < attributes.length; i++) {
      let field = document.createElementNS("http://www.w3.org/1999/xhtml", "html:input");
      field.setAttribute(attributes[i], "tel");

      let results = fac.autoCompleteSearch("", "-Jo", field, null);
      is(results.matchCount, 1 + prePhoneCount, "Found 1 phone number -Jo");
    }
  }
};
