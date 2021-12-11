// nsDoTestsForAutoCompleteWithComposition tests autocomplete with composition.
// Users must include SimpleTest.js and EventUtils.js.

function waitForCondition(condition, nextTest) {
  var tries = 0;
  var interval = setInterval(function() {
    if (condition() || tries >= 30) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() {
    clearInterval(interval);
    nextTest();
  };
}

function nsDoTestsForAutoCompleteWithComposition(
  aDescription,
  aWindow,
  aTarget,
  aAutoCompleteController,
  aIsFunc,
  aGetTargetValueFunc,
  aOnFinishFunc
) {
  this._description = aDescription;
  this._window = aWindow;
  this._target = aTarget;
  this._controller = aAutoCompleteController;

  this._is = aIsFunc;
  this._getTargetValue = aGetTargetValueFunc;
  this._onFinish = aOnFinishFunc;

  this._target.focus();

  this._DefaultCompleteDefaultIndex = this._controller.input.completeDefaultIndex;

  this._doTests();
}

nsDoTestsForAutoCompleteWithComposition.prototype = {
  _window: null,
  _target: null,
  _controller: null,
  _DefaultCompleteDefaultIndex: false,
  _description: "",

  _is: null,
  _getTargetValue() {
    return "not initialized";
  },
  _onFinish: null,

  _doTests() {
    if (++this._testingIndex == this._tests.length) {
      this._controller.input.completeDefaultIndex = this._DefaultCompleteDefaultIndex;
      this._onFinish();
      return;
    }

    var test = this._tests[this._testingIndex];
    if (
      this._controller.input.completeDefaultIndex != test.completeDefaultIndex
    ) {
      this._controller.input.completeDefaultIndex = test.completeDefaultIndex;
    }
    test.execute(this._window);

    if (test.popup) {
      waitForCondition(
        () => this._controller.input.popupOpen,
        this._checkResult.bind(this)
      );
    } else {
      waitForCondition(() => {
        this._controller.searchStatus >=
          Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
      }, this._checkResult.bind(this));
    }
  },

  _checkResult() {
    var test = this._tests[this._testingIndex];
    this._is(
      this._getTargetValue(),
      test.value,
      this._description + ", " + test.description + ": value"
    );
    this._is(
      this._controller.searchString,
      test.searchString,
      this._description + ", " + test.description + ": searchString"
    );
    this._is(
      this._controller.input.popupOpen,
      test.popup,
      this._description + ", " + test.description + ": popupOpen"
    );
    this._doTests();
  },

  _testingIndex: -1,
  _tests: [
    // Simple composition when popup hasn't been shown.
    // The autocomplete popup should not be shown during composition, but
    // after compositionend, the popup should be shown.
    {
      description: "compositionstart shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "M",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "M" },
          },
          aWindow
        );
      },
      popup: false,
      value: "M",
      searchString: "",
    },
    {
      description: "modifying composition string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "Mo",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "o" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "",
    },
    {
      description: "compositionend should open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Enter" } },
          aWindow
        );
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
    },
    // If composition starts when popup is shown, the compositionstart event
    // should cause closing the popup.
    {
      description: "compositionstart should close the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "z",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "z" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Moz",
      searchString: "Mo",
    },
    {
      description: "modifying composition string shouldn't reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "zi",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "i" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozi",
      searchString: "Mo",
    },
    {
      description:
        "compositionend should research the result and open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Enter" } },
          aWindow
        );
      },
      popup: true,
      value: "Mozi",
      searchString: "Mozi",
    },
    // If composition is cancelled, the value shouldn't be changed.
    {
      description: "compositionstart should reclose the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "l",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "l" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozil",
      searchString: "Mozi",
    },
    {
      description: "modifying composition string shouldn't reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "ll",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "l" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozill",
      searchString: "Mozi",
    },
    {
      description:
        "modifying composition string to empty string shouldn't reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: { string: "", clauses: [{ length: 0, attr: 0 }] },
            caret: { start: 0, length: 0 },
            key: { key: "KEY_Backspace" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozi",
      searchString: "Mozi",
    },
    {
      description: "cancled compositionend should reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommit", data: "", key: { key: "KEY_Escape" } },
          aWindow
        );
      },
      popup: true,
      value: "Mozi",
      searchString: "Mozi",
    },
    // But if composition replaces some characters and canceled, the search
    // string should be the latest value.
    {
      description:
        "compositionstart with selected string should close the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("VK_LEFT", { shiftKey: true }, aWindow);
        synthesizeKey("VK_LEFT", { shiftKey: true }, aWindow);
        synthesizeCompositionChange(
          {
            composition: {
              string: "z",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "z" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Moz",
      searchString: "Mozi",
    },
    {
      description: "modifying composition string shouldn't reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "zi",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "i" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozi",
      searchString: "Mozi",
    },
    {
      description:
        "modifying composition string to empty string shouldn't reopen the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: { string: "", clauses: [{ length: 0, attr: 0 }] },
            caret: { start: 0, length: 0 },
            key: { key: "KEY_Backspace" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "Mozi",
    },
    {
      description:
        "canceled compositionend should search the result with the latest value",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Escape" } },
          aWindow
        );
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
    },
    // If all characters are removed, the popup should be closed.
    {
      description:
        "the value becomes empty by backspace, the popup should be closed",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
      },
      popup: false,
      value: "",
      searchString: "",
    },
    // composition which is canceled shouldn't cause opening the popup.
    {
      description: "compositionstart shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "M",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "M" },
          },
          aWindow
        );
      },
      popup: false,
      value: "M",
      searchString: "",
    },
    {
      description: "modifying composition string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "Mo",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "o" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "",
    },
    {
      description:
        "modifying composition string to empty string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: { string: "", clauses: [{ length: 0, attr: 0 }] },
            caret: { start: 0, length: 0 },
            key: { key: "KEY_Backspace" },
          },
          aWindow
        );
      },
      popup: false,
      value: "",
      searchString: "",
    },
    {
      description:
        "canceled compositionend shouldn't open the popup if it was closed",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Escape" } },
          aWindow
        );
      },
      popup: false,
      value: "",
      searchString: "",
    },
    // Down key should open the popup even if the editor is empty.
    {
      description: "DOWN key should open the popup even if the value is empty",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("KEY_ArrowDown", {}, aWindow);
      },
      popup: true,
      value: "",
      searchString: "",
    },
    // If popup is open at starting composition, the popup should be reopened
    // after composition anyway.
    {
      description: "compositionstart shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "M",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "M" },
          },
          aWindow
        );
      },
      popup: false,
      value: "M",
      searchString: "",
    },
    {
      description: "modifying composition string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "Mo",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "o" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "",
    },
    {
      description:
        "modifying composition string to empty string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: { string: "", clauses: [{ length: 0, attr: 0 }] },
            caret: { start: 0, length: 0 },
            key: { key: "KEY_Backspace" },
          },
          aWindow
        );
      },
      popup: false,
      value: "",
      searchString: "",
    },
    {
      description:
        "canceled compositionend should open the popup if it was opened",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Escape" } },
          aWindow
        );
      },
      popup: true,
      value: "",
      searchString: "",
    },
    // Type normally, and hit escape, the popup should be closed.
    {
      description: "ESCAPE should close the popup after typing something",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("M", {}, aWindow);
        synthesizeKey("o", {}, aWindow);
        synthesizeKey("KEY_Escape", {}, aWindow);
      },
      popup: false,
      value: "Mo",
      searchString: "Mo",
    },
    // Even if the popup is closed, composition which is canceled should open
    // the popup if the value isn't empty.
    // XXX This might not be good behavior, but anyway, this is minor issue...
    {
      description: "compositionstart shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "z",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "z" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Moz",
      searchString: "Mo",
    },
    {
      description: "modifying composition string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "zi",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "i" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mozi",
      searchString: "Mo",
    },
    {
      description:
        "modifying composition string to empty string shouldn't open the popup",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: { string: "", clauses: [{ length: 0, attr: 0 }] },
            caret: { start: 0, length: 0 },
            key: { key: "KEY_Backspace" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "Mo",
    },
    {
      description:
        "canceled compositionend shouldn't open the popup if the popup was closed",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Escape" } },
          aWindow
        );
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
    },
    // House keeping...
    {
      description: "house keeping for next tests",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
      },
      popup: false,
      value: "",
      searchString: "",
    },
    // Testing for nsIAutoCompleteInput.completeDefaultIndex being true.
    {
      description:
        "compositionstart shouldn't open the popup (completeDefaultIndex is true)",
      completeDefaultIndex: true,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "M",
              clauses: [{ length: 1, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 1, length: 0 },
            key: { key: "M" },
          },
          aWindow
        );
      },
      popup: false,
      value: "M",
      searchString: "",
    },
    {
      description:
        "modifying composition string shouldn't open the popup (completeDefaultIndex is true)",
      completeDefaultIndex: true,
      execute(aWindow) {
        synthesizeCompositionChange(
          {
            composition: {
              string: "Mo",
              clauses: [{ length: 2, attr: COMPOSITION_ATTR_RAW_CLAUSE }],
            },
            caret: { start: 2, length: 0 },
            key: { key: "o" },
          },
          aWindow
        );
      },
      popup: false,
      value: "Mo",
      searchString: "",
    },
    {
      description:
        "compositionend should open the popup (completeDefaultIndex is true)",
      completeDefaultIndex: true,
      execute(aWindow) {
        synthesizeComposition(
          { type: "compositioncommitasis", key: { key: "KEY_Enter" } },
          aWindow
        );
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mo",
    },
    // House keeping...
    {
      description: "house keeping for next tests",
      completeDefaultIndex: false,
      execute(aWindow) {
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
      },
      popup: false,
      value: "",
      searchString: "",
    },
  ],
};
