let testURL = "chrome://mochikit/content/browser/browser_FormAssistant.html";
testURL = testURL.replace("chrome://mochikit/content/browser/", "file:///home/steakdepapillon/Devel/fennec/mobilebase/mobile/_tests/testing/mochitest/browser/mobile/chrome/");
let newTab = null;
let container = null;

function test() {
  // This test is async
  waitForExplicitFinish();

  // Add new tab to hold the <FormAssistant> page
  newTab = Browser.addTab(testURL, true);
  BrowserUI.closeAutoComplete(true);

  // Wait for the tab to load, then do the test
  waitFor(onTabLoaded, function() { return newTab._loading == false;});
}

function onTabLoaded() {
  container = document.getElementById("form-helper-container");
  testMouseEvents();
}

function testMouseEvents() {
  // Sending a synthesized event directly on content should not work - we
  // don't want web content to be able to open the form helper without the
  // user consent, so we have to pass throught the canvas tile-container
  EventUtils.sendMouseEvent({type: "click"}, "root", newTab.browser.contentWindow);
  is(FormHelper._open, false, "Form Assistant should stay closed");

  let element = newTab.browser.contentDocument.querySelector("*[tabindex='0']");
  EventUtils.synthesizeMouseForContent(element, 1, 1, {}, window);
  
  // XXX because InputHandler hold us for at least 400ms we should wait a bit here
  setTimeout (function() {
    ok(FormHelper._open, "Form Assistant should be open");
    testShowUIForElements();
  }, 1700);
};

function testShowUIForElements() {
  let doc = newTab.browser.contentDocument;

  ok(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='1']")), "canShowUI for input type='text'");
  ok(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='2']")), "canShowUI for input type='password'");
  is(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='3']")), false, "!canShowUI for input type='submit'");
  is(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='4']")), false, "!canShowUI for input type='file'");
  is(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='5']")), false, "!canShowUI for input button type='submit'");
  is(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='6']")), false, "!canShowUI for div@role='button'");
  is(FormHelper.canShowUIFor(doc.querySelector("*[tabindex='7']")), false, "!canShowUI for input type='image'");

  testTabIndexNavigation();
};

function testTabIndexNavigation() {
  // Open the Form Helper
  let firstElement = newTab.browser.contentDocument.getElementById("root");
  FormHelper.open(firstElement);
  is(container.hidden, false, "Form Assistant should be open");

  FormHelper.goToPrevious();
  let element = newTab.browser.contentDocument.querySelector("*[tabindex='0']");
  isnot(FormHelper.getCurrentElement(), element, "Focus should not have changed");

  FormHelper.goToNext();
  element = newTab.browser.contentDocument.querySelector("*[tabindex='2']");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with tab-index : 2");

  FormHelper.goToPrevious();
  element = newTab.browser.contentDocument.querySelector("*[tabindex='1']");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with tab-index : 1");

  FormHelper.goToNext();
  FormHelper.goToNext();
  FormHelper.goToNext();
  FormHelper.goToNext();
  FormHelper.goToNext();
  FormHelper.goToNext();

  element = newTab.browser.contentDocument.querySelector("*[tabindex='7']");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with tab-index : 7");

  FormHelper.goToNext();
  element = newTab.browser.contentDocument.querySelector("*[tabindex='0']");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with tab-index : 0");

  FormHelper.goToNext();
  element = newTab.browser.contentDocument.getElementById("next");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with #id: next");

  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "select",   "Focus should be on element with #id: select");
  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "dumb",     "Focus should be on element with #id: dumb");
  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "reset",    "Focus should be on element with #id: reset");
  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "checkbox", "Focus should be on element with #id: checkbox");
  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "radio0",   "Focus should be on element with #id: radio0");
  FormHelper.goToNext();
  is(FormHelper.getCurrentElement().id, "radio4",   "Focus should be on element with #id: radio4");

  FormHelper.goToNext();
  element = newTab.browser.contentDocument.getElementById("last");
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with #id: last");

  FormHelper.goToNext();
  is(FormHelper.getCurrentElement(), element, "Focus should be on element with #id: last");

  FormHelper.close();
  is(container.hidden, true, "Form Assistant should be close");

  navigateIntoNestedFrames();
};

function navigateIntoNestedFrames() {
  let doc = newTab.browser.contentDocument;

  let iframe = doc.createElement("iframe");
  iframe.setAttribute("src", "data:text/html;charset=utf-8,%3Ciframe%20src%3D%22data%3Atext/html%3Bcharset%3Dutf-8%2C%253Cinput%253E%253Cbr%253E%253Cinput%253E%250A%22%3E%3C/iframe%3E");
  iframe.setAttribute("width", "300");
  iframe.setAttribute("height", "100");

  iframe.addEventListener("load", function() {
    iframe.removeEventListener("load", arguments.callee, false);

    let elements = iframe.contentDocument
                         .querySelector("iframe").contentDocument
                         .getElementsByTagName("input");

    FormHelper.open(elements[0]);
    ok(FormHelper._getPrevious(), "It should be possible to access to the previous field");
    ok(FormHelper._getNext(), "It should be possible to access to the next field");
    FormHelper.goToNext();
    ok(!FormHelper._getNext(), "It should not be possible to access to the next field");

    doc.body.removeChild(iframe);

    // Close the form assistant
    FormHelper.close();

    // Close our tab when finished
    Browser.closeTab(newTab);

    // We must finialize the tests
    finish();
  }, false);
  doc.body.appendChild(iframe);
};

