/**
 * Bug 1641270 - A test case for ensuring the save channel will use the correct
 *               cookieJarSettings when doing the saving and the cache would
 *               work as expected.
 */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/content/tests/browser/common/mockTransfer.js",
  this
);

const TEST_IMAGE_URL = TEST_DOMAIN + TEST_PATH + "file_saveAsImage.sjs";
const TEST_VIDEO_URL = TEST_DOMAIN + TEST_PATH + "file_saveAsVideo.sjs";
const TEST_PAGEINFO_URL = TEST_DOMAIN + TEST_PATH + "file_saveAsPageInfo.html";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

const tempDir = createTemporarySaveDirectory();
MockFilePicker.displayDirectory = tempDir;

function createTemporarySaveDirectory() {
  let saveDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  saveDir.append("testsavedir");
  saveDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  return saveDir;
}

function createPromiseForTransferComplete(aDesirableFileName) {
  return new Promise(resolve => {
    MockFilePicker.showCallback = fp => {
      info("MockFilePicker showCallback");

      let fileName = fp.defaultString;
      let destFile = tempDir.clone();
      destFile.append(fileName);

      if (aDesirableFileName) {
        is(fileName, aDesirableFileName, "The default file name is correct.");
      }

      MockFilePicker.setFiles([destFile]);
      MockFilePicker.filterIndex = 0; // kSaveAsType_Complete

      MockFilePicker.showCallback = null;
      mockTransferCallback = function (downloadSuccess) {
        ok(downloadSuccess, "File should have been downloaded successfully");
        mockTransferCallback = () => {};
        resolve();
      };
    };
  });
}

function createPromiseForObservingChannel(aURL, aPartitionKey) {
  return new Promise(resolve => {
    let observer = (aSubject, aTopic) => {
      if (aTopic === "http-on-modify-request") {
        let httpChannel = aSubject.QueryInterface(Ci.nsIHttpChannel);
        let reqLoadInfo = httpChannel.loadInfo;

        // Make sure this is the request which we want to check.
        if (!httpChannel.URI.spec.endsWith(aURL)) {
          return;
        }

        info(`Checking loadInfo for URI: ${httpChannel.URI.spec}\n`);
        is(
          reqLoadInfo.cookieJarSettings.partitionKey,
          aPartitionKey,
          "The loadInfo has the correct partition key"
        );

        Services.obs.removeObserver(observer, "http-on-modify-request");
        resolve();
      }
    };

    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

add_setup(async function () {
  info("Setting MockFilePicker.");
  mockTransferRegisterer.register();

  registerCleanupFunction(function () {
    mockTransferRegisterer.unregister();
    MockFilePicker.cleanup();
    tempDir.remove(true);
  });
});

add_task(async function testContextMenuSaveImage() {
  let uuidGenerator = Services.uuid;

  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      // We use token to separate the caches.
      let token = uuidGenerator.generateUUID().toString();
      const testImageURL = `${TEST_IMAGE_URL}?token=${token}`;

      info(`Open a new tab for testing "Save image as" in context menu.`);
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_TOP_PAGE
      );

      info(`Insert the testing image into the tab.`);
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [testImageURL],
        async url => {
          let img = content.document.createElement("img");
          let loaded = new content.Promise(resolve => {
            img.onload = resolve;
          });
          content.document.body.appendChild(img);
          img.setAttribute("id", "image1");
          img.src = url;
          await loaded;
        }
      );

      info("Open the context menu.");
      let popupShownPromise = BrowserTestUtils.waitForEvent(
        document,
        "popupshown"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#image1",
        {
          type: "contextmenu",
          button: 2,
        },
        tab.linkedBrowser
      );

      await popupShownPromise;

      let partitionKey = partitionPerSite
        ? "(http,example.net)"
        : "example.net";

      let transferCompletePromise = createPromiseForTransferComplete();
      let observerPromise = createPromiseForObservingChannel(
        testImageURL,
        partitionKey
      );

      let saveElement = document.getElementById("context-saveimage");
      info("Triggering the save process.");
      saveElement.doCommand();

      info("Waiting for the channel.");
      await observerPromise;

      info("Wait until the save is finished.");
      await transferCompletePromise;

      info("Close the context menu.");
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let popupHiddenPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      contextMenu.hidePopup();
      await popupHiddenPromise;

      // Check if there will be only one network request. The another one should
      // be from cache.
      let res = await fetch(`${TEST_IMAGE_URL}?token=${token}&result`);
      let res_text = await res.text();
      is(res_text, "1", "The image should be loaded only once.");

      BrowserTestUtils.removeTab(tab);
    }
  }
});

add_task(async function testContextMenuSaveVideo() {
  let uuidGenerator = Services.uuid;

  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      // We use token to separate the caches.
      let token = uuidGenerator.generateUUID().toString();
      const testVideoURL = `${TEST_VIDEO_URL}?token=${token}`;

      info(`Open a new tab for testing "Save Video as" in context menu.`);
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_TOP_PAGE
      );

      info(`Insert the testing video into the tab.`);
      await SpecialPowers.spawn(
        tab.linkedBrowser,
        [testVideoURL],
        async url => {
          let video = content.document.createElement("video");
          let loaded = new content.Promise(resolve => {
            video.onloadeddata = resolve;
          });
          content.document.body.appendChild(video);
          video.setAttribute("id", "video1");
          video.src = url;
          await loaded;
        }
      );

      info("Open the context menu.");
      let popupShownPromise = BrowserTestUtils.waitForEvent(
        document,
        "popupshown"
      );

      await BrowserTestUtils.synthesizeMouseAtCenter(
        "#video1",
        {
          type: "contextmenu",
          button: 2,
        },
        tab.linkedBrowser
      );

      await popupShownPromise;

      let partitionKey = partitionPerSite
        ? "(http,example.net)"
        : "example.net";

      // We also check the default file name, see Bug 1679325.
      let transferCompletePromise = createPromiseForTransferComplete(
        "file_saveAsVideo.webm"
      );
      let observerPromise = createPromiseForObservingChannel(
        testVideoURL,
        partitionKey
      );

      let saveElement = document.getElementById("context-savevideo");
      info("Triggering the save process.");
      saveElement.doCommand();

      info("Waiting for the channel.");
      await observerPromise;

      info("Wait until the save is finished.");
      await transferCompletePromise;

      info("Close the context menu.");
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let popupHiddenPromise = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      contextMenu.hidePopup();
      await popupHiddenPromise;

      // Check if there will be only one network request. The another one should
      // be from cache.
      let res = await fetch(`${TEST_VIDEO_URL}?token=${token}&result`);
      let res_text = await res.text();
      is(res_text, "1", "The video should be loaded only once.");

      BrowserTestUtils.removeTab(tab);
    }
  }
});

add_task(async function testSavePageInOfflineMode() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      let partitionKey = partitionPerSite
        ? "(http,example.net)"
        : "example.net";

      info(`Open a new tab which loads an image`);
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_IMAGE_URL
      );

      info("Toggle on the offline mode");
      BrowserOffline.toggleOfflineStatus();

      info("Open file menu and trigger 'Save Page As'");
      let menubar = document.getElementById("main-menubar");
      let filePopup = document.getElementById("menu_FilePopup");

      // We only use the shortcut keys to open the file menu in Windows and Linux.
      // Mac doesn't have a shortcut to only open the file menu. Instead, we directly
      // trigger the save in MAC without any UI interactions.
      if (Services.appinfo.OS !== "Darwin") {
        let menubarActive = BrowserTestUtils.waitForEvent(
          menubar,
          "DOMMenuBarActive"
        );
        EventUtils.synthesizeKey("KEY_F10");
        await menubarActive;

        let popupShownPromise = BrowserTestUtils.waitForEvent(
          filePopup,
          "popupshown"
        );
        // In window, it still needs one extra down key to open the file menu.
        if (Services.appinfo.OS === "WINNT") {
          EventUtils.synthesizeKey("KEY_ArrowDown");
        }
        await popupShownPromise;
      }

      let transferCompletePromise = createPromiseForTransferComplete();
      let observerPromise = createPromiseForObservingChannel(
        TEST_IMAGE_URL,
        partitionKey
      );

      info("Triggering the save process.");
      let fileSavePageAsElement = document.getElementById("menu_savePage");
      fileSavePageAsElement.doCommand();

      info("Waiting for the channel.");
      await observerPromise;

      info("Wait until the save is finished.");
      await transferCompletePromise;

      // Close the file menu.
      if (Services.appinfo.OS !== "Darwin") {
        let popupHiddenPromise = BrowserTestUtils.waitForEvent(
          filePopup,
          "popuphidden"
        );
        filePopup.hidePopup();
        await popupHiddenPromise;
      }

      info("Toggle off the offline mode");
      BrowserOffline.toggleOfflineStatus();

      // Clean up
      BrowserTestUtils.removeTab(tab);

      // Clean up the cache count on the server side.
      await fetch(`${TEST_IMAGE_URL}?result`);
      await new Promise(resolve => {
        Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
          resolve()
        );
      });
    }
  }
});

add_task(async function testPageInfoMediaSaveAs() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      let partitionKey = partitionPerSite
        ? "(http,example.net)"
        : "example.net";

      info(
        `Open a new tab for testing "Save AS" in the media panel of the page info.`
      );
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_PAGEINFO_URL
      );

      info("Open the media panel of the pageinfo.");
      let pageInfo = BrowserPageInfo(
        gBrowser.selectedBrowser.currentURI.spec,
        "mediaTab"
      );

      await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

      let imageTree = pageInfo.document.getElementById("imagetree");
      let imageRowsNum = imageTree.view.rowCount;

      is(imageRowsNum, 2, "There should be two media items here.");

      for (let i = 0; i < imageRowsNum; i++) {
        imageTree.view.selection.select(i);
        imageTree.ensureRowIsVisible(i);
        imageTree.focus();

        // Wait until the preview is loaded.
        let preview = pageInfo.document.getElementById("thepreviewimage");
        let mediaType = pageInfo.gImageView.data[i][1]; // COL_IMAGE_TYPE
        if (mediaType == "Image") {
          await BrowserTestUtils.waitForEvent(preview, "load");
        } else if (mediaType == "Video") {
          await BrowserTestUtils.waitForEvent(preview, "canplaythrough");
        }

        let url = pageInfo.gImageView.data[i][0]; // COL_IMAGE_ADDRESS
        info(`Start to save the media item with URL: ${url}`);

        let transferCompletePromise = createPromiseForTransferComplete();

        // Observe the channel and check if it has the correct partitionKey.
        let observerPromise = createPromiseForObservingChannel(
          url,
          partitionKey
        );

        info("Triggering the save process.");
        let saveElement = pageInfo.document.getElementById("imagesaveasbutton");
        saveElement.doCommand();

        info("Waiting for the channel.");
        await observerPromise;

        info("Wait until the save is finished.");
        await transferCompletePromise;
      }

      pageInfo.close();
      BrowserTestUtils.removeTab(tab);
    }
  }
});

add_task(async function testPageInfoMediaMultipleSelectedSaveAs() {
  for (let networkIsolation of [true, false]) {
    for (let partitionPerSite of [true, false]) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["privacy.partition.network_state", networkIsolation],
          ["privacy.dynamic_firstparty.use_site", partitionPerSite],
        ],
      });

      let partitionKey = partitionPerSite
        ? "(http,example.net)"
        : "example.net";

      info(
        `Open a new tab for testing "Save AS" in the media panel of the page info.`
      );
      let tab = await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        TEST_PAGEINFO_URL
      );

      info("Open the media panel of the pageinfo.");
      let pageInfo = BrowserPageInfo(
        gBrowser.selectedBrowser.currentURI.spec,
        "mediaTab"
      );

      await BrowserTestUtils.waitForEvent(pageInfo, "page-info-init");

      // Make sure the preview image is loaded in order to avoid interfering
      // following tests.
      let preview = pageInfo.document.getElementById("thepreviewimage");
      await BrowserTestUtils.waitForCondition(() => {
        return preview.complete;
      });

      let imageTree = pageInfo.document.getElementById("imagetree");
      let imageRowsNum = imageTree.view.rowCount;

      is(imageRowsNum, 2, "There should be two media items here.");

      imageTree.view.selection.selectAll();
      imageTree.focus();

      let url = pageInfo.gImageView.data[0][0]; // COL_IMAGE_ADDRESS
      info(`Start to save the media item with URL: ${url}`);

      let transferCompletePromise = createPromiseForTransferComplete();
      let observerPromises = [];

      // Observe all channels and check if they have the correct partitionKey.
      for (let i = 0; i < imageRowsNum; ++i) {
        let observerPromise = createPromiseForObservingChannel(
          url,
          partitionKey
        );
        observerPromises.push(observerPromise);
      }

      info("Triggering the save process.");
      let saveElement = pageInfo.document.getElementById("imagesaveasbutton");
      saveElement.doCommand();

      info("Waiting for the all channels.");
      await Promise.all(observerPromises);

      info("Wait until the save is finished.");
      await transferCompletePromise;

      pageInfo.close();
      BrowserTestUtils.removeTab(tab);
    }
  }
});
