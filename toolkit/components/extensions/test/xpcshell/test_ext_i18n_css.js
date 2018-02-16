"use strict";

ChromeUtils.import("resource://gre/modules/Preferences.jsm");


const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

Cu.importGlobalProperties(["XMLHttpRequest"]);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

let extensionData = {
  background: function() {
    function backgroundFetch(url) {
      return new Promise((resolve, reject) => {
        let xhr = new XMLHttpRequest();
        xhr.overrideMimeType("text/plain");
        xhr.open("GET", url);
        xhr.onload = () => { resolve(xhr.responseText); };
        xhr.onerror = reject;
        xhr.send();
      });
    }

    Promise.all([backgroundFetch("foo.css"), backgroundFetch("bar.CsS?x#y"), backgroundFetch("foo.txt")]).then(results => {
      browser.test.assertEq("body { max-width: 42px; }", results[0], "CSS file localized");
      browser.test.assertEq("body { max-width: 42px; }", results[1], "CSS file localized");

      browser.test.assertEq("body { __MSG_foo__; }", results[2], "Text file not localized");

      browser.test.notifyPass("i18n-css");
    });

    browser.test.sendMessage("ready", browser.runtime.getURL("foo.css"));
  },

  manifest: {
    "applications": {
      "gecko": {
        "id": "i18n_css@mochi.test",
      },
    },

    "web_accessible_resources": ["foo.css", "foo.txt", "locale.css"],

    "content_scripts": [{
      "matches": ["http://*/*/file_sample.html"],
      "css": ["foo.css"],
    }],

    "default_locale": "en",
  },

  files: {
    "_locales/en/messages.json": JSON.stringify({
      "foo": {
        "message": "max-width: 42px",
        "description": "foo",
      },
    }),

    "foo.css": "body { __MSG_foo__; }",
    "bar.CsS": "body { __MSG_foo__; }",
    "foo.txt": "body { __MSG_foo__; }",
    "locale.css": '* { content: "__MSG_@@ui_locale__ __MSG_@@bidi_dir__ __MSG_@@bidi_reversed_dir__ __MSG_@@bidi_start_edge__ __MSG_@@bidi_end_edge__" }',
  },
};

async function test_i18n_css(options = {}) {
  extensionData.useAddonManager = options.useAddonManager;
  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  let cssURL = await extension.awaitMessage("ready");

  function fetch(url) {
    return new Promise((resolve, reject) => {
      let xhr = new XMLHttpRequest();
      xhr.overrideMimeType("text/plain");
      xhr.open("GET", url);
      xhr.onload = () => { resolve(xhr.responseText); };
      xhr.onerror = reject;
      xhr.send();
    });
  }

  let css = await fetch(cssURL);

  equal(css, "body { max-width: 42px; }", "CSS file localized in mochitest scope");

  let contentPage = await ExtensionTestUtils.loadContentPage(`${BASE_URL}/file_sample.html`);

  // workaround for extension may not be ready for applying foo.css
  await new Promise(executeSoon);

  let maxWidth = await ContentTask.spawn(contentPage.browser, {}, async function() {
    /* globals content */
    let style = content.getComputedStyle(content.document.body);

    return style.maxWidth;
  });

  equal(maxWidth, "42px", "stylesheet correctly applied");

  await contentPage.close();

  cssURL = cssURL.replace(/foo.css$/, "locale.css");

  css = await fetch(cssURL);
  equal(css, '* { content: "en-US ltr rtl left right" }', "CSS file localized in mochitest scope");

  // We don't currently have a good way to mock this.
  if (false) {
    const DIR = "intl.uidirection";

    // We don't wind up actually switching the chrome registry locale, since we
    // don't have a chrome package for Hebrew. So just override it, and force
    // RTL directionality.
    const origReqLocales = Services.locale.getRequestedLocales();
    Services.locale.setRequestedLocales(["he"]);
    Preferences.set(DIR, 1);

    css = await fetch(cssURL);
    equal(css, '* { content: "he rtl ltr right left" }', "CSS file localized in mochitest scope");

    Services.locale.setRequestedLocales(origReqLocales);
    Preferences.reset(DIR);
  }

  await extension.awaitFinish("i18n-css");
  await extension.unload();
}

add_task(async function startup() {
  await promiseStartupManager();
});
add_task(test_i18n_css);
add_task(async function test_i18n_css_xpi() {
  await test_i18n_css({useAddonManager: "temporary"});
});
add_task(async function startup() {
  await promiseShutdownManager();
});
