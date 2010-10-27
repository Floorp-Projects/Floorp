var setupTest = function() {
  controller = mozmill.getBrowserController();
}

var testExpectedEvent = function() {
  controller.open("http://www.mozilla.com/en-US/");
  controller.waitForPageLoad();

  var search = new elementslib.ID(controller.tabs.activeTab, "query");
  var submit = new elementslib.ID(controller.tabs.activeTab, "submit");

  // Clicking the search field should raise a focus event
  controller.click(search, 2, 2, {type: "focus"});

  // Synthesize a keypress without and with an expected event
  controller.keypress(search, "F", {});
  controller.keypress(search, "i", {}, {type: "keypress"});

  // Synthesize a keypress without and with an expected event
  controller.type(search, "ref");
  controller.type(search, "ox", {type: "keypress"});

  // Using Cmd/Ctrl+A should fire a select event on that element
  controller.keypress(search, "a", {accelKey: true}, {type: "keypress"});

  // A focus event for the next element in the tab order should be fired
  controller.keypress(null, "VK_TAB", {}, {type: "focus", target: submit});

  // Opening the context menu shouldn't raise a click event
  try {
    controller.rightClick(submit, 2, 2, {type: "click", target: submit});
    throw new Error("Opening a context menu has raised a click event.");
  } catch (ex) {
  }

  // ... but a contextmenu event
  controller.rightClick(submit, 2, 2, {type: "contextmenu", target: submit});

  // With no event type specified we should throw an error
  var catched = true;
  try {
    controller.keypress(null, "VK_TAB", {}, {target: submit});
    catched = false;
  } catch (ex) {}

  if (!catched) {
    throw new Error("Missing event type should cause a failure.")
  }
}

