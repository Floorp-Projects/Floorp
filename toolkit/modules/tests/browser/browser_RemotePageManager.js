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
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
  });
}

function waitForPage(pages, url = TEST_URL) {
  return new Promise((resolve) => {
    function listener({ target }) {
      pages.removeMessageListener("RemotePage:Init", listener);

      waitForMessage(target, "RemotePage:Load").then(() => resolve(target));
    }

    pages.addMessageListener("RemotePage:Init", listener);
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
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

add_task(async function initialProcessData() {
  const includesTest = () => Services.cpmm.
    initialProcessData["RemotePageManager:urls"].includes(TEST_URL);
  is(includesTest(), false, "Shouldn't have test url in initial process data yet");

  const loadedPort = waitForPort(TEST_URL);
  is(includesTest(), true, "Should have test url when waiting for it to load");

  await loadedPort;
  is(includesTest(), false, "Should have test url removed when done listening");

  gBrowser.removeCurrentTab();
});

// Test that opening a page creates a port, sends the load event and then
// navigating to a new page sends the unload event. Going back should create a
// new port
add_task(async function init_navigate() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let loaded = new Promise(resolve => {
    function listener() {
      gBrowser.selectedBrowser.removeEventListener("load", listener, true);
      resolve();
    }
    gBrowser.selectedBrowser.addEventListener("load", listener, true);
    gBrowser.loadURI("about:blank");
  });

  await waitForMessage(port, "RemotePage:Unload");

  // Port should be destroyed now
  try {
    port.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    port.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }

  await loaded;

  gBrowser.goBack();
  port = await waitForPort(TEST_URL, false);

  port.sendAsyncMessage("Ping2");
  await waitForMessage(port, "Pong2");
  port.destroy();

  gBrowser.removeCurrentTab();
});

// Test that opening a page creates a port, sends the load event and then
// closing the tab sends the unload event
add_task(async function init_close() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  await unloadPromise;

  // Port should be destroyed now
  try {
    port.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    port.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }
});

// Tests that we can send messages to individual pages even when more than one
// is open
add_task(async function multiple_ports() {
  let port1 = await waitForPort(TEST_URL);
  is(port1.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let port2 = await waitForPort(TEST_URL);
  is(port2.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  port2.addMessageListener("Pong", failOnMessage);
  port1.sendAsyncMessage("Ping", { str: "foobar", counter: 0 });
  let message = await waitForMessage(port1, "Pong");
  port2.removeMessageListener("Pong", failOnMessage);
  is(message.data.str, "foobar", "String should pass through");
  is(message.data.counter, 1, "Counter should be incremented");

  port1.addMessageListener("Pong", failOnMessage);
  port2.sendAsyncMessage("Ping", { str: "foobaz", counter: 5 });
  message = await waitForMessage(port2, "Pong");
  port1.removeMessageListener("Pong", failOnMessage);
  is(message.data.str, "foobaz", "String should pass through");
  is(message.data.counter, 6, "Counter should be incremented");

  let unloadPromise = waitForMessage(port2, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(port2.browser));
  await unloadPromise;

  try {
    port2.addMessageListener("Pong", failOnMessage);
    ok(false, "Should not have been able to add a new message listener to a destroyed port.");
  } catch (e) {
    ok(true, "Should not have been able to add a new message listener to a destroyed port.");
  }

  port1.sendAsyncMessage("Ping", { str: "foobar", counter: 0 });
  message = await waitForMessage(port1, "Pong");
  is(message.data.str, "foobar", "String should pass through");
  is(message.data.counter, 1, "Counter should be incremented");

  unloadPromise = waitForMessage(port1, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(port1.browser));
  await unloadPromise;
});

// Tests that swapping browser docshells doesn't break the ports
add_task(async function browser_switch() {
  let port1 = await waitForPort(TEST_URL);
  is(port1.browser, gBrowser.selectedBrowser, "Port is for the correct browser");
  let browser1 = gBrowser.selectedBrowser;
  port1.sendAsyncMessage("SetCookie", { value: "om nom" });

  let port2 = await waitForPort(TEST_URL);
  is(port2.browser, gBrowser.selectedBrowser, "Port is for the correct browser");
  let browser2 = gBrowser.selectedBrowser;
  port2.sendAsyncMessage("SetCookie", { value: "om nom nom" });

  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  let message = await waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = await waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  swapDocShells(browser1, browser2);
  is(port1.browser, browser2, "Should have noticed the swap");
  is(port2.browser, browser1, "Should have noticed the swap");

  // Cookies should have stayed the same
  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  message = await waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = await waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  swapDocShells(browser1, browser2);
  is(port1.browser, browser1, "Should have noticed the swap");
  is(port2.browser, browser2, "Should have noticed the swap");

  // Cookies should have stayed the same
  port2.addMessageListener("Cookie", failOnMessage);
  port1.sendAsyncMessage("GetCookie");
  message = await waitForMessage(port1, "Cookie");
  port2.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom", "Should have the right cookie");

  port1.addMessageListener("Cookie", failOnMessage);
  port2.sendAsyncMessage("GetCookie", { str: "foobaz", counter: 5 });
  message = await waitForMessage(port2, "Cookie");
  port1.removeMessageListener("Cookie", failOnMessage);
  is(message.data.value, "om nom nom", "Should have the right cookie");

  let unloadPromise = waitForMessage(port2, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser2));
  await unloadPromise;

  unloadPromise = waitForMessage(port1, "RemotePage:Unload");
  gBrowser.removeTab(gBrowser.getTabForBrowser(browser1));
  await unloadPromise;
});

// Tests that removeMessageListener in chrome works
add_task(async function remove_chrome_listener() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // This relies on messages sent arriving in the same order. Pong will be
  // sent back before Pong2 so if removeMessageListener fails the test will fail
  port.addMessageListener("Pong", failOnMessage);
  port.removeMessageListener("Pong", failOnMessage);
  port.sendAsyncMessage("Ping", { str: "remove_listener", counter: 27 });
  port.sendAsyncMessage("Ping2");
  await waitForMessage(port, "Pong2");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  await unloadPromise;
});

// Tests that removeMessageListener in content works
add_task(async function remove_content_listener() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // This relies on messages sent arriving in the same order. Pong3 would be
  // sent back before Pong2 so if removeMessageListener fails the test will fail
  port.addMessageListener("Pong3", failOnMessage);
  port.sendAsyncMessage("Ping3");
  port.sendAsyncMessage("Ping2");
  await waitForMessage(port, "Pong2");

  let unloadPromise = waitForMessage(port, "RemotePage:Unload");
  gBrowser.removeCurrentTab();
  await unloadPromise;
});

// Test RemotePages works
add_task(async function remote_pages_basic() {
  let pages = new RemotePages(TEST_URL);
  let port = await waitForPage(pages);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  // Listening to global messages should work
  let unloadPromise = waitForMessage(pages, "RemotePage:Unload", port);
  gBrowser.removeCurrentTab();
  await unloadPromise;

  pages.destroy();

  // RemotePages should be destroyed now
  try {
    pages.addMessageListener("Foo", failOnMessage);
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }

  try {
    pages.sendAsyncMessage("Foo");
    ok(false, "Should have seen exception");
  } catch (e) {
    ok(true, "Should have seen exception");
  }
});

// Test that properties exist on the target port provided to listeners
add_task(async function check_port_properties() {
  let pages = new RemotePages(TEST_URL);

  const expectedProperties = [
    "addMessageListener",
    "browser",
    "destroy",
    "loaded",
    "portID",
    "removeMessageListener",
    "sendAsyncMessage",
    "url"
  ];
  function checkProperties(port, description) {
    const expected = [];
    const unexpected = [];
    for (const key in port) {
      (expectedProperties.includes(key) ? expected : unexpected).push(key);
    }
    is(`${expected.sort()}`, expectedProperties, `${description} has expected keys`);
    is(`${unexpected.sort()}`, "", `${description} should not have unexpected keys`);
  }

  function portFrom(message, extraFn = () => {}) {
    return new Promise(resolve => {
      function onMessage({target}) {
        pages.removeMessageListener(message, onMessage);
        resolve(target);
      }
      pages.addMessageListener(message, onMessage);
      extraFn();
    });
  }

  let portFromInit = await portFrom("RemotePage:Init", () =>
    (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, TEST_URL)));
  checkProperties(portFromInit, "inited port");
  ok(["about:blank", TEST_URL].includes(portFromInit.browser.currentURI.spec),
    `inited port browser is either still blank or already at the target url - got ${portFromInit.browser.currentURI.spec}`);
  is(portFromInit.loaded, false, "inited port has not been loaded yet");
  is(portFromInit.url, TEST_URL, "got expected url");

  let portFromLoad = await portFrom("RemotePage:Load");
  is(portFromLoad, portFromInit, "got the same port from init and load");
  checkProperties(portFromLoad, "loaded port");
  is(portFromInit.browser.currentURI.spec, TEST_URL, "loaded port has browser with actual url");
  is(portFromInit.loaded, true, "loaded port is now loaded");
  is(portFromInit.url, TEST_URL, "still got expected url");

  let portFromUnload = await portFrom("RemotePage:Unload", () =>
    BrowserTestUtils.removeTab(gBrowser.selectedTab));
  is(portFromUnload, portFromInit, "got the same port from init and unload");
  checkProperties(portFromUnload, "unloaded port");
  is(portFromInit.browser, null, "unloaded port has no browser");
  is(portFromInit.loaded, false, "unloaded port is now not loaded");
  is(portFromInit.url, TEST_URL, "still got expected url");

  pages.destroy();
});

// Test sending messages to all remote pages works
add_task(async function remote_pages_multiple_pages() {
  let pages = new RemotePages(TEST_URL);
  let port1 = await waitForPage(pages);
  let port2 = await waitForPage(pages);

  let pongPorts = [];
  await new Promise((resolve) => {
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

// Test that RemotePages with multiple urls works
add_task(async function remote_pages_multiple_urls() {
  const TEST_URLS = [TEST_URL, TEST_URL.replace(".html", "2.html")];
  const pages = new RemotePages(TEST_URLS);

  const ports = [];
  // Load two pages for each url
  for (const [i, url] of TEST_URLS.entries()) {
    const port = await waitForPage(pages, url);
    is(port.browser, gBrowser.selectedBrowser, `port${i} is for the correct browser`);
    ports.push(port);
    ports.push(await waitForPage(pages, url));
  }

  let unloadPromise = waitForMessage(pages, "RemotePage:Unload", ports.pop());
  gBrowser.removeCurrentTab();
  await unloadPromise;

  const pongPorts = new Set();
  await new Promise(resolve => {
    function listener({ name, target, data }) {
      is(name, "Pong", "Should have seen the right response.");
      is(data.str, "FAKE_DATA", "String should pass through");
      is(data.counter, 1235, "Counter should be incremented");
      pongPorts.add(target);
      if (pongPorts.size === ports.length)
        resolve();
    }

    pages.addMessageListener("Pong", listener);
    pages.sendAsyncMessage("Ping", {str: "FAKE_DATA", counter: 1234});
  });

  ports.forEach(port => ok(pongPorts.has(port)));

  pages.destroy();
  ports.forEach(port => gBrowser.removeTab(gBrowser.getTabForBrowser(port.browser)));
});

// Test sending various types of data across the boundary
add_task(async function send_data() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let data = {
    integer: 45,
    real: 45.78,
    str: "foobar",
    array: [1, 2, 3, 5, 27]
  };

  port.sendAsyncMessage("SendData", data);
  let message = await waitForMessage(port, "ReceivedData");

  ok(message.data.result, message.data.status);

  gBrowser.removeCurrentTab();
});

// Test sending an object of data across the boundary
add_task(async function send_data2() {
  let port = await waitForPort(TEST_URL);
  is(port.browser, gBrowser.selectedBrowser, "Port is for the correct browser");

  let data = {
    integer: 45,
    real: 45.78,
    str: "foobar",
    array: [1, 2, 3, 5, 27]
  };

  port.sendAsyncMessage("SendData2", {data});
  let message = await waitForMessage(port, "ReceivedData2");

  ok(message.data.result, message.data.status);

  gBrowser.removeCurrentTab();
});

add_task(async function get_ports_for_browser() {
  let pages = new RemotePages(TEST_URL);
  let port = await waitForPage(pages);
  // waitForPage creates a new tab and selects it by default, so
  // the selected tab should be the one hosting this port.
  let browser = gBrowser.selectedBrowser;
  let foundPorts = pages.portsForBrowser(browser);
  is(foundPorts.length, 1, "There should only be one port for this simple page");
  is(foundPorts[0], port, "Should find the port");

  pages.destroy();
  gBrowser.removeCurrentTab();
});
