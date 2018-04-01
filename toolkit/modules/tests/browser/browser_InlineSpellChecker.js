var InlineSpellChecker;
var SpellCheckHelper;

function test() {
  let tempScope = {};
  ChromeUtils.import("resource://gre/modules/InlineSpellChecker.jsm", tempScope);
  InlineSpellChecker = tempScope.InlineSpellChecker;
  SpellCheckHelper = tempScope.SpellCheckHelper;

  ok(InlineSpellChecker, "InlineSpellChecker class exists");
  for (var fname in tests) {
    tests[fname]();
  }
}

var tests = {
  testFlagsForInputs() {
    const HTML_NS = "http://www.w3.org/1999/xhtml";
    const {
      INPUT, EDITABLE, TEXTINPUT, NUMERIC, PASSWORD, SPELLCHECKABLE,
    } = SpellCheckHelper;
    const kExpectedResults = {
      "text": INPUT | EDITABLE | TEXTINPUT | SPELLCHECKABLE,
      "password": INPUT | EDITABLE | TEXTINPUT | PASSWORD,
      "search": INPUT | EDITABLE | TEXTINPUT | SPELLCHECKABLE,
      "url": INPUT | EDITABLE | TEXTINPUT,
      "tel": INPUT | EDITABLE | TEXTINPUT,
      "email": INPUT | EDITABLE | TEXTINPUT,
      "number": INPUT | EDITABLE | TEXTINPUT | NUMERIC,
      "checkbox": INPUT,
      "radio": INPUT,
    };

    for (let [type, expectedFlags] of Object.entries(kExpectedResults)) {
      let input = document.createElementNS(HTML_NS, "input");
      input.type = type;
      let actualFlags = SpellCheckHelper.isEditable(input, window);
      is(actualFlags, expectedFlags,
         `For input type "${type}" expected flags ${"0x" + expectedFlags.toString(16)}; ` +
         `got ${"0x" + actualFlags.toString(16)}`);
    }
  },
};
