const {
  ContentEventObserverService,
  WebElementEventTarget,
} = ChromeUtils.import("chrome://marionette/content/dom.js");

class MessageSender {
  constructor() {
    this.listeners = {};
    this.sent = [];
  }

  addMessageListener(name, listener) {
    this.listeners[name] = listener;
  }

  sendAsyncMessage(name, data) {
    this.sent.push({ name, data });
  }
}

class Window {
  constructor() {
    this.events = [];
  }

  addEventListener(type) {
    this.events.push(type);
  }

  removeEventListener(type) {
    for (let i = 0; i < this.events.length; ++i) {
      if (this.events[i] === type) {
        this.events.splice(i, 1);
        return;
      }
    }
  }
}

add_test(function test_WebElementEventTarget_addEventListener_init() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);
  equal(Object.keys(eventTarget.listeners).length, 0);
  equal(Object.keys(ipc.listeners).length, 1);

  run_next_test();
});

add_test(function test_addEventListener() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  let listener = () => {};
  eventTarget.addEventListener("click", listener);

  // click listener was appended
  equal(Object.keys(eventTarget.listeners).length, 1);
  ok("click" in eventTarget.listeners);
  equal(eventTarget.listeners.click.length, 1);
  equal(eventTarget.listeners.click[0], listener);

  // should have sent a registration message
  deepEqual(ipc.sent[0], {
    name: "Marionette:DOM:AddEventListener",
    data: { type: "click" },
  });

  run_next_test();
});

add_test(function test_addEventListener_sameReference() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  let listener = () => {};
  eventTarget.addEventListener("click", listener);
  eventTarget.addEventListener("click", listener);
  equal(eventTarget.listeners.click.length, 1);

  run_next_test();
});

add_test(function test_WebElementEventTarget_addEventListener_once() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  eventTarget.addEventListener("click", () => {}, { once: true });
  equal(eventTarget.listeners.click[0].once, true);

  eventTarget.dispatchEvent({ type: "click" });
  equal(eventTarget.listeners.click.length, 0);
  deepEqual(ipc.sent[1], {
    name: "Marionette:DOM:RemoveEventListener",
    data: { type: "click" },
  });

  run_next_test();
});

add_test(function test_WebElementEventTarget_removeEventListener() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  equal(Object.keys(eventTarget.listeners).length, 0);
  eventTarget.removeEventListener("click", () => {});
  equal(Object.keys(eventTarget.listeners).length, 0);

  let firstListener = () => {};
  eventTarget.addEventListener("click", firstListener);
  equal(eventTarget.listeners.click.length, 1);
  ok(eventTarget.listeners.click[0] === firstListener);

  let secondListener = () => {};
  eventTarget.addEventListener("click", secondListener);
  equal(eventTarget.listeners.click.length, 2);
  ok(eventTarget.listeners.click[1] === secondListener);

  ok(eventTarget.listeners.click[0] !== eventTarget.listeners.click[1]);

  eventTarget.removeEventListener("click", secondListener);
  equal(eventTarget.listeners.click.length, 1);
  ok(eventTarget.listeners.click[0] === firstListener);

  // event should not have been unregistered
  // because there still exists another click event
  equal(ipc.sent[ipc.sent.length - 1].name, "Marionette:DOM:AddEventListener");

  eventTarget.removeEventListener("click", firstListener);
  equal(eventTarget.listeners.click.length, 0);
  deepEqual(ipc.sent[ipc.sent.length - 1], {
    name: "Marionette:DOM:RemoveEventListener",
    data: { type: "click" },
  });

  run_next_test();
});

add_test(function test_WebElementEventTarget_dispatchEvent() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  let listenerCalled = false;
  let listener = () => (listenerCalled = true);
  eventTarget.addEventListener("click", listener);
  eventTarget.dispatchEvent({ type: "click" });
  ok(listenerCalled);

  run_next_test();
});

add_test(function test_WebElementEventTarget_dispatchEvent_multipleListeners() {
  let ipc = new MessageSender();
  let eventTarget = new WebElementEventTarget(ipc);

  let clicksA = 0;
  let clicksB = 0;
  let listenerA = () => ++clicksA;
  let listenerB = () => ++clicksB;

  // the same listener should only be added, and consequently fire, once
  eventTarget.addEventListener("click", listenerA);
  eventTarget.addEventListener("click", listenerA);
  eventTarget.addEventListener("click", listenerB);
  eventTarget.dispatchEvent({ type: "click" });
  equal(clicksA, 1);
  equal(clicksB, 1);

  run_next_test();
});

add_test(function test_ContentEventObserverService_add() {
  let ipc = new MessageSender();
  let win = new Window();
  let obs = new ContentEventObserverService(
    win,
    ipc.sendAsyncMessage.bind(ipc)
  );

  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  obs.add("foo");
  equal(obs.events.size, 1);
  equal(win.events.length, 1);
  equal(obs.events.values().next().value, "foo");
  equal(win.events[0], "foo");

  obs.add("foo");
  equal(obs.events.size, 1);
  equal(win.events.length, 1);

  run_next_test();
});

add_test(function test_ContentEventObserverService_remove() {
  let ipc = new MessageSender();
  let win = new Window();
  let obs = new ContentEventObserverService(
    win,
    ipc.sendAsyncMessage.bind(ipc)
  );

  obs.remove("foo");
  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  obs.add("bar");
  equal(obs.events.size, 1);
  equal(win.events.length, 1);

  obs.remove("bar");
  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  obs.add("baz");
  obs.add("baz");
  equal(obs.events.size, 1);
  equal(win.events.length, 1);

  obs.add("bah");
  equal(obs.events.size, 2);
  equal(win.events.length, 2);

  obs.remove("baz");
  equal(obs.events.size, 1);
  equal(win.events.length, 1);

  obs.remove("bah");
  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  run_next_test();
});

add_test(function test_ContentEventObserverService_clear() {
  let ipc = new MessageSender();
  let win = new Window();
  let obs = new ContentEventObserverService(
    win,
    ipc.sendAsyncMessage.bind(ipc)
  );

  obs.clear();
  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  obs.add("foo");
  obs.add("foo");
  obs.add("bar");
  equal(obs.events.size, 2);
  equal(win.events.length, 2);

  obs.clear();
  equal(obs.events.size, 0);
  equal(win.events.length, 0);

  run_next_test();
});

add_test(function test_ContentEventObserverService_handleEvent() {
  let ipc = new MessageSender();
  let win = new Window();
  let obs = new ContentEventObserverService(
    win,
    ipc.sendAsyncMessage.bind(ipc)
  );

  obs.handleEvent({ type: "click", target: win });
  deepEqual(ipc.sent[0], {
    name: "Marionette:DOM:OnEvent",
    data: { type: "click" },
  });

  run_next_test();
});
