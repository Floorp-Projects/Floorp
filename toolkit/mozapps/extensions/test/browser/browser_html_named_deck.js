/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint max-len: ["error", 80] */

"use strict";

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.htmlaboutaddons.enabled", true]],
  });
});

const DEFAULT_SECTION_NAMES = ["one", "two", "three"];

function makeButton({doc, name, deckId}) {
  let button = doc.createElement("named-deck-button");
  button.setAttribute("name", name);
  button.deckId = deckId;
  button.textContent = name.toUpperCase();
  return button;
}

function makeSection({doc, name}) {
  let view = doc.createElement("section");
  view.setAttribute("name", name);
  view.textContent = name + name;
  return view;
}

function addSection({name, deck, buttons}) {
  let doc = deck.ownerDocument;
  let button = makeButton({doc, name, deckId: deck.id});
  buttons.appendChild(button);
  let view = makeSection({doc, name});
  deck.appendChild(view);
  return {button, view};
}

async function runTests({deck, buttons}) {
  const selectedSlot = deck.shadowRoot.querySelector('slot[name="selected"]');
  const getButtonByName = name => buttons.querySelector(`[name="${name}"]`);

  function checkState(name, count, empty = false) {
    // Check that the right view is selected.
    is(deck.selectedViewName, name, "The right view is selected");

    // Verify there's one element in the slot.
    let slottedEls = selectedSlot.assignedElements();
    if (empty) {
      is(slottedEls.length, 0, "The deck is empty");
    } else {
      is(slottedEls.length, 1, "There's one visible view");
      is(slottedEls[0].getAttribute("name"), name,
        "The correct view is in the slot");
    }

    // Check that the hidden properties are set.
    let sections = deck.querySelectorAll("section");
    is(sections.length, count, "There are the right number of sections");
    for (let section of sections) {
      let sectionName = section.getAttribute("name");
      if (sectionName == name) {
        is(section.slot, "selected", `${sectionName} is visible`);
      } else {
        is(section.slot, "", `${sectionName} is hidden`);
      }
    }

    // Check the right button is selected.
    is(buttons.children.length, count, "There are the right number of buttons");
    for (let button of buttons.children) {
      let buttonName = button.getAttribute("name");
      let selected = buttonName == name;
      is(button.hasAttribute("selected"), selected,
         `${buttonName} is ${selected ? "selected" : "not selected"}`);
    }
  }

  // Check that the first view is selected by default.
  checkState("one", 3);

  // Switch to the third view.
  info("Switch to section three");
  getButtonByName("three").click();
  checkState("three", 3);

  // Add a new section, nothing changes.
  info("Add section last");
  let last = addSection({name: "last", deck, buttons});
  checkState("three", 4);

  // We can switch to the new section.
  last.button.click();
  info("Switch to section last");
  checkState("last", 4);

  info("Switch view with selectedViewName");
  let shown = BrowserTestUtils.waitForEvent(deck, "view-changed");
  deck.selectedViewName = "two";
  await shown;
  checkState("two", 4);

  info("Switch back to the last view to test removing selected view");
  shown = BrowserTestUtils.waitForEvent(deck, "view-changed");
  deck.setAttribute("selected-view", "last");
  await shown;
  checkState("last", 4);

  // Removing the selected section leaves the selected slot empty.
  info("Remove section last");
  last.button.remove();
  last.view.remove();

  info("Should not have any selected views");
  checkState("last", 3, true);

  // Setting a missing view will give a "view-changed" event.
  info("Set view to a missing name");
  let hidden = BrowserTestUtils.waitForEvent(deck, "view-changed");
  deck.selectedViewName = "missing";
  await hidden;
  checkState("missing", 3, true);

  // Adding the view won't trigger "view-changed", but the view will slotted.
  info("Add the missing view, it should be shown");
  shown = BrowserTestUtils.waitForEvent(selectedSlot, "slotchange");
  let viewChangedEvent = false;
  let viewChangedFn = () => { viewChangedEvent = true; };
  deck.addEventListener("view-changed", viewChangedFn);
  addSection({name: "missing", deck, buttons});
  await shown;
  deck.removeEventListener("view-changed", viewChangedFn);
  ok(!viewChangedEvent, "The view-changed event didn't fire");
  checkState("missing", 4);
}

async function setup({doc, beAsync, first}) {
  const deckId = `${first}-first-${beAsync}`;

  // Make the deck and buttons.
  const deck = doc.createElement("named-deck");
  deck.id = deckId;
  for (let name of DEFAULT_SECTION_NAMES) {
    deck.appendChild(makeSection({doc, name}));
  }
  const buttons = doc.createElement("div");
  for (let name of DEFAULT_SECTION_NAMES) {
    buttons.appendChild(makeButton({doc, name, deckId}));
  }

  let ordered;
  if (first == "deck") {
    ordered = [deck, buttons];
  } else if (first == "buttons") {
    ordered = [buttons, deck];
  } else {
    throw new Error("Invalid order");
  }

  // Insert them in the specified order, possibly async.
  doc.body.appendChild(ordered.shift());
  if (beAsync) {
    await new Promise(resolve => requestAnimationFrame(resolve));
  }
  doc.body.appendChild(ordered.shift());

  return {deck, buttons};
}

add_task(async function testNamedDeckAndButtons() {
  const win = await loadInitialView("extension");
  const doc = win.document;

  // Check adding the deck first.
  dump("Running deck first tests synchronously");
  await runTests(await setup({doc, beAsync: false, first: "deck"}));
  dump("Running deck first tests asynchronously");
  await runTests(await setup({doc, beAsync: true, first: "deck"}));

  // Check adding the buttons first.
  dump("Running buttons first tests synchronously");
  await runTests(await setup({doc, beAsync: false, first: "buttons"}));
  dump("Running buttons first tests asynchronously");
  await runTests(await setup({doc, beAsync: true, first: "buttons"}));

  await closeView(win);
});
