/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "http://www.example.com/browser/toolkit/modules/tests/browser/testremotepagemanager.html";

var { RemotePages, RemotePageManager } = Cu.import("resource://gre/modules/RemotePageManager.jsm", {});

function failOnMessage(message) {
  ok(false, "Should not have seen message " + message.name);
}

function waitForMessage(port, message, expectedPort = port) {
  return new Promise((resolve) => {
    function listener(message) {
      is(message.target, expectedPort, "Message should be from the right port.");

      port.removeMessageListener(listener);
      resolve(message);
    }

    port.addMessageListener(message, listener);
  });
}

function waitForPort(url, createTab = true) {
  return new Promise((resolve) => {
    RemotePageManager.addRemotePageListener(url, (port) => {
      RemotePageManager.removeRemotePageListener(url);

      waitForMessage(port, "RemotePage:Load").then(() => resolve(port));
    });

    if (createTab)
      gBrowser.selectedTab = gBrowser.addTab(url);
  });
}

function waitForPage(pages) {
  return new Promise((resolve) => {
    function listener({ target }) {
      pages.removeMessageListener("RemotePage:Init", listener);

      waitForMessage(target, "RemotePage:Load").then(() => resolve(target));
    }

    pages.addMessageListener("RemotePage:Init", listener);
    gBrowser.selectedTab = gBrowser.addTab(TEST_URL);
  });
}

function swapDocShells(browser1, browser2) {
  // Swap frameLoaders.
  browser1.swapDocShells(browser2);

  // Swap permanentKeys.
  let tmp = browser1.permanentKey;
  browser1.permanentKey = browser2.permanentKey;
  browser2.permanentKey = tmp;
}

// Test that opening a page creates a port, sends the load event and then
// navigating to a new page sends the unload event. Going back should create a
// new port
add_task(function* init_navigate() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let loaded = new Promise(resolve => {
    function listener() {
      gBrowser.selectedBrowser.removeEventListener("load", listener, true);
      resolve();
    }
    gBrowser.selectedBrowser.addEventListener("load", listener, true);
    gBrowser.loadURI("about:blank");
  });

  yield waitForMessage(port, "RemotePage:Unload");

  // Port should be destroyed now
  try {
    port.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    port.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }

  yield loaded;

  gBrowser.goBack();
  port = yield waitForPort(TEST_URL, false);

  port.sendAsyncMessage("Ping2");
  let message = yield waitForMessage(port, "Pong2");
  port.destroy();

  gBrowser.removeCurrentTab();
});

// Test that opening a page creates a port, sends the load event and then
// closing the tab sends the unload event
add_task(function* init_close() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  yield unloadPromise;

  // Port should be destroyed now
  try {
    port.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    port.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }
});

// Tests that we can send messages to individual pages even when more than one
// is open
add_task(function* multiple_ports() {
  let port1 = yield waitForPort(TEST_URL);
  is(port1.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let port2 = yield waitForPort(TEST_URL);
  is(port2.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  port2.addMessageListener("Pong", failOnMessage);
  port1.sendAsyncMessage("Ping", { str: "foobar", counter: 0 });
  let message = yield waitForMessage(port1, "Pong");
  port2.removeMessageListener("Pong", failOnMessage);
  is(message.data.str, "foobar", "String should pass through");
  is(message.data.counter, 1, "Counter should be incremented");

  port1.addMessageListener("Pong", failOnMessage);
  port2.sendAsyncMessage("Ping", { str: "foobaz", counter: 5 });
  message = yield waitForMessage(port2, "Pong");
  port1.removeMessageListener("Pong", failOnMessage);
  is(message.data.str, "foobaz", "String should pass through");
  is(message.data.counter, 6, "Counter should be incremented");

  let unloadPromise = waitForMessage(port2, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(port2.browser));
  yield unloadPromise;

  try {
    port2.addMessageListener("Pong", failOnMessage);
    ok(false, "Should not have been able to add a new message listener to a destroyed port.");
  }
  catch (e) {
    ok(true, "Should not have been able to add a new message listener to a destroyed port.");
  }

  port1.sendAsyncMessage("Ping", { str: "foobar", counter: 0 });
  message = yield waitForMessage(port1, "Pong");
  is(message.data.str, "foobar", "String should pass through");
  is(message.data.counter, 1, "Counter should be incremented");

  unloadPromise = waitForMessage(port1, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(port1.browser));
  yield unloadPromise;
});

// Tests that swapping browser docshells doesn't break the ports
add_task(function* browser_switch() {
  let port1 = yield waitForPort(TEST_URL);
  is(port1.browser, gBrowser.selectedBrowser, "Port is for the correct browser");
  let browser1 = gBrowser.selectedBrowser;
  port1.sendAsyncMessage("SetCookie", { value: "om nom" });

  let port2 = yield waitForPort(TEST_URL);
  is(port2.browser, gBrowser.selectedBrowser, "Port is for the correct browser");
  let browser2 = gBrowser.selectedBrowser;
  port2.sendAsyncMessage("SetCookie", { value: "om nom nom" });

  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  let message = yield waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = yield waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  swapDocShells(browser1, browser2);
  is(port1.browser, browser2, "Should have noticed the swap");
  is(port2.browser, browser1, "Should have noticed the swap");

  // Cookies should have stayed the same
  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  message = yield waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = yield waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  swapDocShells(browser1, browser2);
  is(port1.browser, browser1, "Should have noticed the swap");
  is(port2.browser, browser2, "Should have noticed the swap");

  // Cookies should have stayed the same
  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  message = yield waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = yield waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  let unloadPromise = waitForMessage(port2, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser2));
  yield unloadPromise;

  unloadPromise = waitForMessage(port1, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser1));
  yield unloadPromise;
});

// Tests that removeMessageListener in chrome works
add_task(function* remove_chrome_listener() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // This relies on messages sent arriving in the same order. Pong will be
  // sent back before Pong2 so if removeMessageListener fails the test will fail
  port.addMessageListener("Pong", failOnMessage);
  port.removeMessageListener("Pong", failOnMessage);
  port.sendAsyncMessage("Ping", { str: "remove_listener", counter: 27 });
  port.sendAsyncMessage("Ping2");
  yield waitForMessage(port, "Pong2");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  yield unloadPromise;
});

// Tests that removeMessageListener in content works
add_task(function* remove_content_listener() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // This relies on messages sent arriving in the same order. Pong3 would be
  // sent back before Pong2 so if removeMessageListener fails the test will fail
  port.addMessageListener("Pong3", failOnMessage);
  port.sendAsyncMessage("Ping3");
  port.sendAsyncMessage("Ping2");
  yield waitForMessage(port, "Pong2");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  yield unloadPromise;
});

// Test RemotePages works
add_task(function* remote_pages_basic() {
  let pages = new RemotePages(TEST_URL);
  let port = yield waitForPage(pages);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // Listening to global messages should work
  let unloadPromise = waitForMessage(pages, "RemotePage:Unload", port);
  gBrowser.removeCurrentTab();
  yield unloadPromise;

  pages.destroy();

  // RemotePages should be destroyed now
  try {
    pages.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    pages.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  }
  catch (e) {
    ok(true, "Should have seen exception");
  }
});

// Test sending messages to all remote pages works
add_task(function* remote_pages_multiple() {
  let pages = new RemotePages(TEST_URL);
  let port1 = yield waitForPage(pages);
  let port2 = yield waitForPage(pages);

  let pongPorts = [];
  yield new Promise((resolve) => {
    function listener({ name, target, data }) {
      is(name, "Pong", "Should have seen the right response.");
      is(data.str, "remote_pages", "String should pass through");
      is(data.counter, 43, "Counter should be incremented");
      pongPorts.push(target);
      if (pongPorts.length == 2)
        resolve();
    }

    pages.addMessageListener("Pong", listener);
    pages.sendAsyncMessage("Ping", { str: "remote_pages", counter: 42 });
  });

  // We don't make any guarantees about which order messages are sent to known
  // pages so the pongs could have come back in any order.
  isnot(pongPorts[0], pongPorts[1], "Should have received pongs from different ports");
  ok(pongPorts.indexOf(port1) >= 0, "Should have seen a pong from port1");
  ok(pongPorts.indexOf(port2) >= 0, "Should have seen a pong from port2");

  // After destroy we should see no messages
  pages.addMessageListener("RemotePage:Unload", failOnMessage);
  pages.destroy();

  gBrowser.removeTab(gBrowser.getTabForBrowser(port1.browser));
  gBrowser.removeTab(gBrowser.getTabForBrowser(port2.browser));
});

// Test sending various types of data across the boundary
add_task(function* send_data() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let data = {
    integer: 45,
    real: 45.78,
    str: "foobar",
    array: [1, 2, 3, 5, 27]
  };

  port.sendAsyncMessage("SendData", data);
  let message = yield waitForMessage(port, "ReceivedData");

  ok(message.data.result, message.data.status);

  gBrowser.removeCurrentTab();
});

// Test sending an object of data across the boundary
add_task(function* send_data2() {
  let port = yield waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let data = {
    integer: 45,
    real: 45.78,
    str: "foobar",
    array: [1, 2, 3, 5, 27]
  };

  port.sendAsyncMessage("SendData2", {data});
  let message = yield waitForMessage(port, "ReceivedData2");

  ok(message.data.result, message.data.status);

  gBrowser.removeCurrentTab();
});

add_task(function* get_ports_for_browser() {
  let pages = new RemotePages(TEST_URL);
  let port = yield waitForPage(pages);
  // waitForPage creates a new tab and selects it by default, so
  // the selected tab should be the one hosting this port.
  let browser = gBrowser.selectedBrowser;
  let foundPorts = pages.portsForBrowser(browser);
  is(foundPorts.length, 1, "There should only be one port for this simple page");
  is(foundPorts[0], port, "Should find the port");
  gBrowser.removeCurrentTab();
});
