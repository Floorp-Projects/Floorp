dump("====================== Content Script Loaded =======================\n");

let assistant = Content.formAssistant;

// Copied from http://mxr.mozilla.org/mozilla-central/source/testing/mochitest/tests/SimpleTest/EventUtils.js
// except the netscape.security.PrivilegeManager.enablePrivilege call
function sendMouseEvent(aEvent, aTarget, aWindow) {
  if (['click', 'mousedown', 'mouseup', 'mouseover', 'mouseout'].indexOf(aEvent.type) == -1) {
    throw new Error("sendMouseEvent doesn't know about event type '" + aEvent.type + "'");
  }

  if (!aWindow) {
    aWindow = window;
  }

  if (!(aTarget instanceof Element)) {
    aTarget = aWindow.document.getElementById(aTarget);
  }

  let event = aWindow.document.createEvent('MouseEvent');

  let typeArg          = aEvent.type;
  let canBubbleArg     = true;
  let cancelableArg    = true;
  let viewArg          = aWindow;
  let detailArg        = aEvent.detail        || (aEvent.type == 'click'     ||
                                                  aEvent.type == 'mousedown' ||
                                                  aEvent.type == 'mouseup' ? 1 : 0);
  let screenXArg       = aEvent.screenX       || 0;
  let screenYArg       = aEvent.screenY       || 0;
  let clientXArg       = aEvent.clientX       || 0;
  let clientYArg       = aEvent.clientY       || 0;
  let ctrlKeyArg       = aEvent.ctrlKey       || false;
  let altKeyArg        = aEvent.altKey        || false;
  let shiftKeyArg      = aEvent.shiftKey      || false;
  let metaKeyArg       = aEvent.metaKey       || false;
  let buttonArg        = aEvent.button        || 0;
  let relatedTargetArg = aEvent.relatedTarget || null;

  event.initMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg,
                       screenXArg, screenYArg, clientXArg, clientYArg,
                       ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg,
                       buttonArg, relatedTargetArg);

  aTarget.dispatchEvent(event);
}

AsyncTests.add("Test:Click", function(aMessage, aJson) {
  sendMouseEvent({type: "click"}, "root", content);
  return assistant._open;
});

AsyncTests.add("Test:Focus", function(aMessage, aJson) {
  let targetElement = content.document.querySelector(aJson.value);
  targetElement.focus();
  assistant._executeDelayed(function() {
    sendAsyncMessage(aMessage, { result: assistant._open });
  });
});

AsyncTests.add("Test:FocusRedirect", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  element.addEventListener("focus", function(aEvent) {
    element.removeEventListener("focus", arguments.callee, false);
    content.document.getElementById("root").focus();
  }, false);
  element.focus();

  assistant._executeDelayed(function() {
    sendAsyncMessage(aMessage, { result: assistant._open });
  });
});

// It should be only 2 ways to open the FormAssistant, the first one is
// by manually synchronizing the focus to the form helper and the other
// one is by a user click on an authorized element
AsyncTests.add("Test:OpenUIWithSyncFocus", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);

  assistant._open = false;
  assitant.focusSync = true;
  element.focus();
  assistant._executeDelayed(function() {
    assistant.focusSync = false;
    sendAsyncMessage(aMessage, { result: assistant._open });
  });
});

AsyncTests.add("Test:Open", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  assistant._open = false;
  return assistant.open(element);
});

AsyncTests.add("Test:OpenWithFocusRedirect", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  assistant._open = false;
  assistant.focusSync = true;
  assistant.open(element);
  assistant._executeDelayed(function() {
    assistant.focusSync = false;
    sendAsyncMessage(aMessage, { result: assistant._open });
  });
});

AsyncTests.add("Test:CanShowUI", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  element.disabled = aJson.disabled;
  assistant._open = false;
  let open = assistant.open(element);
  element.disabled = false;
  return open;
});

AsyncTests.add("Test:CanShowUISelect", function(aMessage, aJson) {
  let select = content.document.getElementById("select");
  select.disabled = aJson.disabled;

  let element = content.document.querySelector(aJson.value);
  assistant._open = false;
  let open = assistant.open(element);
  select.disabled = false;
  return open;
});

AsyncTests.add("Test:Previous", function(aMessage, aJson) {
  let targetElement = content.document.querySelector(aJson.value);
  assistant.currentIndex--;
  return (assistant.currentElement == targetElement);
});

AsyncTests.add("Test:Next", function(aMessage, aJson) {
  let targetElement = content.document.querySelector(aJson.value);
  assistant.currentIndex++;
  return (assistant.currentElement == targetElement);
});

// ============= iframe navigation ==================
let iframe = null;
let iframeInputs = null;
AsyncTests.add("Test:Iframe", function(aMessage, aJson) {
  iframe = content.document.createElement("iframe");
  iframe.setAttribute("src", "data:text/html;charset=utf-8,%3Ciframe%20src%3D%22data%3Atext/html%3Bcharset%3Dutf-8%2C%253Cinput%253E%253Cbr%253E%253Cinput%253E%250A%22%3E%3C/iframe%3E");
  iframe.setAttribute("width", "300");
  iframe.setAttribute("height", "100");

  iframe.addEventListener("load", function() {
    iframe.removeEventListener("load", arguments.callee, false);
    iframeInputs = iframe.contentDocument
                         .querySelector("iframe").contentDocument
                         .getElementsByTagName("input");
    sendAsyncMessage(aMessage, { result: true });
  }, false);

  content.document.body.appendChild(iframe);
});

AsyncTests.add("Test:IframeOpen", function(aMessage, aJson) {
  return assistant.open(iframeInputs[0]);
});

AsyncTests.add("Test:IframePrevious", function(aMessage, aJson) {
  assistant.currentIndex--;
  return (assistant.currentElement == iframeInputs[aJson.value]);
});

AsyncTests.add("Test:IframeNext", function(aMessage, aJson) {
  assistant.currentIndex++;
  return (assistant.currentElement == iframeInputs[aJson.value]);
});

