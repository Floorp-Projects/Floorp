/* import-globals-from partitionedstorage_head.js */

PartitionedStorageHelper.runTestInNormalAndPrivateMode(
  "HTTP Cookies",
  async (win3rdParty, win1stParty, allowed) => {
    await win3rdParty.fetch("cookies.sjs?3rd").then(r => r.text());
    await win3rdParty
      .fetch("cookies.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie:foopy=3rd", "3rd party cookie set");
      });

    await win1stParty.fetch("cookies.sjs?first").then(r => r.text());
    await win1stParty
      .fetch("cookies.sjs")
      .then(r => r.text())
      .then(text => {
        is(text, "cookie:foopy=first", "First party cookie set");
      });

    await win3rdParty
      .fetch("cookies.sjs")
      .then(r => r.text())
      .then(text => {
        if (allowed) {
          is(
            text,
            "cookie:foopy=first",
            "3rd party has the first party cookie set"
          );
        } else {
          is(
            text,
            "cookie:foopy=3rd",
            "3rd party has not the first party cookie set"
          );
        }
      });
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

PartitionedStorageHelper.runTestInNormalAndPrivateMode(
  "DOM Cookies",
  async (win3rdParty, win1stParty, allowed) => {
    win3rdParty.document.cookie = "foo=3rd";
    is(win3rdParty.document.cookie, "foo=3rd", "3rd party cookie set");

    win1stParty.document.cookie = "foo=first";
    is(win1stParty.document.cookie, "foo=first", "First party cookie set");

    if (allowed) {
      is(
        win3rdParty.document.cookie,
        "foo=first",
        "3rd party has the first party cookie set"
      );
    } else {
      is(
        win3rdParty.document.cookie,
        "foo=3rd",
        "3rd party has not the first party cookie set"
      );
    }
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

PartitionedStorageHelper.runPartitioningTestInNormalAndPrivateMode(
  "Partitioned tabs - DOM Cookies",
  "cookies",

  // getDataCallback
  async win => {
    return win.document.cookie;
  },

  // addDataCallback
  async (win, value) => {
    win.document.cookie = value;
    return true;
  },

  // cleanup
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);

PartitionedStorageHelper.runPartitioningTestInNormalAndPrivateMode(
  "Partitioned tabs - Network Cookies",
  "cookies",

  // getDataCallback
  async win => {
    return win
      .fetch("cookies.sjs")
      .then(r => r.text())
      .then(text => {
        return text.substring("cookie:foopy=".length);
      });
  },

  // addDataCallback
  async (win, value) => {
    await win.fetch("cookies.sjs?" + value).then(r => r.text());
    return true;
  },

  // cleanup
  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
        resolve()
      );
    });
  }
);
