// nsDoTestsForEditorWithAutoComplete tests basic functions of editor with autocomplete.
// Users must include SimpleTest.js and EventUtils.js, and register "Mozilla" to the autocomplete for the target.

async function waitForCondition(condition) {
  return new Promise(resolve => {
    var tries = 0;
    var interval = setInterval(function() {
      if (condition() || tries >= 60) {
        moveOn();
      }
      tries++;
    }, 100);
    var moveOn = function() {
      clearInterval(interval);
      resolve();
    };
  });
}

function nsDoTestsForEditorWithAutoComplete(
  aDescription,
  aWindow,
  aTarget,
  aAutoCompleteController,
  aIsFunc,
  aTodoIsFunc,
  aGetTargetValueFunc
) {
  this._description = aDescription;
  this._window = aWindow;
  this._target = aTarget;
  this._controller = aAutoCompleteController;

  this._is = aIsFunc;
  this._todo_is = aTodoIsFunc;
  this._getTargetValue = aGetTargetValueFunc;

  this._target.focus();

  this._DefaultCompleteDefaultIndex = this._controller.input.completeDefaultIndex;
}

nsDoTestsForEditorWithAutoComplete.prototype = {
  _window: null,
  _target: null,
  _controller: null,
  _DefaultCompleteDefaultIndex: false,
  _description: "",

  _is: null,
  _getTargetValue() {
    return "not initialized";
  },

  run: async function runTestsImpl() {
    for (let test of this._tests) {
      if (
        this._controller.input.completeDefaultIndex != test.completeDefaultIndex
      ) {
        this._controller.input.completeDefaultIndex = test.completeDefaultIndex;
      }

      let beforeInputEvents = [];
      let inputEvents = [];
      function onBeforeInput(aEvent) {
        beforeInputEvents.push(aEvent);
      }
      function onInput(aEvent) {
        inputEvents.push(aEvent);
      }
      this._target.addEventListener("beforeinput", onBeforeInput);
      this._target.addEventListener("input", onInput);

      if (test.execute(this._window, this._target) === false) {
        this._target.removeEventListener("beforeinput", onBeforeInput);
        this._target.removeEventListener("input", onInput);
        continue;
      }

      await waitForCondition(() => {
        return (
          this._controller.searchStatus >=
          Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH
        );
      });
      this._target.removeEventListener("beforeinput", onBeforeInput);
      this._target.removeEventListener("input", onInput);
      this._checkResult(test, beforeInputEvents, inputEvents);
    }
    this._controller.input.completeDefaultIndex = this._DefaultCompleteDefaultIndex;
  },

  _checkResult(aTest, aBeforeInputEvents, aInputEvents) {
    this._is(
      this._getTargetValue(),
      aTest.value,
      this._description + ", " + aTest.description + ": value"
    );
    this._is(
      this._controller.searchString,
      aTest.searchString,
      this._description + ", " + aTest.description + ": searchString"
    );
    this._is(
      this._controller.input.popupOpen,
      aTest.popup,
      this._description + ", " + aTest.description + ": popupOpen"
    );
    this._is(
      this._controller.searchStatus,
      Ci.nsIAutoCompleteController.STATUS_COMPLETE_MATCH,
      this._description + ", " + aTest.description + ": status"
    );
    this._is(
      aBeforeInputEvents.length,
      aTest.inputEvents.length,
      this._description +
        ", " +
        aTest.description +
        ": number of beforeinput events wrong"
    );
    this._is(
      aInputEvents.length,
      aTest.inputEvents.length,
      this._description +
        ", " +
        aTest.description +
        ": number of input events wrong"
    );
    for (let events of [aBeforeInputEvents, aInputEvents]) {
      for (let i = 0; i < events.length; i++) {
        if (aTest.inputEvents[i] === undefined) {
          this._is(
            true,
            false,
            this._description +
              ", " +
              aTest.description +
              ': "beforeinput" and "input" event shouldn\'t be dispatched anymore'
          );
          return;
        }
        this._is(
          events[i] instanceof this._window.InputEvent,
          true,
          `${this._description}, ${aTest.description}: "${events[i].type}" event should be dispatched with InputEvent interface`
        );
        this._is(
          events[i].cancelable,
          events[i].type === "beforeinput",
          `${this._description}, ${aTest.description}: "${
            events[i].type
          }" event should ${
            events[i].type === "beforeinput" ? "be" : "be never"
          } cancelable`
        );
        this._is(
          events[i].bubbles,
          true,
          `${this._description}, ${aTest.description}: "${events[i].type}" event should always bubble`
        );
        this._is(
          events[i].inputType,
          aTest.inputEvents[i].inputType,
          `${this._description}, ${aTest.description}: inputType of "${events[i].type}" event should be "${aTest.inputEvents[i].inputType}"`
        );
        this._is(
          events[i].data,
          aTest.inputEvents[i].data,
          `${this._description}, ${aTest.description}: data of "${events[i].type}" event should be ${aTest.inputEvents[i].data}`
        );
        this._is(
          events[i].dataTransfer,
          null,
          `${this._description}, ${aTest.description}: dataTransfer of "${events[i].type}" event should be null`
        );
        this._is(
          events[i].getTargetRanges().length,
          0,
          `${this._description}, ${aTest.description}: getTargetRanges() of "${events[i].type}" event should return empty array`
        );
      }
    }
  },

  _tests: [
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: type 'Mo'",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("M", { shiftKey: true }, aWindow);
        synthesizeKey("o", {}, aWindow);
        return true;
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
      inputEvents: [
        { inputType: "insertText", data: "M" },
        { inputType: "insertText", data: "o" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: select 'Mozilla' to complete the word",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("KEY_ArrowDown", {}, aWindow);
        synthesizeKey("KEY_Enter", {}, aWindow);
        return true;
      },
      popup: false,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [{ inputType: "insertReplacementText", data: "Mozilla" }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: undo the word, but typed text shouldn't be canceled",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: undo the typed text",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: redo the typed text",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
      inputEvents: [{ inputType: "historyRedo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: redo the word",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [{ inputType: "historyRedo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case: removing all text for next test...",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("a", { accelKey: true }, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "deleteContentBackward", data: null }],
    },

    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: type 'mo'",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("m", {}, aWindow);
        synthesizeKey("o", {}, aWindow);
        return true;
      },
      popup: true,
      value: "mo",
      searchString: "mo",
      inputEvents: [
        { inputType: "insertText", data: "m" },
        { inputType: "insertText", data: "o" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: select 'Mozilla' to complete the word",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("KEY_ArrowDown", {}, aWindow);
        synthesizeKey("KEY_Enter", {}, aWindow);
        return true;
      },
      popup: false,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [{ inputType: "insertReplacementText", data: "Mozilla" }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: undo the word, but typed text shouldn't be canceled",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mo",
      searchString: "mo",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: undo the typed text",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: redo the typed text",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mo",
      searchString: "mo",
      inputEvents: [{ inputType: "historyRedo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: redo the word",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [{ inputType: "historyRedo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case: removing all text for next test...",
      completeDefaultIndex: false,
      execute(aWindow, aTarget) {
        synthesizeKey("a", { accelKey: true }, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "deleteContentBackward", data: null }],
    },

    // Testing for nsIAutoCompleteInput.completeDefaultIndex being true.
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): type 'Mo'",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("M", { shiftKey: true }, aWindow);
        synthesizeKey("o", {}, aWindow);
        return true;
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mo",
      inputEvents: [
        { inputType: "insertText", data: "M" },
        { inputType: "insertText", data: "o" },
        { inputType: "insertReplacementText", data: "Mozilla" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): select 'Mozilla' to complete the word",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("KEY_ArrowDown", {}, aWindow);
        synthesizeKey("KEY_Enter", {}, aWindow);
        return true;
      },
      popup: false,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): undo the word, but typed text shouldn't be canceled",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mo",
      searchString: "Mo",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): undo the typed text",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): redo the typed text",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mo",
      inputEvents: [
        { inputType: "historyRedo", data: null },
        { inputType: "insertReplacementText", data: "Mozilla" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): redo the word",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "Mozilla",
      searchString: "Mo",
      inputEvents: [],
    },
    {
      description:
        "Undo/Redo behavior check when typed text exactly matches the case (completeDefaultIndex is true): removing all text for next test...",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("a", { accelKey: true }, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "deleteContentBackward", data: null }],
    },

    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): type 'mo'",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("m", {}, aWindow);
        synthesizeKey("o", {}, aWindow);
        return true;
      },
      popup: true,
      value: "mozilla",
      searchString: "mo",
      inputEvents: [
        { inputType: "insertText", data: "m" },
        { inputType: "insertText", data: "o" },
        { inputType: "insertReplacementText", data: "mozilla" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): select 'Mozilla' to complete the word",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("KEY_ArrowDown", {}, aWindow);
        synthesizeKey("KEY_Enter", {}, aWindow);
        return true;
      },
      popup: false,
      value: "Mozilla",
      searchString: "Mozilla",
      inputEvents: [{ inputType: "insertReplacementText", data: "Mozilla" }],
    },
    // Different from "exactly matches the case" case, modifying the case causes one additional transaction.
    // Although we could make this transaction ignored.
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): undo the selected word, but typed text shouldn't be canceled",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mozilla",
      searchString: "mozilla",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): undo the word, but typed text shouldn't be canceled",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mo",
      searchString: "mo",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): undo the typed text",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("z", { accelKey: true }, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "historyUndo", data: null }],
    },
    // XXX This is odd case.  Consistency with undo behavior, this should restore "mo".
    //     However, looks like that autocomplete automatically restores "mozilla".
    //     Additionally, looks like that it causes clearing the redo stack.
    //     Therefore, the following redo operations do nothing.
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): redo the typed text",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mozilla",
      searchString: "mo",
      inputEvents: [
        { inputType: "historyRedo", data: null },
        { inputType: "insertReplacementText", data: "mozilla" },
      ],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): redo the default index word",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mozilla",
      searchString: "mo",
      inputEvents: [],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): redo the word",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("Z", { accelKey: true, shiftKey: true }, aWindow);
        return true;
      },
      popup: true,
      value: "mozilla",
      searchString: "mo",
      inputEvents: [],
    },
    {
      description:
        "Undo/Redo behavior check when typed text does not match the case (completeDefaultIndex is true): removing all text for next test...",
      completeDefaultIndex: true,
      execute(aWindow, aTarget) {
        synthesizeKey("a", { accelKey: true }, aWindow);
        synthesizeKey("KEY_Backspace", {}, aWindow);
        return true;
      },
      popup: false,
      value: "",
      searchString: "",
      inputEvents: [{ inputType: "deleteContentBackward", data: null }],
    },
  ],
};
