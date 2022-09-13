"use strict";

// This test checks whether browser.theme.getCurrent() works correctly when theme
// does not originate from extension querying the theme.
const THEME = {
  manifest: {
    theme: {
      images: {
        theme_frame: "image1.png",
      },
      colors: {
        frame: ACCENT_COLOR,
        tab_background_text: TEXT_COLOR,
      },
    },
  },
  files: {
    "image1.png": BACKGROUND,
  },
};

add_task(async function test_getcurrent() {
  const theme = ExtensionTestUtils.loadExtension(THEME);
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.theme.onUpdated.addListener(() => {
        browser.theme.getCurrent().then(theme => {
          browser.test.sendMessage("theme-updated", theme);
          if (!theme?.images) {
            return;
          }

          // Try to access the theme_frame image
          fetch(theme.images.theme_frame)
            .then(() => {
              browser.test.sendMessage("theme-image", { success: true });
            })
            .catch(e => {
              browser.test.sendMessage("theme-image", {
                success: false,
                error: e.message,
              });
            });
        });
      });
    },
  });

  await extension.startup();

  info("Testing getCurrent after static theme startup");
  let updatedPromise = extension.awaitMessage("theme-updated");
  let imageLoaded = extension.awaitMessage("theme-image");
  await theme.startup();
  let receivedTheme = await updatedPromise;
  Assert.ok(
    receivedTheme.images.theme_frame.includes("image1.png"),
    "getCurrent returns correct theme_frame image"
  );
  Assert.equal(
    receivedTheme.colors.frame,
    ACCENT_COLOR,
    "getCurrent returns correct frame color"
  );
  Assert.equal(
    receivedTheme.colors.tab_background_text,
    TEXT_COLOR,
    "getCurrent returns correct tab_background_text color"
  );
  Assert.deepEqual(await imageLoaded, { success: true }, "theme image loaded");

  info("Testing getCurrent after static theme unload");
  updatedPromise = extension.awaitMessage("theme-updated");
  await theme.unload();
  receivedTheme = await updatedPromise;
  Assert.equal(
    JSON.stringify({ colors: null, images: null, properties: null }),
    JSON.stringify(receivedTheme),
    "getCurrent returns empty theme"
  );

  await extension.unload();
});

add_task(async function test_getcurrent_privateBrowsing() {
  const theme = ExtensionTestUtils.loadExtension(THEME);

  const extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      sidebar_action: {
        default_panel: "sidebar.html",
      },
    },
    files: {
      "sidebar.html": `<!DOCTYPE html>
        <html>
          <body>
            Test Extension Sidebar
            <script src="sidebar.js"></script>
          </body>
        </html>
      `,
      "sidebar.js": function() {
        browser.theme.getCurrent().then(theme => {
          if (!theme?.images) {
            browser.test.fail(
              `Missing expected images from theme.getCurrent result`
            );
            return;
          }

          // Try to access the theme_frame image
          fetch(theme.images.theme_frame)
            .then(() => {
              browser.test.sendMessage("theme-image", { success: true });
            })
            .catch(e => {
              browser.test.sendMessage("theme-image", {
                success: false,
                error: e.message,
              });
            });
        });
      },
    },
  });

  await extension.startup();
  await theme.startup();

  const privateWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  const { ExtensionCommon } = ChromeUtils.import(
    "resource://gre/modules/ExtensionCommon.jsm"
  );
  const { makeWidgetId } = ExtensionCommon;
  privateWin.SidebarUI.show(`${makeWidgetId(extension.id)}-sidebar-action`);

  let imageLoaded = extension.awaitMessage("theme-image");
  Assert.deepEqual(await imageLoaded, { success: true }, "theme image loaded");

  await extension.unload();
  await theme.unload();

  await BrowserTestUtils.closeWindow(privateWin);
});
