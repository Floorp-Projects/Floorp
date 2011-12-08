function focusReceived() {
  sendAsyncMessage("Test:E10SFocusReceived");
}

function blurReceived() {
  sendAsyncMessage("Test:E10SBlurReceived");
}

addEventListener("focus", focusReceived, true);
addEventListener("blur", blurReceived, true);

addMessageListener("Test:E10SFocusTestFinished", function testFinished() {
  removeEventListener("focus", focusReceived, true);
  removeEventListener("blur", blurReceived, true);
  removeMessageListener("Test:E10SFocusTestFinished", testFinished);
});
