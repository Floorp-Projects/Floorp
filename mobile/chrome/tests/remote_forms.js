dump("====================== Content Script Loaded =======================\n");

let assistant = contentObject._formAssistant;

AsyncTests.add("Test:Click", function(aMessage, aJson) {
  sendMouseEvent({type: "click"}, "root", content);
  return assistant._open;
});

AsyncTests.add("Test:Open", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  return assistant.open(element);
});

AsyncTests.add("Test:CanShowUI", function(aMessage, aJson) {
  let element = content.document.querySelector(aJson.value);
  assistant._open = false;
  return assistant.open(element);
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

