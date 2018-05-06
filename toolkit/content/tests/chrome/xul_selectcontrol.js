// This script is used to test elements that implement
// nsIDOMXULSelectControlElement. This currently is the following elements:
//   listbox, menulist, radiogroup, richlistbox, tabs
//
// flag behaviours that differ for certain elements
//   allow-other-value - alternate values for the value property may be used
//                       besides those in the list
//   other-value-clears-selection - alternative values for the value property
//                                  clears the selected item
//   selection-required - an item must be selected in the list, unless there
//                        aren't any to select
//   activate-disabled-menuitem - disabled menuitems can be highlighted
//   select-keynav-wraps - key navigation over a selectable list wraps
//   select-extended-keynav - home, end, page up and page down keys work to
//                            navigate over a selectable list
//   keynav-leftright - key navigation is left/right rather than up/down
// The win:, mac: and gtk: or other prefixes may be used for platform specific behaviour
var behaviours = {
  menu: "win:activate-disabled-menuitem activate-disabled-menuitem-mousemove select-keynav-wraps select-extended-keynav",
  menulist: "allow-other-value other-value-clears-selection",
  listbox: "select-extended-keynav",
  richlistbox: "select-extended-keynav",
  radiogroup: "select-keynav-wraps dont-select-disabled allow-other-value",
  tabs: "select-extended-keynav mac:select-keynav-wraps allow-other-value selection-required keynav-leftright"
};

function behaviourContains(tag, behaviour) {
  var platform = "none:";
  if (navigator.platform.includes("Mac"))
    platform = "mac:";
  else if (navigator.platform.includes("Win"))
    platform = "win:";
  else if (navigator.platform.includes("X"))
    platform = "gtk:";

  var re = new RegExp("\\s" + platform + behaviour + "\\s|\\s" + behaviour + "\\s");
  return re.test(" " + behaviours[tag] + " ");
}

function test_nsIDOMXULSelectControlElement(element, childtag, testprefix) {
  var testid = (testprefix) ? testprefix + " " : "";
  testid += element.localName + " nsIDOMXULSelectControlElement ";

  // editable menulists use the label as the value instead
  var firstvalue = "first", secondvalue = "second", fourthvalue = "fourth";
  if (element.localName == "menulist" && element.editable) {
    firstvalue = "First Item";
    secondvalue = "Second Item";
    fourthvalue = "Fourth Item";
  }

  // 'initial' - check if the initial state of the element is correct
  test_nsIDOMXULSelectControlElement_States(element, testid + "initial", 0, null, -1, "");

  test_nsIDOMXULSelectControlElement_init(element, testid);

  // 'appendItem' - check if appendItem works to add a new item
  var firstitem = element.appendItem("First Item", "first");
  is(firstitem.localName, childtag,
                testid + "appendItem - first item is " + childtag);
  test_nsIDOMXULSelectControlElement_States(element, testid + "appendItem", 1, null, -1, "");

  is(firstitem.control, element, testid + "control");

  // 'selectedIndex' - check if an item may be selected
  element.selectedIndex = 0;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedIndex", 1, firstitem, 0, firstvalue);

  // 'appendItem 2' - check if a second item may be added
  var seconditem = element.appendItem("Second Item", "second");
  test_nsIDOMXULSelectControlElement_States(element, testid + "appendItem 2", 2, firstitem, 0, firstvalue);

  // 'selectedItem' - check if the second item may be selected
  element.selectedItem = seconditem;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedItem", 2, seconditem, 1, secondvalue);

  // 'selectedIndex 2' - check if selectedIndex may be set to -1 to deselect items
  var selectionRequired = behaviourContains(element.localName, "selection-required");
  element.selectedIndex = -1;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedIndex 2", 2,
        selectionRequired ? seconditem : null, selectionRequired ? 1 : -1,
        selectionRequired ? secondvalue : "");

  // 'selectedItem 2' - check if the selectedItem property may be set to null
  element.selectedIndex = 1;
  element.selectedItem = null;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedItem 2", 2,
        selectionRequired ? seconditem : null, selectionRequired ? 1 : -1,
        selectionRequired ? secondvalue : "");

  // 'getIndexOfItem' - check if getIndexOfItem returns the right index
  is(element.getIndexOfItem(firstitem), 0, testid + "getIndexOfItem - first item at index 0");
  is(element.getIndexOfItem(seconditem), 1, testid + "getIndexOfItem - second item at index 1");

  var otheritem = element.ownerDocument.createElement(childtag);
  is(element.getIndexOfItem(otheritem), -1, testid + "getIndexOfItem - other item not found");

  // 'getItemAtIndex' - check if getItemAtIndex returns the right item
  is(element.getItemAtIndex(0), firstitem, testid + "getItemAtIndex - index 0 is first item");
  is(element.getItemAtIndex(1), seconditem, testid + "getItemAtIndex - index 0 is second item");
  is(element.getItemAtIndex(-1), null, testid + "getItemAtIndex - index -1 is null");
  is(element.getItemAtIndex(2), null, testid + "getItemAtIndex - index 2 is null");

  // check if setting the value changes the selection
  element.value = firstvalue;
  test_nsIDOMXULSelectControlElement_States(element, testid + "set value 1", 2, firstitem, 0, firstvalue);
  element.value = secondvalue;
  test_nsIDOMXULSelectControlElement_States(element, testid + "set value 2", 2, seconditem, 1, secondvalue);
  // setting the value attribute to one not in the list doesn't change the selection.
  // The value is only changed for elements which support having a value other than the
  // selection.
  element.value = "other";
  var allowOtherValue = behaviourContains(element.localName, "allow-other-value");
  var otherValueClearsSelection = behaviourContains(element.localName, "other-value-clears-selection");
  test_nsIDOMXULSelectControlElement_States(element, testid + "set value other", 2,
                                            otherValueClearsSelection ? null : seconditem,
                                            otherValueClearsSelection ? -1 : 1,
                                            allowOtherValue ? "other" : secondvalue);
  if (allowOtherValue)
    element.value = "";

  var fourthitem = element.appendItem("Fourth Item", fourthvalue);
  element.selectedIndex = 0;
  fourthitem.disabled = true;
  element.selectedIndex = 2;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedIndex disabled", 3, fourthitem, 2, fourthvalue);

  element.selectedIndex = 0;
  element.selectedItem = fourthitem;
  test_nsIDOMXULSelectControlElement_States(element, testid + "selectedItem disabled", 3, fourthitem, 2, fourthvalue);

  if (element.menupopup) {
    element.menupopup.textContent = "";
  } else {
    element.textContent = "";
  }
}

function test_nsIDOMXULSelectControlElement_init(element, testprefix) {
  // editable menulists use the label as the value
  var isEditable = (element.localName == "menulist" && element.editable);

  var id = element.id;
  element = document.getElementById(id + "-initwithvalue");
  if (element) {
    var seconditem = element.getItemAtIndex(1);
    test_nsIDOMXULSelectControlElement_States(element, testprefix + " value initialization",
                                              3, seconditem, 1,
                                              isEditable ? seconditem.label : seconditem.value);
  }

  element = document.getElementById(id + "-initwithselected");
  if (element) {
    var thirditem = element.getItemAtIndex(2);
    test_nsIDOMXULSelectControlElement_States(element, testprefix + " selected initialization",
                                              3, thirditem, 2,
                                              isEditable ? thirditem.label : thirditem.value);
  }
}

function test_nsIDOMXULSelectControlElement_States(element, testid,
                                                   expectedcount, expecteditem,
                                                   expectedindex, expectedvalue) {
  // need an itemCount property here
  var count = element.itemCount;
  is(count, expectedcount, testid + " item count");
  is(element.selectedItem, expecteditem, testid + " selectedItem");
  is(element.selectedIndex, expectedindex, testid + " selectedIndex");
  is(element.value, expectedvalue, testid + " value");
  if (element.selectedItem) {
    is(element.selectedItem.selected, true,
                  testid + " selectedItem marked as selected");
  }
}

/** test_nsIDOMXULSelectControlElement_UI
  *
  * Test the UI aspects of an element which implements nsIDOMXULSelectControlElement
  *
  * Parameters:
  *   element - element to test
  */
function test_nsIDOMXULSelectControlElement_UI(element, testprefix) {
  var testid = (testprefix) ? testprefix + " " : "";
  testid += element.localName + " nsIDOMXULSelectControlElement UI ";

  if (element.menupopup) {
    element.menupopup.textContent = "";
  } else {
    element.textContent = "";
  }

  var firstitem = element.appendItem("First Item", "first");
  var seconditem = element.appendItem("Second Item", "second");

  // 'mouse select' - check if clicking an item selects it
  synthesizeMouseExpectEvent(firstitem, 2, 2, {}, element, "select", testid + "mouse select");
  test_nsIDOMXULSelectControlElement_States(element, testid + "mouse select", 2, firstitem, 0, "first");

  synthesizeMouseExpectEvent(seconditem, 2, 2, {}, element, "select", testid + "mouse select 2");
  test_nsIDOMXULSelectControlElement_States(element, testid + "mouse select 2", 2, seconditem, 1, "second");

  // make sure the element is focused so keyboard navigation will apply
  element.selectedIndex = 1;
  element.focus();

  var navLeftRight = behaviourContains(element.localName, "keynav-leftright");
  var backKey = navLeftRight ? "VK_LEFT" : "VK_UP";
  var forwardKey = navLeftRight ? "VK_RIGHT" : "VK_DOWN";

  // 'key select' - check if keypresses move between items
  synthesizeKeyExpectEvent(backKey, {}, element, "select", testid + "key up");
  test_nsIDOMXULSelectControlElement_States(element, testid + "key up", 2, firstitem, 0, "first");

  var keyWrap = behaviourContains(element.localName, "select-keynav-wraps");

  var expectedItem = keyWrap ? seconditem : firstitem;
  var expectedIndex = keyWrap ? 1 : 0;
  var expectedValue = keyWrap ? "second" : "first";
  synthesizeKeyExpectEvent(backKey, {}, keyWrap ? element : null, "select", testid + "key up 2");
  test_nsIDOMXULSelectControlElement_States(element, testid + "key up 2", 2,
    expectedItem, expectedIndex, expectedValue);

  element.selectedIndex = 0;
  synthesizeKeyExpectEvent(forwardKey, {}, element, "select", testid + "key down");
  test_nsIDOMXULSelectControlElement_States(element, testid + "key down", 2, seconditem, 1, "second");

  expectedItem = keyWrap ? firstitem : seconditem;
  expectedIndex = keyWrap ? 0 : 1;
  expectedValue = keyWrap ? "first" : "second";
  synthesizeKeyExpectEvent(forwardKey, {}, keyWrap ? element : null, "select", testid + "key down 2");
  test_nsIDOMXULSelectControlElement_States(element, testid + "key down 2", 2,
    expectedItem, expectedIndex, expectedValue);

  var thirditem = element.appendItem("Third Item", "third");
  var fourthitem = element.appendItem("Fourth Item", "fourth");
  if (behaviourContains(element.localName, "select-extended-keynav")) {
    element.appendItem("Fifth Item", "fifth");
    var sixthitem = element.appendItem("Sixth Item", "sixth");

    synthesizeKeyExpectEvent("VK_END", {}, element, "select", testid + "key end");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key end", 6, sixthitem, 5, "sixth");

    synthesizeKeyExpectEvent("VK_HOME", {}, element, "select", testid + "key home");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key home", 6, firstitem, 0, "first");

    synthesizeKeyExpectEvent("VK_PAGE_DOWN", {}, element, "select", testid + "key page down");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key page down", 6, fourthitem, 3, "fourth");
    synthesizeKeyExpectEvent("VK_PAGE_DOWN", {}, element, "select", testid + "key page down to end");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key page down to end", 6, sixthitem, 5, "sixth");

    synthesizeKeyExpectEvent("VK_PAGE_UP", {}, element, "select", testid + "key page up");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key page up", 6, thirditem, 2, "third");
    synthesizeKeyExpectEvent("VK_PAGE_UP", {}, element, "select", testid + "key page up to start");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key page up to start", 6, firstitem, 0, "first");

    element.getItemAtIndex(5).remove();
    element.getItemAtIndex(4).remove();
  }

  // now test whether a disabled item works.
  element.selectedIndex = 0;
  seconditem.disabled = true;

  var dontSelectDisabled = (behaviourContains(element.localName, "dont-select-disabled"));

  // 'mouse select' - check if clicking an item selects it
  synthesizeMouseExpectEvent(seconditem, 2, 2, {}, element,
                             dontSelectDisabled ? "!select" : "select",
                             testid + "mouse select disabled");
  test_nsIDOMXULSelectControlElement_States(element, testid + "mouse select disabled", 4,
    dontSelectDisabled ? firstitem : seconditem, dontSelectDisabled ? 0 : 1,
    dontSelectDisabled ? "first" : "second");

  if (dontSelectDisabled) {
    // test whether disabling an item won't allow it to be selected
    synthesizeKeyExpectEvent(forwardKey, {}, element, "select", testid + "key down disabled");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key down disabled", 4, thirditem, 2, "third");

    synthesizeKeyExpectEvent(backKey, {}, element, "select", testid + "key up disabled");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key up disabled", 4, firstitem, 0, "first");

    element.selectedIndex = 2;
    firstitem.disabled = true;

    synthesizeKeyExpectEvent(backKey, {}, keyWrap ? element : null, "select", testid + "key up disabled 2");
    expectedItem = keyWrap ? fourthitem : thirditem;
    expectedIndex = keyWrap ? 3 : 2;
    expectedValue = keyWrap ? "fourth" : "third";
    test_nsIDOMXULSelectControlElement_States(element, testid + "key up disabled 2", 4,
      expectedItem, expectedIndex, expectedValue);
  } else {
    // in this case, disabled items should behave the same as non-disabled items.
    element.selectedIndex = 0;
    synthesizeKeyExpectEvent(forwardKey, {}, element, "select", testid + "key down disabled");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key down disabled", 4, seconditem, 1, "second");
    synthesizeKeyExpectEvent(forwardKey, {}, element, "select", testid + "key down disabled again");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key down disabled again", 4, thirditem, 2, "third");

    synthesizeKeyExpectEvent(backKey, {}, element, "select", testid + "key up disabled");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key up disabled", 4, seconditem, 1, "second");
    synthesizeKeyExpectEvent(backKey, {}, element, "select", testid + "key up disabled again");
    test_nsIDOMXULSelectControlElement_States(element, testid + "key up disabled again", 4, firstitem, 0, "first");
  }
}
