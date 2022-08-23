"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

const BASE_URL = `http://localhost:${server.identity.primaryPort}/data`;

var originalReqLocales = Services.locale.requestedLocales;

registerCleanupFunction(() => {
  Preferences.reset("intl.accept_languages");
  Services.locale.requestedLocales = originalReqLocales;
});

add_task(async function test_i18n() {
  function runTests(assertEq) {
    let _ = browser.i18n.getMessage.bind(browser.i18n);

    let url = browser.runtime.getURL("/");
    assertEq(
      url,
      `moz-extension://${_("@@extension_id")}/`,
      "@@extension_id builtin message"
    );

    assertEq("Foo.", _("Foo"), "Simple message in selected locale.");

    assertEq("(bar)", _("bar"), "Simple message fallback in default locale.");

    assertEq("", _("some-unknown-locale-string"), "Unknown locale string.");

    assertEq("", _("@@unknown_builtin_string"), "Unknown built-in string.");
    assertEq(
      "",
      _("@@bidi_unknown_builtin_string"),
      "Unknown built-in bidi string."
    );

    assertEq("Føo.", _("Föo"), "Multi-byte message in selected locale.");

    let substitutions = [];
    substitutions[4] = "5";
    substitutions[13] = "14";

    assertEq(
      "'$0' '14' '' '5' '$$$$' '$'.",
      _("basic_substitutions", substitutions),
      "Basic numeric substitutions"
    );

    assertEq(
      "'$0' '' 'just a string' '' '$$$$' '$'.",
      _("basic_substitutions", "just a string"),
      "Basic numeric substitutions, with non-array value"
    );

    let values = _("named_placeholder_substitutions", [
      "(subst $1 $2)",
      "(2 $1 $2)",
    ]).split("\n");

    assertEq(
      "_foo_ (subst $1 $2) _bar_",
      values[0],
      "Named and numeric substitution"
    );

    assertEq(
      "(2 $1 $2)",
      values[1],
      "Numeric substitution amid named placeholders"
    );

    assertEq("$bad name$", values[2], "Named placeholder with invalid key");

    assertEq("", values[3], "Named placeholder with an invalid value");

    assertEq(
      "Accepted, but shouldn't break.",
      values[4],
      "Named placeholder with a strange content value"
    );

    assertEq("$foo", values[5], "Non-placeholder token that should be ignored");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      default_locale: "jp",

      content_scripts: [
        { matches: ["http://*/*/file_sample.html"], js: ["content.js"] },
      ],
    },

    files: {
      "_locales/en_US/messages.json": {
        foo: {
          message: "Foo.",
          description: "foo",
        },

        föo: {
          message: "Føo.",
          description: "foo",
        },

        basic_substitutions: {
          message: "'$0' '$14' '$1' '$5' '$$$$$' '$$'.",
          description: "foo",
        },

        Named_placeholder_substitutions: {
          message:
            "$Foo$\n$2\n$bad name$\n$bad_value$\n$bad_content_value$\n$foo",
          description: "foo",
          placeholders: {
            foO: {
              content: "_foo_ $1 _bar_",
              description: "foo",
            },

            "bad name": {
              content: "Nope.",
              description: "bad name",
            },

            bad_value: "Nope.",

            bad_content_value: {
              content: ["Accepted, but shouldn't break."],
              description: "bad value",
            },
          },
        },

        broken_placeholders: {
          message: "$broken$",
          description: "broken placeholders",
          placeholders: "foo.",
        },
      },

      "_locales/jp/messages.json": {
        foo: {
          message: "(foo)",
          description: "foo",
        },

        bar: {
          message: "(bar)",
          description: "bar",
        },
      },

      "content.js":
        "new " +
        function(runTestsFn) {
          runTestsFn((...args) => {
            browser.runtime.sendMessage(["assertEq", ...args]);
          });

          browser.runtime.sendMessage(["content-script-finished"]);
        } +
        `(${runTests})`,
    },

    background:
      "new " +
      function(runTestsFn) {
        browser.runtime.onMessage.addListener(([msg, ...args]) => {
          if (msg == "assertEq") {
            browser.test.assertEq(...args);
          } else {
            browser.test.sendMessage(msg, ...args);
          }
        });

        runTestsFn(browser.test.assertEq.bind(browser.test));
      } +
      `(${runTests})`,
  });

  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );
  await extension.awaitMessage("content-script-finished");
  await contentPage.close();

  await extension.unload();
});

add_task(async function test_i18n_negotiation() {
  function runTests(expected) {
    let _ = browser.i18n.getMessage.bind(browser.i18n);

    browser.test.assertEq(expected, _("foo"), "Got expected message");
  }

  let extensionData = {
    manifest: {
      default_locale: "en_US",

      content_scripts: [
        { matches: ["http://*/*/file_sample.html"], js: ["content.js"] },
      ],
    },

    files: {
      "_locales/en_US/messages.json": {
        foo: {
          message: "English.",
          description: "foo",
        },
      },

      "_locales/jp/messages.json": {
        foo: {
          message: "\u65e5\u672c\u8a9e",
          description: "foo",
        },
      },

      "content.js":
        "new " +
        function(runTestsFn) {
          browser.test.onMessage.addListener(expected => {
            runTestsFn(expected);

            browser.test.sendMessage("content-script-finished");
          });
          browser.test.sendMessage("content-ready");
        } +
        `(${runTests})`,
    },

    background:
      "new " +
      function(runTestsFn) {
        browser.test.onMessage.addListener(expected => {
          runTestsFn(expected);

          browser.test.sendMessage("background-script-finished");
        });
      } +
      `(${runTests})`,
  };

  // At the moment extension language negotiation is tied to Firefox language
  // negotiation result. That means that to test an extension in `fr`, we need
  // to mock `fr` being available in Firefox and then request it.
  //
  // In the future, we should provide some way for tests to decouple their
  // language selection from that of Firefox.
  Services.locale.availableLocales = ["en-US", "fr", "jp"];

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  for (let [lang, msg] of [
    ["en-US", "English."],
    ["jp", "\u65e5\u672c\u8a9e"],
  ]) {
    Services.locale.requestedLocales = [lang];

    let extension = ExtensionTestUtils.loadExtension(extensionData);
    await extension.startup();
    await extension.awaitMessage("content-ready");

    extension.sendMessage(msg);
    await extension.awaitMessage("background-script-finished");
    await extension.awaitMessage("content-script-finished");

    await extension.unload();
  }
  Services.locale.requestedLocales = originalReqLocales;

  await contentPage.close();
});

add_task(async function test_get_accept_languages() {
  function checkResults(source, results, expected) {
    browser.test.assertEq(
      expected.length,
      results.length,
      `got expected number of languages in ${source}`
    );
    results.forEach((lang, index) => {
      browser.test.assertEq(
        expected[index],
        lang,
        `got expected language in ${source}`
      );
    });
  }

  function background(checkResultsFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      browser.i18n.getAcceptLanguages().then(results => {
        checkResultsFn("background", results, expected);

        browser.test.sendMessage("background-done");
      });
    });
  }

  function content(checkResultsFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      browser.i18n.getAcceptLanguages().then(results => {
        checkResultsFn("contentScript", results, expected);

        browser.test.sendMessage("content-done");
      });
    });
    browser.test.sendMessage("content-loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          run_at: "document_start",
          js: ["content_script.js"],
        },
      ],
    },

    background: `(${background})(${checkResults})`,

    files: {
      "content_script.js": `(${content})(${checkResults})`,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.startup();
  await extension.awaitMessage("content-loaded");

  // TODO bug 1765375: ", en" is missing on Android.
  // TODO bug 1785807: "en-us" should be "en-US" on Android
  let expectedLangs =
    AppConstants.platform == "android" ? ["en-us"] : ["en-US", "en"];
  extension.sendMessage(["expect-results", expectedLangs]);
  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");

  expectedLangs = ["en-US", "en", "fr-CA", "fr"];
  Preferences.set("intl.accept_languages", expectedLangs.toString());
  extension.sendMessage(["expect-results", expectedLangs]);
  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");
  Preferences.reset("intl.accept_languages");

  await contentPage.close();

  await extension.unload();
});

add_task(async function test_get_ui_language() {
  function getResults() {
    return {
      getUILanguage: browser.i18n.getUILanguage(),
      getMessage: browser.i18n.getMessage("@@ui_locale"),
    };
  }

  function checkResults(source, results, expected) {
    browser.test.assertEq(
      expected,
      results.getUILanguage,
      `Got expected getUILanguage result in ${source}`
    );
    browser.test.assertEq(
      expected,
      results.getMessage,
      `Got expected getMessage result in ${source}`
    );
  }

  function background(getResultsFn, checkResultsFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      checkResultsFn("background", getResultsFn(), expected);

      browser.test.sendMessage("background-done");
    });
  }

  function content(getResultsFn, checkResultsFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      checkResultsFn("contentScript", getResultsFn(), expected);

      browser.test.sendMessage("content-done");
    });
    browser.test.sendMessage("content-loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          run_at: "document_start",
          js: ["content_script.js"],
        },
      ],
    },

    background: `(${background})(${getResults}, ${checkResults})`,

    files: {
      "content_script.js": `(${content})(${getResults}, ${checkResults})`,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.startup();
  await extension.awaitMessage("content-loaded");

  extension.sendMessage(["expect-results", "en-US"]);

  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");

  // We don't currently have a good way to mock this.
  if (false) {
    Services.locale.requestedLocales = ["he"];

    extension.sendMessage(["expect-results", "he"]);

    await extension.awaitMessage("background-done");
    await extension.awaitMessage("content-done");
  }

  await contentPage.close();

  await extension.unload();
});

add_task(async function test_detect_language() {
  const af_string =
    " aam skukuza die naam beteken hy wat skoonvee of hy wat alles onderstebo keer wysig " +
    "bosveldkampe boskampe is kleiner afgeleë ruskampe wat oor min fasiliteite beskik daar is geen restaurante " +
    "of winkels nie en slegs oornagbesoekers word toegelaat bateleur";
  // String with intermixed French/English text
  const fr_en_string =
    "France is the largest country in Western Europe and the third-largest in Europe as a whole. " +
    "A accès aux chiens et aux frontaux qui lui ont été il peut consulter et modifier ses collections et exporter " +
    "Cet article concerne le pays européen aujourd’hui appelé République française. Pour d’autres usages du nom France, " +
    "Pour une aide rapide et effective, veuiller trouver votre aide dans le menu ci-dessus." +
    "Motoring events began soon after the construction of the first successful gasoline-fueled automobiles. The quick brown fox jumped over the lazy dog";

  function checkResult(source, result, expected) {
    browser.test.assertEq(
      expected.isReliable,
      result.isReliable,
      "result.confident is true"
    );
    browser.test.assertEq(
      expected.languages.length,
      result.languages.length,
      `result.languages contains the expected number of languages in ${source}`
    );
    expected.languages.forEach((lang, index) => {
      browser.test.assertEq(
        lang.percentage,
        result.languages[index].percentage,
        `element ${index} of result.languages array has the expected percentage in ${source}`
      );
      browser.test.assertEq(
        lang.language,
        result.languages[index].language,
        `element ${index} of result.languages array has the expected language in ${source}`
      );
    });
  }

  function backgroundScript(checkResultFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      browser.i18n.detectLanguage(msg).then(result => {
        checkResultFn("background", result, expected);
        browser.test.sendMessage("background-done");
      });
    });
  }

  function content(checkResultFn) {
    browser.test.onMessage.addListener(([msg, expected]) => {
      browser.i18n.detectLanguage(msg).then(result => {
        checkResultFn("contentScript", result, expected);
        browser.test.sendMessage("content-done");
      });
    });
    browser.test.sendMessage("content-loaded");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://*/*/file_sample.html"],
          run_at: "document_start",
          js: ["content_script.js"],
        },
      ],
    },

    background: `(${backgroundScript})(${checkResult})`,

    files: {
      "content_script.js": `(${content})(${checkResult})`,
    },
  });

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `${BASE_URL}/file_sample.html`
  );

  await extension.startup();
  await extension.awaitMessage("content-loaded");

  let expected = {
    isReliable: true,
    languages: [
      {
        language: "fr",
        percentage: 67,
      },
      {
        language: "en",
        percentage: 32,
      },
    ],
  };
  extension.sendMessage([fr_en_string, expected]);
  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");

  expected = {
    isReliable: true,
    languages: [
      {
        language: "af",
        percentage: 99,
      },
    ],
  };
  extension.sendMessage([af_string, expected]);
  await extension.awaitMessage("background-done");
  await extension.awaitMessage("content-done");

  await contentPage.close();

  await extension.unload();
});
