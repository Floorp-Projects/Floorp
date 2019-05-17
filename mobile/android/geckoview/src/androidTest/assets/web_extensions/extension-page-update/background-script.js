browser.runtime.onMessage.addListener(notify);

function notify(message) {
  if (message.action == "showTab") {
    browser.tabs.update({url: "/tab.html"});
  }
}
