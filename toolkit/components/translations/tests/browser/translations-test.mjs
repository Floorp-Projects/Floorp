/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// eslint-disable-next-line no-unused-vars
let ok;
let is;
// eslint-disable-next-line no-unused-vars
let isnot;
let ContentTaskUtils;

/** @type {{ document: Document, window: Window }} */
let content;

/**
 * Inject the global variables from the test scope into the ES module scope.
 */
export function setup(config) {
  // When a function is provided to `ContentTask.spawn`, that function is provided the
  // Assert library through variable capture. In this case, this code is an ESM module,
  // and does not have access to that scope. To work around this issue, pass any any
  // relevant variables to that can be bound to the module scope.
  //
  // See: https://searchfox.org/mozilla-central/rev/cdddec7fd690700efa4d6b48532cf70155e0386b/testing/mochitest/BrowserTestUtils/content/content-task.js#78
  const { Assert } = config;
  ok = Assert.ok.bind(Assert);
  is = Assert.equal.bind(Assert);
  isnot = Assert.notEqual.bind(Assert);

  ContentTaskUtils = config.ContentTaskUtils;
  content = config.content;
}

export function getSelectors() {
  return {
    getH1() {
      return content.document.querySelector("h1");
    },
    getHeader() {
      return content.document.querySelector("header");
    },
    getFirstParagraph() {
      return content.document.querySelector("p:first-of-type");
    },
    getLastParagraph() {
      return content.document.querySelector("p:last-of-type");
    },
    getSpanishParagraph() {
      return content.document.getElementById("spanish-paragraph");
    },
    getSpanishHyperlink() {
      return content.document.getElementById("spanish-hyperlink");
    },
    getEnglishHyperlink() {
      return content.document.getElementById("english-hyperlink");
    },
  };
}

/**
 * Provide longer defaults for the waitForCondition.
 *
 * @param {Function} callback
 * @param {string} messages
 */
function waitForCondition(callback, message) {
  const interval = 100;
  // Use 4 times the defaults to guard against intermittents. Many of the tests rely on
  // communication between the parent and child process, which is inherently async.
  const maxTries = 50 * 4;
  return ContentTaskUtils.waitForCondition(
    callback,
    message,
    interval,
    maxTries
  );
}

/**
 * Asserts that a page was translated with a specific result.
 *
 * @param {string} message The assertion message.
 * @param {Function} getNode A function to get the node.
 * @param {string} translation The translated message.
 */
export async function assertTranslationResult(message, getNode, translation) {
  try {
    await waitForCondition(
      () => translation === getNode()?.innerText,
      `Waiting for: "${translation}"`
    );
  } catch (error) {
    // The result wasn't found, but the assertion below will report the error.
    console.error(error);
  }

  is(translation, getNode()?.innerText, message);
}

/**
 * Simulates right-clicking an element with the mouse.
 *
 * @param {element} element - The element to right-click.
 */
export function rightClickContentElement(element) {
  return new Promise(resolve => {
    element.addEventListener(
      "contextmenu",
      function () {
        resolve();
      },
      { once: true }
    );

    const EventUtils = ContentTaskUtils.getEventUtils(content);
    EventUtils.sendMouseEvent({ type: "contextmenu" }, element, content.window);
  });
}

/**
 * Selects all the content within a specified element.
 *
 * @param {Element} element - The element containing the content to be selected.
 * @returns {string} - The text content of the selection.
 */
export function selectContentElement(element) {
  content.focus();
  content.getSelection().selectAllChildren(element);
  return element.textContent;
}
