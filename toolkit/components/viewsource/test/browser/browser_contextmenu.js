/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var source =
  "data:text/html,text<link%20href='http://example.com/'%20/>more%20text<a%20href='mailto:abc@def.ghi'>email</a>";
var gViewSourceWindow, gContextMenu, gCopyLinkMenuItem, gCopyEmailMenuItem;

var expectedData = [];

add_task(async function() {
  // Full source in view source tab
  let newTab = await openDocument(source);
  await onViewSourceWindowOpen(window);

  let contextMenu = document.getElementById("contentAreaContextMenu");

  for (let test of expectedData) {
    await checkMenuItems(contextMenu, test[0], test[1], test[2], test[3]);
  }

  gBrowser.removeTab(newTab);

  // Selection source in view source tab
  expectedData = [];
  newTab = await openDocumentSelect(source, "body");
  await onViewSourceWindowOpen(window);

  contextMenu = document.getElementById("contentAreaContextMenu");

  for (let test of expectedData) {
    await checkMenuItems(contextMenu, test[0], test[1], test[2], test[3]);
  }

  gBrowser.removeTab(newTab);
});

async function onViewSourceWindowOpen(aWindow) {
  gViewSourceWindow = aWindow;

  gCopyLinkMenuItem = aWindow.document.getElementById("context-copylink");
  gCopyEmailMenuItem = aWindow.document.getElementById("context-copyemail");

  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [], async function(arg) {
    let tags = content.document.querySelectorAll("a[href]");
    Assert.equal(
      tags[0].href,
      "view-source:http://example.com/",
      "Link has correct href"
    );
    Assert.equal(tags[1].href, "mailto:abc@def.ghi", "Link has correct href");
  });

  expectedData.push(["a[href]", true, false, "http://example.com/"]);
  expectedData.push(["a[href^=mailto]", false, true, "abc@def.ghi"]);
  expectedData.push(["span", false, false, null]);
}

async function checkMenuItems(
  contextMenu,
  selector,
  copyLinkExpected,
  copyEmailExpected,
  expectedClipboardContent
) {
  let browser = gBrowser.selectedBrowser;
  await SpecialPowers.spawn(browser, [{ selector }], async function(arg) {
    content.document.querySelector(arg.selector).scrollIntoView();
  });

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    selector,
    { type: "contextmenu", button: 2 },
    browser
  );
  await popupShownPromise;

  is(
    gCopyLinkMenuItem.hidden,
    !copyLinkExpected,
    "Copy link menuitem is " + (copyLinkExpected ? "not hidden" : "hidden")
  );
  is(
    gCopyEmailMenuItem.hidden,
    !copyEmailExpected,
    "Copy email menuitem is " + (copyEmailExpected ? "not hidden" : "hidden")
  );

  if (copyLinkExpected || copyEmailExpected) {
    await new Promise((resolve, reject) => {
      waitForClipboard(
        expectedClipboardContent,
        function() {
          if (copyLinkExpected) {
            gCopyLinkMenuItem.click();
          } else {
            gCopyEmailMenuItem.click();
          }
        },
        resolve,
        reject
      );
    });
  }

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}
