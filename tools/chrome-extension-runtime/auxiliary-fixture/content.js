// SPDX-License-Identifier: MPL-2.0
chrome.runtime.onMessage.addListener((message, _sender, respond) => {
  if (message?.auxiliaryDeclarative) {
    if (message.removeMatch) document.querySelector("#item")?.remove();
    respond({
      injected: document.documentElement.dataset.nativeDeclarative || "0",
    });
    return false;
  }
  if (message?.auxiliaryProbe) {
    (async () => {
      const area = message.area || "session";
      const report = { sessionType: typeof chrome.storage[area] };
      try {
        report.value = await chrome.storage[area].get("contractValue");
        if (message.setAccessLevel) {
          await chrome.storage[area].setAccessLevel({
            accessLevel: "TRUSTED_AND_UNTRUSTED_CONTEXTS",
          });
        }
      } catch (error) {
        report.error = String(error);
      }
      respond(report);
    })();
    return true;
  }
});
