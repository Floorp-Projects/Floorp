"use strict";

AddonTestUtils.init(this);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_setup(async function setup() {
  await ExtensionTestUtils.startAddonManager();
});

const CAN_CRASH_EXTENSIONS = WebExtensionPolicy.useRemoteWebExtensions;

add_setup(
  // Crash dumps are only generated when MOZ_CRASHREPORTER is set.
  // Crashes are only generated if tests can crash the extension process.
  { skip_if: () => !AppConstants.MOZ_CRASHREPORTER || !CAN_CRASH_EXTENSIONS },
  setup_crash_reporter_override_and_cleaner
);

add_task(async function test_storage_session() {
  await test_background_page_storage("session");
});

add_task(async function test_storage_session_onChanged_event_page() {
  await test_storage_change_event_page("session");
});

add_task(async function test_storage_session_persistance() {
  await test_storage_after_reload("session", { expectPersistency: false });
});

add_task(async function test_storage_session_empty_events() {
  await test_storage_empty_events("session");
});

add_task(async function test_storage_session_contentscript() {
  let extensionData = {
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/data/file_sample.html"],
          js: ["content_script.js"],
        },
      ],
      permissions: ["storage"],
    },
    background() {
      let events = [];
      browser.storage.onChanged.addListener((_, area) => {
        events.push(area);
      });
      browser.test.onMessage.addListener(_msg => {
        browser.test.sendMessage("bg-events", events.join());
      });
      browser.runtime.onMessage.addListener(async _msg => {
        await browser.storage.local.set({ foo: "local" });
        await browser.storage.session.set({ foo: "session" });
        await browser.storage.sync.set({ foo: "sync" });
        browser.test.sendMessage("done");
      });
    },
    files: {
      "content_script.js"() {
        let events = [];
        browser.storage.onChanged.addListener((_, area) => {
          events.push(area);
        });
        browser.test.onMessage.addListener(_msg => {
          browser.test.sendMessage("cs-events", events.join());
        });

        browser.test.assertEq(
          typeof browser.storage.session,
          "undefined",
          "Expect storage.session to not be available in content scripts"
        );
        browser.runtime.sendMessage("ready");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/data/file_sample.html"
  );

  await extension.awaitMessage("done");
  extension.sendMessage("_getEvents");

  equal(
    "local,sync",
    await extension.awaitMessage("cs-events"),
    "Content script doesn't see storage.onChanged events from the session area."
  );
  equal(
    "local,session,sync",
    await extension.awaitMessage("bg-events"),
    "Background receives onChanged events from all storage areas."
  );

  await extension.unload();
  await contentPage.close();
});

async function test_storage_session_after_crash({ persistent }) {
  async function background() {
    let before = await browser.storage.session.get();

    browser.storage.session.set({ count: (before.count ?? 0) + 1 });

    // Roundtrip the data through the parent process.
    let after = await browser.storage.session.get();

    browser.test.sendMessage("data", { before, after });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
      background: { persistent },
    },
    background,
  });

  await extension.startup();

  info(`Testing storage.session after crash with persistent=${persistent}`);

  {
    let { before, after } = await extension.awaitMessage("data");

    equal(JSON.stringify(before), "{}", "Initial before storage is empty.");
    equal(after.count, 1, "After storage counter is correct.");
  }

  info("Crashing the extension process.");
  await crashExtensionBackground(extension);
  await extension.wakeupBackground();

  {
    let { before, after } = await extension.awaitMessage("data");

    equal(before.count, 1, "Before storage counter is correct.");
    equal(after.count, 2, "After storage counter is correct.");
  }

  await extension.unload();
}

add_task(
  { skip_if: () => !CAN_CRASH_EXTENSIONS },
  function test_storage_session_after_crash_persistent() {
    return test_storage_session_after_crash({ persistent: true });
  }
);

add_task(
  { skip_if: () => !CAN_CRASH_EXTENSIONS },
  function test_storage_session_after_crash_event_page() {
    return test_storage_session_after_crash({ persistent: false });
  }
);
