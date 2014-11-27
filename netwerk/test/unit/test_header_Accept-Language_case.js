var testpath = "/bug1054739";

function run_test() {
  let intlPrefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService).getBranch("intl.");

  let oldAcceptLangPref = intlPrefs.getCharPref("accept_languages");

  let testData = [
    ["de, en-US, en",   "de,en-US;q=0.7,en;q=0.3"],
    ["de,en-us,en",     "de,en-US;q=0.7,en;q=0.3"],
    ["en-US, en",       "en-US,en;q=0.5"],
    ["EN-US;q=0.2, EN", "en-US,en;q=0.5"],
  ];

  for (let i = 0; i < testData.length; i++) {
    let acceptLangPref = testData[i][0];
    let expectedHeader = testData[i][1];

    intlPrefs.setCharPref("accept_languages", acceptLangPref);
    let acceptLangHeader = setupChannel(testpath).getRequestHeader("Accept-Language");
    equal(acceptLangHeader, expectedHeader);
  }

  intlPrefs.setCharPref("accept_languages", oldAcceptLangPref);
}

function setupChannel(path) {
  let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  let chan = ios.newChannel("http://localhost:4444" + path, "", null);
  chan.QueryInterface(Ci.nsIHttpChannel);
  return chan;
}
