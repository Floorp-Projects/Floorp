/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

async function testAppliedFilters(ext, expectedFilter, expectedFilterCount) {
  let tempDir = FileUtils.getDir("TmpD", [`testDownloadDir-${Math.random()}`]);
  tempDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let filterCount = 0;

  let MockFilePicker = SpecialPowers.MockFilePicker;
  MockFilePicker.init(window);
  MockFilePicker.displayDirectory = tempDir;
  MockFilePicker.returnValue = MockFilePicker.returnCancel;
  MockFilePicker.appendFiltersCallback = function (fp, val) {
    const hexstr = "0x" + ("000" + val.toString(16)).substr(-3);
    filterCount++;
    if (filterCount < expectedFilterCount) {
      is(val, expectedFilter, "Got expected filter: " + hexstr);
    } else if (filterCount == expectedFilterCount) {
      is(val, MockFilePicker.filterAll, "Got all files filter: " + hexstr);
    } else {
      is(val, null, "Got unexpected filter: " + hexstr);
    }
  };
  MockFilePicker.showCallback = function (fp) {
    const filename = fp.defaultString;
    info("MockFilePicker - save as: " + filename);
  };

  let manifest = {
    description: ext,
    permissions: ["downloads"],
  };

  const extension = ExtensionTestUtils.loadExtension({
    manifest: manifest,

    background: async function () {
      let ext = chrome.runtime.getManifest().description;
      await browser.test.assertRejects(
        browser.downloads.download({
          url: "http://any-origin/any-path/any-resource",
          filename: "any-file" + ext,
          saveAs: true,
        }),
        "Download canceled by the user",
        "expected request to be canceled"
      );
      browser.test.sendMessage("canceled");
    },
  });

  await extension.startup();
  await extension.awaitMessage("canceled");
  await extension.unload();

  is(
    filterCount,
    expectedFilterCount,
    "Got correct number of filters: " + filterCount
  );

  MockFilePicker.cleanup();

  tempDir.remove(true);
}

// Missing extension
add_task(async function testDownload_missing_All() {
  await testAppliedFilters("", null, 1);
});

// Unrecognized extension
add_task(async function testDownload_unrecognized_All() {
  await testAppliedFilters(".xxx", null, 1);
});

// Recognized extensions
add_task(async function testDownload_html_HTML() {
  await testAppliedFilters(".html", Ci.nsIFilePicker.filterHTML, 2);
});

add_task(async function testDownload_xhtml_HTML() {
  await testAppliedFilters(".xhtml", Ci.nsIFilePicker.filterHTML, 2);
});

add_task(async function testDownload_txt_Text() {
  await testAppliedFilters(".txt", Ci.nsIFilePicker.filterText, 2);
});

add_task(async function testDownload_text_Text() {
  await testAppliedFilters(".text", Ci.nsIFilePicker.filterText, 2);
});

add_task(async function testDownload_jpe_Images() {
  await testAppliedFilters(".jpe", Ci.nsIFilePicker.filterImages, 2);
});

add_task(async function testDownload_tif_Images() {
  await testAppliedFilters(".tif", Ci.nsIFilePicker.filterImages, 2);
});

add_task(async function testDownload_webp_Images() {
  await testAppliedFilters(".webp", Ci.nsIFilePicker.filterImages, 2);
});

add_task(async function testDownload_heic_Images() {
  await testAppliedFilters(".heic", Ci.nsIFilePicker.filterImages, 2);
});

add_task(async function testDownload_xml_XML() {
  await testAppliedFilters(".xml", Ci.nsIFilePicker.filterXML, 2);
});

add_task(async function testDownload_aac_Audio() {
  await testAppliedFilters(".aac", Ci.nsIFilePicker.filterAudio, 2);
});

add_task(async function testDownload_mp3_Audio() {
  await testAppliedFilters(".mp3", Ci.nsIFilePicker.filterAudio, 2);
});

add_task(async function testDownload_wma_Audio() {
  await testAppliedFilters(".wma", Ci.nsIFilePicker.filterAudio, 2);
});

add_task(async function testDownload_avi_Video() {
  await testAppliedFilters(".avi", Ci.nsIFilePicker.filterVideo, 2);
});

add_task(async function testDownload_mp4_Video() {
  await testAppliedFilters(".mp4", Ci.nsIFilePicker.filterVideo, 2);
});

add_task(async function testDownload_xvid_Video() {
  await testAppliedFilters(".xvid", Ci.nsIFilePicker.filterVideo, 2);
});
