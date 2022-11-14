/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_richlistbox_keyboard() {
  await SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  await BrowserTestUtils.withNewTab("about:about", browser => {
    let document = browser.contentDocument;
    let box = document.createXULElement("richlistbox");

    function checkTabIndices(selectedLine) {
      for (let button of box.querySelectorAll(`.line${selectedLine} button`)) {
        is(
          button.tabIndex,
          0,
          `Should have ensured buttons inside selected line ${selectedLine} are focusable`
        );
      }
      for (let otherButton of box.querySelectorAll(
        `richlistitem:not(.line${selectedLine}) button`
      )) {
        is(
          otherButton.tabIndex,
          -1,
          `Should have ensured buttons outside selected line ${selectedLine} are not focusable`
        );
      }
    }

    let poem = `I wandered lonely as a cloud
         That floats on high o'er vales and hills
         When all at once I saw a crowd
         A host, of golden daffodils;
         Beside the lake, beneath the trees,
         Fluttering and dancing in the breeze.`;
    let items = poem.split("\n").map((line, index) => {
      let item = document.createXULElement("richlistitem");
      item.className = `line${index + 1}`;
      let button1 = document.createXULElement("button");
      button1.textContent = "Like";
      let button2 = document.createXULElement("button");
      button2.textContent = "Subscribe";
      item.append(line.trim(), button1, button2);
      return item;
    });
    box.append(...items);
    document.body.prepend(box);
    box.focus();
    box.getBoundingClientRect(); // force a flush
    box.selectedItem = box.firstChild;
    checkTabIndices(1);
    EventUtils.synthesizeKey("VK_DOWN", {}, document.defaultView);
    is(
      box.selectedItem.className,
      "line2",
      "Should have moved selection to the next line."
    );
    checkTabIndices(2);
    EventUtils.synthesizeKey("VK_TAB", {}, document.defaultView);
    is(
      document.activeElement,
      box.selectedItem.querySelector("button"),
      "Initial button gets focus in the selected list item."
    );
    EventUtils.synthesizeKey("VK_UP", {}, document.defaultView);
    checkTabIndices(1);
    is(
      document.activeElement,
      box.selectedItem.querySelector("button"),
      "Initial button gets focus in the selected list item when moving up with arrow key."
    );
    EventUtils.synthesizeKey("VK_DOWN", {}, document.defaultView);
    checkTabIndices(2);
    is(
      document.activeElement,
      box.selectedItem.querySelector("button"),
      "Initial button gets focus in the selected list item when moving down with arrow key."
    );
  });
});
