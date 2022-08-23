const port = browser.runtime.connectNative("browser");
port.onMessage.addListener(message => {
  handleMessage(message, null);
});

browser.runtime.onMessage.addListener((message, sender) => {
  handleMessage(message, sender.tab.id);
});

browser.pageAction.onClicked.addListener(tab => {
  port.postMessage({ method: "onClicked", tabId: tab.id, type: "pageAction" });
});

browser.browserAction.onClicked.addListener(tab => {
  port.postMessage({
    method: "onClicked",
    tabId: tab.id,
    type: "browserAction",
  });
});

function handlePageActionMessage(message, tabId) {
  switch (message.action) {
    case "enable":
      browser.pageAction.show(tabId);
      break;

    case "disable":
      browser.pageAction.hide(tabId);
      break;

    case "setPopup":
      browser.pageAction.setPopup({
        tabId,
        popup: message.popup,
      });
      break;

    case "setPopupCheckRestrictions":
      browser.pageAction
        .setPopup({
          tabId,
          popup: message.popup,
        })
        .then(
          () => {
            port.postMessage({
              resultFor: "setPopup",
              type: "pageAction",
              success: true,
            });
          },
          err => {
            port.postMessage({
              resultFor: "setPopup",
              type: "pageAction",
              success: false,
              error: String(err),
            });
          }
        );
      break;

    case "setTitle":
      browser.pageAction.setTitle({
        tabId,
        title: message.title,
      });
      break;

    case "setIcon":
      browser.pageAction.setIcon({
        tabId,
        imageData: message.imageData,
        path: message.path,
      });
      break;

    default:
      throw new Error(`Page Action does not support ${message.action}`);
  }
}

function handleBrowserActionMessage(message, tabId) {
  switch (message.action) {
    case "enable":
      browser.browserAction.enable(tabId);
      break;

    case "disable":
      browser.browserAction.disable(tabId);
      break;

    case "setBadgeText":
      browser.browserAction.setBadgeText({
        tabId,
        text: message.text,
      });
      break;

    case "setBadgeTextColor":
      browser.browserAction.setBadgeTextColor({
        tabId,
        color: message.color,
      });
      break;

    case "setBadgeBackgroundColor":
      browser.browserAction.setBadgeBackgroundColor({
        tabId,
        color: message.color,
      });
      break;

    case "setPopup":
      browser.browserAction.setPopup({
        tabId,
        popup: message.popup,
      });
      break;

    case "setPopupCheckRestrictions":
      browser.browserAction
        .setPopup({
          tabId,
          popup: message.popup,
        })
        .then(
          () => {
            port.postMessage({
              resultFor: "setPopup",
              type: "browserAction",
              success: true,
            });
          },
          err => {
            port.postMessage({
              resultFor: "setPopup",
              type: "browserAction",
              success: false,
              error: String(err),
            });
          }
        );
      break;

    case "setTitle":
      browser.browserAction.setTitle({
        tabId,
        title: message.title,
      });
      break;

    case "setIcon":
      browser.browserAction.setIcon({
        tabId,
        imageData: message.imageData,
        path: message.path,
      });
      break;

    default:
      throw new Error(`Browser Action does not support ${message.action}`);
  }
}

function handleMessage(message, tabId) {
  switch (message.type) {
    case "ping":
      port.postMessage({ method: "pong" });
      return;

    case "load":
      browser.tabs.update(tabId, {
        url: message.url,
      });
      return;

    case "browserAction":
      handleBrowserActionMessage(message, tabId);
      return;

    case "pageAction":
      handlePageActionMessage(message, tabId);
      return;

    default:
      throw new Error(`Unsupported message type ${message.type}`);
  }
}
