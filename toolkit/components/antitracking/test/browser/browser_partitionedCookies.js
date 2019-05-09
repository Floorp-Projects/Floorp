/* import-globals-from storageprincipal_head.js */

StoragePrincipalHelper.runTest("HTTP Cookies",
  async (win3rdParty, win1stParty, allowed) => {
    await win3rdParty.fetch("cookies.sjs?3rd").then(r => r.text());
    await win3rdParty.fetch("cookies.sjs").then(r => r.text()).then(text => {
      is(text, "cookie:foopy=3rd", "3rd party cookie set");
    });

    await win1stParty.fetch("cookies.sjs?first").then(r => r.text());
    await win1stParty.fetch("cookies.sjs").then(r => r.text()).then(text => {
      is(text, "cookie:foopy=first", "First party cookie set");
    });

    await win3rdParty.fetch("cookies.sjs").then(r => r.text()).then(text => {
      if (allowed) {
        is(text, "cookie:foopy=first", "3rd party has the first party cookie set");
      } else {
        is(text, "cookie:foopy=3rd", "3rd party has not the first party cookie set");
      }
    });
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });

StoragePrincipalHelper.runTest("DOM Cookies",
  async (win3rdParty, win1stParty, allowed) => {
    win3rdParty.document.cookie = "foo=3rd";
    is(win3rdParty.document.cookie, "foo=3rd", "3rd party cookie set");

    win1stParty.document.cookie = "foo=first";
    is(win1stParty.document.cookie, "foo=first", "First party cookie set");

    if (allowed) {
      is(win3rdParty.document.cookie, "foo=first", "3rd party has the first party cookie set");
    } else {
      is(win3rdParty.document.cookie, "foo=3rd", "3rd party has not the first party cookie set");
    }
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
    });
  });
