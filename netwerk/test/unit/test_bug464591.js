
const StandardURL = Components.Constructor("@mozilla.org/network/standard-url;1",
                                           "nsIStandardURL",
                                           "init");

 // 1.percent-encoded IDN that contains blacklisted character should be converted
 //   to punycode, not UTF-8 string
 // 2.only hostname-valid percent encoded ASCII characters should be decoded
 // 3.IDN convertion must not bypassed by %00
let reference = [
   ["www.example.com%e2%88%95www.mozill%d0%b0.com%e2%81%84www.mozilla.org",
    "www.example.xn--comwww-re3c.xn--mozill-8nf.xn--comwww-rq0c.mozilla.org"],
   ["www.mozill%61%2f.org", "www.mozilla%2f.org"], // a slash is not valid in the hostname
   ["www.e%00xample.com%e2%88%95www.mozill%d0%b0.com%e2%81%84www.mozill%61.org",
    "www.e%00xample.xn--comwww-re3c.xn--mozill-8nf.xn--comwww-rq0c.mozilla.org"],
];

let prefData =
  [
    {
      name: "network.enableIDN",
      newVal: true
    },
    {
      name: "network.IDN_show_punycode",
      newVal: false
    },
    {
      name: "network.IDN.whitelist.org",
      newVal: true
    }
  ];

 let prefIdnBlackList = {
      name: "network.IDN.blacklist_chars",
      minimumList: "\u2215\u0430\u2044",
  };

function stringToURL(str) {
  return (new StandardURL(Ci.nsIStandardURL.URLTYPE_AUTHORITY, 80,
			 str, "UTF-8", null))
         .QueryInterface(Ci.nsIURL);
}

function run_test() {
  // Make sure our prefs are set such that this test actually means something
  let prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);
  for (let pref of prefData) {
    prefs.setBoolPref(pref.name, pref.newVal);
  }

  prefIdnBlackList.set = false;
  try {
    prefIdnBlackList.oldVal = prefs.getComplexValue(prefIdnBlackList.name,
                                                    Ci.nsIPrefLocalizedString).data;
    prefs.getComplexValue(prefIdnBlackList.name,
                          Ci.nsIPrefLocalizedString).data=prefIdnBlackList.minimumList;
    prefIdnBlackList.set = true;
  } catch (e) {
  }

  registerCleanupFunction(function() {
    for (let pref of prefData) {
      prefs.clearUserPref(pref.name);
    }
    if (prefIdnBlackList.set) {
      prefs.getComplexValue(prefIdnBlackList.name,
                            Ci.nsIPrefLocalizedString).data = prefIdnBlackList.oldVal;
    }
  });

  for (let i = 0; i < reference.length; ++i) {
    try {
      let result = stringToURL("http://" + reference[i][0]).host;
      equal(result, reference[i][1]);
    } catch (e) {
      ok(false, "Error testing "+reference[i][0]);
    }
  }
}
