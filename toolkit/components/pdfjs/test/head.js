async function waitForPdfJS(browser, url) {
  // Runs tests after all "load" event handlers have fired off
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "documentloaded",
    false,
    null,
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loadPromise;
}

async function waitForPdfJSAnnotationLayer(browser, url) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationlayerrendered",
    false,
    null,
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loadPromise;
}

async function waitForPdfJSAllLayers(browser, url, layers) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "textlayerrendered",
    false,
    null,
    true
  );
  let annotationPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationlayerrendered",
    false,
    null,
    true
  );
  let annotationEditorPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationeditorlayerrendered",
    false,
    null,
    true
  );

  BrowserTestUtils.startLoadingURIString(browser, url);
  await Promise.all([loadPromise, annotationPromise, annotationEditorPromise]);

  await SpecialPowers.spawn(browser, [layers], async function (layers) {
    const { ContentTaskUtils } = ChromeUtils.importESModule(
      "resource://testing-common/ContentTaskUtils.sys.mjs"
    );
    const { document } = content;

    for (let i = 0; i < layers.length; i++) {
      await ContentTaskUtils.waitForCondition(
        () =>
          layers[i].every(
            name =>
              !!document.querySelector(
                `.page[data-page-number='${i + 1}'] .${name}`
              )
          ),
        `All the layers must be displayed on page ${i}`
      );
    }
  });

  await TestUtils.waitForTick();
}

async function waitForPdfJSCanvas(browser, url) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pagerendered",
    false,
    null,
    true
  );
  BrowserTestUtils.startLoadingURIString(browser, url);
  return loadPromise;
}

async function waitForPdfJSSandbox(browser) {
  let loadPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "sandboxcreated",
    false,
    null,
    true
  );
  return loadPromise;
}

async function waitForSelector(browser, selector, message) {
  return SpecialPowers.spawn(
    browser,
    [selector, message],
    async function (sel, msg) {
      const { ContentTaskUtils } = ChromeUtils.importESModule(
        "resource://testing-common/ContentTaskUtils.sys.mjs"
      );
      const { document } = content;

      await ContentTaskUtils.waitForCondition(
        () => !!document.querySelector(sel),
        `${sel} must be displayed`
      );

      await ContentTaskUtils.waitForCondition(
        () => ContentTaskUtils.isVisible(document.querySelector(sel)),
        msg
      );
    }
  );
}

async function click(browser, selector) {
  await SpecialPowers.spawn(browser, [selector], async function (sel) {
    const el = content.document.querySelector(sel);
    await new Promise(r => {
      el.addEventListener("click", r, { once: true });
      el.click();
    });
  });
}

/**
 * Enable an editor (Ink, FreeText, ...).
 * @param {Object} browser
 * @param {string} name
 */
async function enableEditor(browser, name) {
  const editingModePromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationeditormodechanged",
    false,
    null,
    true
  );
  const editingStatePromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "annotationeditorstateschanged",
    false,
    null,
    true
  );
  await clickOn(browser, `#editor${name}`);
  await editingModePromise;
  await editingStatePromise;
  await TestUtils.waitForTick();
}

/**
 * The text layer contains some spans with the text of the pdf.
 * @param {Object} browser
 * @param {string} text
 * @returns {Object} the bbox of the span containing the text.
 */
async function getSpanBox(browser, text) {
  return SpecialPowers.spawn(browser, [text], async function (text) {
    const { ContentTaskUtils } = ChromeUtils.importESModule(
      "resource://testing-common/ContentTaskUtils.sys.mjs"
    );
    const { document } = content;

    await ContentTaskUtils.waitForCondition(
      () => !!document.querySelector(".textLayer .endOfContent"),
      "The text layer must be displayed"
    );

    let targetSpan = null;
    for (const span of document.querySelectorAll(
      `.textLayer span[role="presentation"]`
    )) {
      if (span.innerText.includes(text)) {
        targetSpan = span;
        break;
      }
    }

    Assert.ok(targetSpan, `document must have a span containing '${text}'`);

    const { x, y, width, height } = targetSpan.getBoundingClientRect();
    return { x, y, width, height };
  });
}

/**
 * Count the number of elements corresponding to the given selector.
 * @param {Object} browser
 * @param {string} selector
 * @returns
 */
async function countElements(browser, selector) {
  return SpecialPowers.spawn(browser, [selector], async function (selector) {
    return new Promise(resolve => {
      content.setTimeout(() => {
        resolve(content.document.querySelectorAll(selector).length);
      }, 0);
    });
  });
}

/**
 * Click at the given coordinates.
 * @param {Object} browser
 * @param {number} x
 * @param {number} y
 */
async function clickAt(browser, x, y) {
  await BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "mousedown",
      button: 0,
    },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtPoint(
    x,
    y,
    {
      type: "mouseup",
      button: 0,
    },
    browser
  );
  await TestUtils.waitForTick();
}

/**
 * Click on the element corresponding to the given selector.
 * @param {Object} browser
 * @param {string} selector
 */
async function clickOn(browser, selector) {
  await waitForSelector(browser, selector);
  const [x, y] = await SpecialPowers.spawn(
    browser,
    [selector],
    async selector => {
      const element = content.document.querySelector(selector);
      Assert.ok(
        !!element,
        `Element "${selector}" must be available in order to be clicked`
      );
      const { x, y, width, height } = element.getBoundingClientRect();
      return [x + width / 2, y + height / 2];
    }
  );
  await clickAt(browser, x, y);
}

async function focusEditorLayer(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    const layer = content.document.querySelector(".annotationEditorLayer");
    if (layer === content.document.activeElement) {
      return Promise.resolve();
    }
    const promise = new Promise(resolve => {
      const listener = () => {
        layer.removeEventListener("focus", listener);
        resolve();
      };
      layer.addEventListener("focus", listener);
    });
    layer.focus();
    return promise;
  });
}

/**
 * Hit a key.
 */
async function hitKey(browser, char) {
  await SpecialPowers.spawn(browser, [char], async function (char) {
    const { ContentTaskUtils } = ChromeUtils.importESModule(
      "resource://testing-common/ContentTaskUtils.sys.mjs"
    );
    const EventUtils = ContentTaskUtils.getEventUtils(content);
    await EventUtils.synthesizeKey(char, {}, content);
  });
  await TestUtils.waitForTick();
}

/**
 * Write some text using the keyboard.
 * @param {Object} browser
 * @param {string} text
 */
async function write(browser, text) {
  for (const char of text.split("")) {
    hitKey(browser, char);
  }
}

/**
 * Hit escape key.
 */
async function escape(browser) {
  await hitKey(browser, "KEY_Escape");
}

/**
 * Add a FreeText annotation and write some text inside.
 * @param {Object} browser
 * @param {string} text
 * @param {Object} box
 */
async function addFreeText(browser, text, box) {
  const { x, y, width, height } = box;
  const count = await countElements(browser, ".freeTextEditor");
  await focusEditorLayer(browser);
  await clickAt(browser, x + 0.1 * width, y + 0.5 * height);
  await BrowserTestUtils.waitForCondition(
    async () => (await countElements(browser, ".freeTextEditor")) === count + 1
  );

  await write(browser, text);
  await escape(browser);
}

function changeMimeHandler(preferredAction, alwaysAskBeforeHandling) {
  let handlerService = Cc[
    "@mozilla.org/uriloader/handler-service;1"
  ].getService(Ci.nsIHandlerService);
  let mimeService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);
  let handlerInfo = mimeService.getFromTypeAndExtension(
    "application/pdf",
    "pdf"
  );
  var oldAction = [
    handlerInfo.preferredAction,
    handlerInfo.alwaysAskBeforeHandling,
  ];

  // Change and save mime handler settings
  handlerInfo.alwaysAskBeforeHandling = alwaysAskBeforeHandling;
  handlerInfo.preferredAction = preferredAction;
  handlerService.store(handlerInfo);

  // Refresh data
  handlerInfo = mimeService.getFromTypeAndExtension("application/pdf", "pdf");

  // Test: Mime handler was updated
  is(
    handlerInfo.alwaysAskBeforeHandling,
    alwaysAskBeforeHandling,
    "always-ask prompt change successful"
  );
  is(
    handlerInfo.preferredAction,
    preferredAction,
    "mime handler change successful"
  );

  return oldAction;
}

function createTemporarySaveDirectory(id = "") {
  var saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append(`testsavedir${id}`);
  if (!saveDir.exists()) {
    saveDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  }
  return saveDir;
}

async function cleanupDownloads(listId = Downloads.PUBLIC) {
  info("cleaning up downloads");
  let downloadList = await Downloads.getList(listId);
  for (let download of await downloadList.getAll()) {
    await download.finalize(true);
    try {
      if (Services.appinfo.OS === "WINNT") {
        // We need to make the file writable to delete it on Windows.
        await IOUtils.setPermissions(download.target.path, 0o600);
      }
      await IOUtils.remove(download.target.path);
    } catch (error) {
      info("The file " + download.target.path + " is not removed, " + error);
    }

    await downloadList.remove(download);
    await download.finalize();
  }
}
