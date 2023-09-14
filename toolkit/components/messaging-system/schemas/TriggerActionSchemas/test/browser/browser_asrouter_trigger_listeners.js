ChromeUtils.defineModuleGetter(
  this,
  "ASRouterTriggerListeners",
  "resource://activity-stream/lib/ASRouterTriggerListeners.jsm"
);
ChromeUtils.defineESModuleGetters(this, {
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

async function openURLInWindow(window, url) {
  const { selectedBrowser } = window.gBrowser;
  BrowserTestUtils.startLoadingURIString(selectedBrowser, url);
  await BrowserTestUtils.browserLoaded(selectedBrowser, false, url);
}

add_task(async function check_matchPatternFailureCase() {
  const articleTrigger = ASRouterTriggerListeners.get("openArticleURL");

  articleTrigger.uninit();

  articleTrigger.init(() => {}, [], ["example.com"]);

  is(
    articleTrigger._matchPatternSet.matches("http://example.com"),
    false,
    "Should fail, bad pattern"
  );

  articleTrigger.init(() => {}, [], ["*://*.example.com/"]);

  is(
    articleTrigger._matchPatternSet.matches("http://www.example.com"),
    true,
    "Should work, updated pattern"
  );

  articleTrigger.uninit();
});

add_task(async function check_openArticleURL() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";
  const articleTrigger = ASRouterTriggerListeners.get("openArticleURL");

  // Previously initialized by the Router
  articleTrigger.uninit();

  // Initialize the trigger with a new triggerHandler that resolves a promise
  // with the URL match
  const listenerTriggered = new Promise(resolve =>
    articleTrigger.init((browser, match) => resolve(match), ["example.com"])
  );

  const win = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(win, TEST_URL);
  // Send a message from the content page (the TEST_URL) to the parent
  // This should trigger the `receiveMessage` cb in the articleTrigger
  await ContentTask.spawn(win.gBrowser.selectedBrowser, null, async () => {
    let readerActor = content.windowGlobalChild.getActor("AboutReader");
    readerActor.sendAsyncMessage("Reader:UpdateReaderButton", {
      isArticle: true,
    });
  });

  await listenerTriggered.then(data =>
    is(
      data.param.url,
      TEST_URL,
      "We should match on the TEST_URL as a website article"
    )
  );

  // Cleanup
  articleTrigger.uninit();
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function check_openURL_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let urlVisitCount = 0;
  const triggerHandler = () => urlVisitCount++;
  const openURLListener = ASRouterTriggerListeners.get("openURL");

  // Previously initialized by the Router
  openURLListener.uninit();

  const normalWindow = await BrowserTestUtils.openNewBrowserWindow();
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Initialise listener
  openURLListener.init(triggerHandler, ["example.com"]);

  await openURLInWindow(normalWindow, TEST_URL);
  await BrowserTestUtils.waitForCondition(
    () => urlVisitCount !== 0,
    "Wait for the location change listener to run"
  );
  is(urlVisitCount, 1, "should receive page visits from existing windows");

  await openURLInWindow(normalWindow, "http://www.example.com/abc");
  is(urlVisitCount, 1, "should not receive page visits for different domains");

  await openURLInWindow(privateWindow, TEST_URL);
  is(
    urlVisitCount,
    1,
    "should not receive page visits from existing private windows"
  );

  const secondNormalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(secondNormalWindow, TEST_URL);
  await BrowserTestUtils.waitForCondition(
    () => urlVisitCount === 2,
    "Wait for the location change listener to run"
  );
  is(urlVisitCount, 2, "should receive page visits from newly opened windows");

  const secondPrivateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await openURLInWindow(secondPrivateWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should not receive page visits from newly opened private windows"
  );

  // Uninitialise listener
  openURLListener.uninit();

  await openURLInWindow(normalWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should now not receive page visits from existing windows"
  );

  const thirdNormalWindow = await BrowserTestUtils.openNewBrowserWindow();
  await openURLInWindow(thirdNormalWindow, TEST_URL);
  is(
    urlVisitCount,
    2,
    "should now not receive page visits from newly opened windows"
  );

  // Cleanup
  const windows = [
    normalWindow,
    privateWindow,
    secondNormalWindow,
    secondPrivateWindow,
    thirdNormalWindow,
  ];
  await Promise.all(windows.map(win => BrowserTestUtils.closeWindow(win)));
});

add_task(async function check_newSavedLogin_save_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let triggerTypesHandled = {
    save: 0,
    update: 0,
  };
  const triggerHandler = (sub, { id, context }) => {
    is(id, "newSavedLogin", "Check trigger id");
    triggerTypesHandled[context.type]++;
  };
  const newSavedLoginListener = ASRouterTriggerListeners.get("newSavedLogin");

  // Previously initialized by the Router
  newSavedLoginListener.uninit();

  // Initialise listener
  await newSavedLoginListener.init(triggerHandler);

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerNewSavedPassword(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
      await BrowserTestUtils.waitForCondition(
        () => triggerTypesHandled.save !== 0,
        "Wait for the observer notification to run"
      );
      is(triggerTypesHandled.save, 1, "should receive observer notification");
    }
  );

  is(triggerTypesHandled.update, 0, "shouldn't have handled other trigger");

  // Uninitialise listener
  newSavedLoginListener.uninit();

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerNewSavedPasswordAfterUninit(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:NewSavedPassword");
      await new Promise(resolve => executeSoon(resolve));
      is(
        triggerTypesHandled.save,
        1,
        "shouldn't receive obs. notification after uninit"
      );
    }
  );
});

add_task(async function check_newSavedLogin_update_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let triggerTypesHandled = {
    save: 0,
    update: 0,
  };
  const triggerHandler = (sub, { id, context }) => {
    is(id, "newSavedLogin", "Check trigger id");
    triggerTypesHandled[context.type]++;
  };
  const newSavedLoginListener = ASRouterTriggerListeners.get("newSavedLogin");

  // Previously initialized by the Router
  newSavedLoginListener.uninit();

  // Initialise listener
  await newSavedLoginListener.init(triggerHandler);

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerLoginUpdateSaved(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:LoginUpdateSaved");
      await BrowserTestUtils.waitForCondition(
        () => triggerTypesHandled.update !== 0,
        "Wait for the observer notification to run"
      );
      is(triggerTypesHandled.update, 1, "should receive observer notification");
    }
  );

  is(triggerTypesHandled.save, 0, "shouldn't have handled other trigger");

  // Uninitialise listener
  newSavedLoginListener.uninit();

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggerLoginUpdateSavedAfterUninit(browser) {
      Services.obs.notifyObservers(browser, "LoginStats:LoginUpdateSaved");
      await new Promise(resolve => executeSoon(resolve));
      is(
        triggerTypesHandled.update,
        1,
        "shouldn't receive obs. notification after uninit"
      );
    }
  );
});

add_task(async function check_contentBlocking_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  const event1 = 0x0001;
  const event2 = 0x0010;
  const event3 = 0x0100;
  const event4 = 0x1000;

  // Initialise listener to listen 2 events, for any incoming event e,
  // it will be triggered if and only if:
  // 1. (e & event1) && (e & event2)
  // 2. (e & event3)
  const bindEvents = [event1 | event2, event3];

  let observerEvent = 0;
  let pageLoadSum = 0;
  const triggerHandler = (target, trigger) => {
    const {
      id,
      param: { host, type },
      context: { pageLoad },
    } = trigger;
    is(id, "contentBlocking", "should match event name");
    is(host, TEST_URL, "should match test URL");
    is(
      bindEvents.filter(e => (type & e) === e).length,
      1,
      `event ${type} is valid`
    );
    ok(pageLoadSum <= pageLoad, "pageLoad is non-decreasing");

    observerEvent += 1;
    pageLoadSum = pageLoad;
  };
  const contentBlockingListener =
    ASRouterTriggerListeners.get("contentBlocking");

  // Previously initialized by the Router
  contentBlockingListener.uninit();

  await contentBlockingListener.init(triggerHandler, bindEvents);

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlocking(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            host: TEST_URL,
            event: event1, // won't trigger
          },
        },
        "SiteProtection:ContentBlockingEvent"
      );
    }
  );

  is(observerEvent, 0, "shouldn't receive unrelated observer notification");
  is(pageLoadSum, 0, "shouldn't receive unrelated observer notification");

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlocking(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            host: TEST_URL,
            event: event3, // will trigger
          },
        },
        "SiteProtection:ContentBlockingEvent"
      );

      await BrowserTestUtils.waitForCondition(
        () => observerEvent !== 0,
        "Wait for the observer notification to run"
      );
      is(observerEvent, 1, "should receive observer notification");
      is(pageLoadSum, 2, "should receive observer notification");

      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            host: TEST_URL,
            event: event1 | event2 | event4, // still trigger
          },
        },
        "SiteProtection:ContentBlockingEvent"
      );

      await BrowserTestUtils.waitForCondition(
        () => observerEvent !== 1,
        "Wait for the observer notification to run"
      );
      is(observerEvent, 2, "should receive another observer notification");
      is(pageLoadSum, 2, "should receive another observer notification");

      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            host: TEST_URL,
            event: event1, // no trigger
          },
        },
        "SiteProtection:ContentBlockingEvent"
      );

      await new Promise(resolve => executeSoon(resolve));
      is(observerEvent, 2, "shouldn't receive unrelated notification");
      is(pageLoadSum, 2, "shouldn't receive unrelated notification");
    }
  );

  // Uninitialise listener
  contentBlockingListener.uninit();

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlockingAfterUninit(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            host: TEST_URL,
            event: event3, // wont trigger after uninit
          },
        },
        "SiteProtection:ContentBlockingEvent"
      );
      await new Promise(resolve => executeSoon(resolve));
      is(observerEvent, 2, "shouldn't receive obs. notification after uninit");
      is(pageLoadSum, 2, "shouldn't receive obs. notification after uninit");
    }
  );
});

add_task(async function check_contentBlockingMilestone_listener() {
  const TEST_URL =
    "https://example.com/browser/browser/components/newtab/test/browser/red_page.html";

  let observerEvent = 0;
  const triggerHandler = (target, trigger) => {
    const {
      id,
      param: { type },
    } = trigger;
    is(id, "contentBlocking", "should match event name");
    is(type, "ContentBlockingMilestone", "Should be the correct event type");
    observerEvent += 1;
  };
  const contentBlockingListener =
    ASRouterTriggerListeners.get("contentBlocking");

  // Previously initialized by the Router
  contentBlockingListener.uninit();

  // Initialise listener
  contentBlockingListener.init(triggerHandler, ["ContentBlockingMilestone"]);

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlocking(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            event: "Other Event",
          },
        },
        "SiteProtection:ContentBlockingMilestone"
      );
    }
  );

  is(observerEvent, 0, "shouldn't receive unrelated observer notification");

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlocking(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            event: "ContentBlockingMilestone",
          },
        },
        "SiteProtection:ContentBlockingMilestone"
      );

      await BrowserTestUtils.waitForCondition(
        () => observerEvent !== 0,
        "Wait for the observer notification to run"
      );
      is(observerEvent, 1, "should receive observer notification");
    }
  );

  // Uninitialise listener
  contentBlockingListener.uninit();

  await BrowserTestUtils.withNewTab(
    TEST_URL,
    async function triggercontentBlockingAfterUninit(browser) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: {
            browser,
            event: "ContentBlockingMilestone",
          },
        },
        "SiteProtection:ContentBlockingMilestone"
      );
      await new Promise(resolve => executeSoon(resolve));
      is(observerEvent, 1, "shouldn't receive obs. notification after uninit");
    }
  );
});

add_task(function test_pattern_match() {
  const openURLListener = ASRouterTriggerListeners.get("openURL");
  openURLListener.uninit();
  openURLListener.init(() => {}, [], ["*://*/*.pdf"]);
  let pattern = openURLListener._matchPatternSet;

  Assert.ok(pattern.matches("https://example.com/foo.pdf"), "match 1");
  Assert.ok(pattern.matches("https://example.com/bar/foo.pdf"), "match 2");
  Assert.ok(pattern.matches("https://www.example.com/foo.pdf"), "match 3");
  // Shouldn't match. Too generic.
  Assert.ok(!pattern.matches("https://www.example.com/foo"), "match 4");
  Assert.ok(!pattern.matches("https://www.example.com/pdf"), "match 5");
});
