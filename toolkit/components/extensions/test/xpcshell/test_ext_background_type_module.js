/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function assertBackgroundScriptTypes(
  extensionTestWrapper,
  expectedScriptTypesMap
) {
  const { baseURI } = extensionTestWrapper.extension;
  let expectedMapWithResolvedURLs = Object.keys(expectedScriptTypesMap).reduce(
    (result, scriptPath) => {
      result[baseURI.resolve(scriptPath)] = expectedScriptTypesMap[scriptPath];
      return result;
    },
    {}
  );
  const page = await ExtensionTestUtils.loadContentPage(
    baseURI.resolve("_generated_background_page.html")
  );
  const scriptTypesMap = await page.spawn([], () => {
    const scripts = Array.from(
      this.content.document.querySelectorAll("script")
    );
    return scripts.reduce((result, script) => {
      result[script.getAttribute("src")] = script.getAttribute("type");
      return result;
    }, {});
  });
  await page.close();
  Assert.deepEqual(
    scriptTypesMap,
    expectedMapWithResolvedURLs,
    "Got the expected script type from the generated background page"
  );
}

async function testBackgroundScriptClassic({ manifestTypeClassicSet }) {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["anotherScript.js", "main.js"],
        type: manifestTypeClassicSet ? "classic" : undefined,
      },
    },
    files: {
      "main.js": ``,
      "anotherScript.js": ``,
    },
  });

  await extension.startup();
  await assertBackgroundScriptTypes(extension, {
    "main.js": "text/javascript",
    "anotherScript.js": "text/javascript",
  });
  await extension.unload();
}

add_task(async function test_background_scripts_type_default() {
  await testBackgroundScriptClassic({ manifestTypeClassicSet: false });
});

add_task(async function test_background_scripts_type_classic() {
  await testBackgroundScriptClassic({ manifestTypeClassicSet: true });
});

add_task(async function test_background_scripts_type_module() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["anotherModule.js", "mainModule.js"],
        type: "module",
      },
    },
    files: {
      "mainModule.js": `
        import { initBackground } from "/importedModule.js";
        browser.test.log("mainModule.js - ESM module executing");
        initBackground();
      `,
      "importedModule.js": `
        export function initBackground() {
          browser.test.onMessage.addListener((msg) => {
            browser.test.log("importedModule.js - test message received");
            browser.test.sendMessage("esm-module-reply", msg);
          });
          browser.test.log("importedModule.js - initBackground executed");
        }
        browser.test.log("importedModule.js - ESM module loaded");
      `,
      "anotherModule.js": `
        browser.test.log("anotherModule.js - ESM module loaded");
      `,
    },
  });

  await extension.startup();
  await extension.sendMessage("test-event-value");
  equal(
    await extension.awaitMessage("esm-module-reply"),
    "test-event-value",
    "Got the expected event from the ESM module loaded from the background script"
  );
  await assertBackgroundScriptTypes(extension, {
    "mainModule.js": "module",
    "anotherModule.js": "module",
  });
  await extension.unload();
});

add_task(async function test_background_scripts_type_invalid() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        scripts: ["anotherScript.js", "main.js"],
        type: "invalid",
      },
    },
    files: {
      "main.js": ``,
      "anotherScript.js": ``,
    },
  });

  ExtensionTestUtils.failOnSchemaWarnings(false);
  await Assert.rejects(
    extension.startup(),
    /Error processing background: .* \.type must be one of/,
    "Expected install to fail"
  );
  ExtensionTestUtils.failOnSchemaWarnings(true);
});
