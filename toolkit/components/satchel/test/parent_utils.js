/* eslint-env mozilla/chrome-script */

const { FormHistory } = ChromeUtils.importESModule(
  "resource://gre/modules/FormHistory.sys.mjs"
);
const { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

var gAutocompletePopup =
  Services.ww.activeWindow.document.getElementById("PopupAutoComplete");
assert.ok(gAutocompletePopup, "Got autocomplete popup");

var ParentUtils = {
  // Returns a object with two fields:
  //  labels - an array of the labels of the current dropdown
  //  comments - an array of the comments of the current dropdown
  getMenuEntries() {
    let labels = [],
      comments = [];
    let numRows = gAutocompletePopup.view.matchCount;
    for (let i = 0; i < numRows; i++) {
      labels.push(gAutocompletePopup.view.getLabelAt(i));
      comments.push(gAutocompletePopup.view.getCommentAt(i));
    }
    return { labels, comments };
  },

  cleanUpFormHistory() {
    return FormHistory.update({ op: "remove" });
  },

  updateFormHistory(changes) {
    FormHistory.update(changes).then(
      () => {
        sendAsyncMessage("formHistoryUpdated", { ok: true });
      },
      error => {
        sendAsyncMessage("formHistoryUpdated", { ok: false });
        assert.ok(false, error);
      }
    );
  },

  popupshownListener() {
    let entries = this.getMenuEntries();
    sendAsyncMessage("onpopupshown", entries);
  },

  countEntries(name, value) {
    let obj = {};
    if (name) {
      obj.fieldname = name;
    }
    if (value) {
      obj.value = value;
    }

    FormHistory.count(obj).then(
      count => {
        sendAsyncMessage("entriesCounted", { ok: true, count });
      },
      error => {
        assert.ok(false, error);
        sendAsyncMessage("entriesCounted", { ok: false });
      }
    );
  },

  async checkRowCount(expectedCount, expectedFirstValue = null) {
    await ContentTaskUtils.waitForCondition(() => {
      // This may be called before gAutocompletePopup has initialised
      // which causes it to throw
      try {
        return (
          gAutocompletePopup.view.matchCount === expectedCount &&
          (!expectedFirstValue ||
            expectedCount <= 1 ||
            gAutocompletePopup.view.getValueAt(0) === expectedFirstValue)
        );
      } catch (e) {
        return false;
      }
    }, `Waiting for row count change to ${expectedCount}, first value: ${expectedFirstValue}.`);

    return this.getMenuEntries();
  },

  async checkSelectedIndex(expectedIndex) {
    await ContentTaskUtils.waitForCondition(
      () =>
        gAutocompletePopup.popupOpen &&
        gAutocompletePopup.selectedIndex === expectedIndex,
      "Checking selected index"
    );
  },

  // Tests using this function need to flip pref for exceptional use of
  // `new Function` / `eval()`.
  // See test_autofill_and_ordinal_forms.html for example.
  testMenuEntry(index, statement) {
    ContentTaskUtils.waitForCondition(() => {
      let el = gAutocompletePopup.richlistbox.getItemAtIndex(index);
      let testFunc = new Services.ww.activeWindow.Function(
        "el",
        `return ${statement}`
      );
      return gAutocompletePopup.popupOpen && el && testFunc(el);
    }, "Testing menu entry").then(() => {
      sendAsyncMessage("menuEntryTested");
    });
  },

  getPopupState() {
    function reply() {
      sendAsyncMessage("gotPopupState", {
        open: gAutocompletePopup.popupOpen,
        selectedIndex: gAutocompletePopup.selectedIndex,
        direction: gAutocompletePopup.style.direction,
      });
    }
    // If the popup state is stable, we can reply immediately.  However, if
    // it's showing or hiding, we should wait its finish and then, send the
    // reply.
    if (
      gAutocompletePopup.state == "open" ||
      gAutocompletePopup.state == "closed"
    ) {
      reply();
      return;
    }
    const stablerState =
      gAutocompletePopup.state == "showing" ? "open" : "closed";
    TestUtils.waitForCondition(
      () => gAutocompletePopup.state == stablerState,
      `Waiting for autocomplete popup getting "${stablerState}" state`
    ).then(reply);
  },

  observe(_subject, topic, data) {
    // This function can be called after SimpleTest.finish().
    // Do not write assertions here, they will lead to intermittent failures.
    sendAsyncMessage("satchel-storage-changed", { subject: null, topic, data });
  },

  async cleanup() {
    gAutocompletePopup.removeEventListener(
      "popupshown",
      this._popupshownListener
    );
    await this.cleanUpFormHistory();
  },
};

ParentUtils._popupshownListener =
  ParentUtils.popupshownListener.bind(ParentUtils);
gAutocompletePopup.addEventListener(
  "popupshown",
  ParentUtils._popupshownListener
);
ParentUtils.cleanUpFormHistory();

addMessageListener("updateFormHistory", msg => {
  ParentUtils.updateFormHistory(msg.changes);
});

addMessageListener("countEntries", ({ name, value }) => {
  ParentUtils.countEntries(name, value);
});

addMessageListener(
  "waitForMenuChange",
  ({ expectedCount, expectedFirstValue }) =>
    ParentUtils.checkRowCount(expectedCount, expectedFirstValue)
);

addMessageListener("waitForSelectedIndex", ({ expectedIndex }) =>
  ParentUtils.checkSelectedIndex(expectedIndex)
);
addMessageListener("waitForMenuEntryTest", ({ index, statement }) => {
  ParentUtils.testMenuEntry(index, statement);
});

addMessageListener("getPopupState", () => {
  ParentUtils.getPopupState();
});

addMessageListener("addObserver", () => {
  Services.obs.addObserver(ParentUtils, "satchel-storage-changed");
});
addMessageListener("removeObserver", () => {
  Services.obs.removeObserver(ParentUtils, "satchel-storage-changed");
});

addMessageListener("cleanup", async () => {
  await ParentUtils.cleanup();
});
