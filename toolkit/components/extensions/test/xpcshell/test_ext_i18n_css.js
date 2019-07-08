"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// Some multibyte characters. This sample was taken from the encoding/api-basics.html web platform test.
const MULTIBYTE_STRING = "z\xA2\u6C34\uD834\uDD1E\uF8FF\uDBFF\uDFFD\uFFFE";
let getCSS = (a, b) => `a { content: '${a}'; } b { content: '${b}'; }`;

let extensionData = {
  background: function() {
    function backgroundFetch(url) {
      return new Promise((resolve, reject) => {
        let xhr = new XMLHttpRequest();
        xhr.overrideMimeType("text/plain");
        xhr.open("GET", url);
        xhr.onload = () => {
          resolve(xhr.responseText);
        };
        xhr.onerror = reject;
        xhr.send();
      });
    }

    Promise.all([
      backgroundFetch("foo.css"),
      backgroundFetch("bar.CsS?x#y"),
      backgroundFetch("foo.txt"),
    ]).then(results => {
      browser.test.assertEq(
        "body { max-width: 42px; }",
        results[0],
        "CSS file localized"
      );
      browser.test.assertEq(
        "body { max-width: 42px; }",
        results[1],
        "CSS file localized"
      );

      browser.test.assertEq(
        "body { __MSG_foo__; }",
        results[2],
        "Text file not localized"
      );

      browser.test.notifyPass("i18n-css");
    });

    browser.test.sendMessage("ready", browser.runtime.getURL("/"));
  },

  manifest: {
    applications: {
      gecko: {
        id: "i18n_css@mochi.test",
      },
    },

    web_accessible_resources: [
      "foo.css",
      "foo.txt",
      "locale.css",
      "multibyte.css",
    ],

    content_scripts: [
      {
        matches: ["http://*/*/file_sample.html"],
        css: ["foo.css"],
        run_at: "document_start",
      },
      {
        matches: ["http://*/*/file_sample.html"],
        js: ["content.js"],
      },
    ],

    default_locale: "en",
  },

  files: {
    "_locales/en/messages.json": JSON.stringify({
      foo: {
        message: "max-width: 42px",
        description: "foo",
      },
      multibyteKey: {
        message: MULTIBYTE_STRING,
      },
    }),

    "content.js": function() {
      let style = getComputedStyle(document.body);
      browser.test.sendMessage("content-maxWidth", style.maxWidth);
    },

    "foo.css": "body { __MSG_foo__; }",
    "bar.CsS": "body { __MSG_foo__; }",
    "foo.txt": "body { __MSG_foo__; }",
    "locale.css":
      '* { content: "__MSG_@@ui_locale__ __MSG_@@bidi_dir__ __MSG_@@bidi_reversed_dir__ __MSG_@@bidi_start_edge__ __MSG_@@bidi_end_edge__" }',
    "multibyte.css": getCSS("__MSG_multibyteKey__", MULTIBYTE_STRING),
  },
};

async function test_i18n_css(options = {}) {
  extensionData.useAddonManager = options.useAddonManager;
  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  let baseURL = await extension.awaitMessage("ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  let css = await contentPage.fetch(baseURL + "foo.css");

  equal(
    css,
    "body { max-width: 42px; }",
    "CSS file localized in mochitest scope"
  );

  let maxWidth = await extension.awaitMessage("content-maxWidth");

  equal(maxWidth, "42px", "stylesheet correctly applied");

  css = await contentPage.fetch(baseURL + "locale.css");
  equal(
    css,
    '* { content: "en-US ltr rtl left right" }',
    "CSS file localized in mochitest scope"
  );

  css = await contentPage.fetch(baseURL + "multibyte.css");
  equal(
    css,
    getCSS(MULTIBYTE_STRING, MULTIBYTE_STRING),
    "CSS file contains multibyte string"
  );

  await contentPage.close();

  // We don't currently have a good way to mock this.
  if (false) {
    const DIR = "intl.uidirection";

    // We don't wind up actually switching the chrome registry locale, since we
    // don't have a chrome package for Hebrew. So just override it, and force
    // RTL directionality.
    const origReqLocales = Services.locale.requestedLocales;
    Services.locale.requestedLocales = ["he"];
    Preferences.set(DIR, 1);

    css = await fetch(baseURL + "locale.css");
    equal(
      css,
      '* { content: "he rtl ltr right left" }',
      "CSS file localized in mochitest scope"
    );

    Services.locale.requestedLocales = origReqLocales;
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
  await test_i18n_css({ useAddonManager: "temporary" });
});
add_task(async function startup() {
  await promiseShutdownManager();
});
