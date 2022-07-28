/* eslint-env mozilla/chrome-script */

const { FormHistory } = ChromeUtils.import(
  "resource://gre/modules/FormHistory.jsm"
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

var gAutocompletePopup = Services.ww.activeWindow.document.getElementById(
  "PopupAutoComplete"
);
assert.ok(gAutocompletePopup, "Got autocomplete popup");

var ParentUtils = {
  getMenuEntries() {
    let entries = [];
    let numRows = gAutocompletePopup.view.matchCount;
    for (let i = 0; i < numRows; i++) {
      entries.push(gAutocompletePopup.view.getValueAt(i));
    }
    return entries;
  },

  cleanUpFormHistory() {
    return FormHistory.update({ op: "remove" });
  },

  updateFormHistory(changes) {
    let handler = {
      handleError(error) {
        assert.ok(false, error);
        sendAsyncMessage("formHistoryUpdated", { ok: false });
      },
      handleCompletion(reason) {
        if (!reason) {
          sendAsyncMessage("formHistoryUpdated", { ok: true });
        }
      },
    };
    FormHistory.update(changes, handler);
  },

  popupshownListener() {
    let results = this.getMenuEntries();
    sendAsyncMessage("onpopupshown", { results });
  },

  countEntries(name, value) {
    let obj = {};
    if (name) {
      obj.fieldname = name;
    }
    if (value) {
      obj.value = value;
    }

    let count = 0;
    let listener = {
      handleResult(result) {
        count = result;
      },
      handleError(error) {
        assert.ok(false, error);
        sendAsyncMessage("entriesCounted", { ok: false });
      },
      handleCompletion(reason) {
        if (!reason) {
          sendAsyncMessage("entriesCounted", { ok: true, count });
        }
      },
    };

    FormHistory.count(obj, listener);
  },

  checkRowCount(expectedCount, expectedFirstValue = null) {
    ContentTaskUtils.waitForCondition(() => {
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
    }, "Waiting for row count change: " + expectedCount + " First value: " + expectedFirstValue).then(
      () => {
        let results = this.getMenuEntries();
        sendAsyncMessage("gotMenuChange", { results });
      }
    );
  },

  checkSelectedIndex(expectedIndex) {
    ContentTaskUtils.waitForCondition(() => {
      return (
        gAutocompletePopup.popupOpen &&
        gAutocompletePopup.selectedIndex === expectedIndex
      );
    }, "Checking selected index").then(() => {
      sendAsyncMessage("gotSelectedIndex");
    });
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

ParentUtils._popupshownListener = ParentUtils.popupshownListener.bind(
  ParentUtils
);
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
  ({ expectedCount, expectedFirstValue }) => {
    ParentUtils.checkRowCount(expectedCount, expectedFirstValue);
  }
);

addMessageListener("waitForSelectedIndex", ({ expectedIndex }) => {
  ParentUtils.checkSelectedIndex(expectedIndex);
});
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
