// There are at least seven different ways in a which a file can be saved or downloaded. This
// test ensures that the filename is determined correctly when saving in these ways. The seven
// ways are:
//   - save the file individually from the File menu
//   - save as complete web page (this uses a different codepath than the previous one)
//   - dragging an image to the local file system
//   - copy an image and paste it as a file to the local file system (windows only)
//   - open a link with content-disposition set to attachment
//   - open a link with the download attribute
//   - save a link or image from the context menu

requestLongerTimeout(8);

let types = {
  text: "text/plain",
  html: "text/html",
  png: "image/png",
  jpeg: "image/jpeg",
  webp: "image/webp",
  otherimage: "image/unknown",
  // Other js types (application/javascript and text/javascript) are handled by the system
  // inconsistently, but application/x-javascript is handled by the external helper app service,
  // so it is used here for this test.
  js: "application/x-javascript",
  binary: "application/octet-stream",
  nonsense: "application/x-nonsense",
  zip: "application/zip",
  json: "application/json",
  tar: "application/x-tar",
};

const PNG_DATA = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
    "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII="
);

const JPEG_DATA = atob(
  "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4z" +
    "NDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCAABAAEDASIAAhEB" +
    "AxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS" +
    "0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKz" +
    "tLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgEC" +
    "BAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpj" +
    "ZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6" +
    "/9oADAMBAAIRAxEAPwD3+iiigD//2Q=="
);

const WEBP_DATA = atob(
  "UklGRiIAAABXRUJQVlA4TBUAAAAvY8AYAAfQ/4j+B4CE8H+/ENH/VCIA"
);

const DEFAULT_FILENAME =
  AppConstants.platform == "win" ? "Untitled.htm" : "Untitled.html";

const PROMISE_FILENAME_TYPE = "application/x-moz-file-promise-dest-filename";

let MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);

let expectedItems;
let sendAsAttachment = false;
let httpServer = null;

function handleRequest(aRequest, aResponse) {
  const queryString = new URLSearchParams(aRequest.queryString);
  let type = queryString.get("type");
  let filename = queryString.get("filename");
  let dispname = queryString.get("dispname");

  aResponse.setStatusLine(aRequest.httpVersion, 200);
  if (type) {
    aResponse.setHeader("Content-Type", types[type]);
  }

  if (dispname) {
    let dispositionType = sendAsAttachment ? "attachment" : "inline";
    aResponse.setHeader(
      "Content-Disposition",
      dispositionType + ';name="' + dispname + '"'
    );
  } else if (filename) {
    let dispositionType = sendAsAttachment ? "attachment" : "inline";
    aResponse.setHeader(
      "Content-Disposition",
      dispositionType + ';filename="' + filename + '"'
    );
  } else if (sendAsAttachment) {
    aResponse.setHeader("Content-Disposition", "attachment");
  }

  if (type == "png") {
    aResponse.write(PNG_DATA);
  } else if (type == "jpeg") {
    aResponse.write(JPEG_DATA);
  } else if (type == "webp") {
    aResponse.write(WEBP_DATA);
  } else if (type == "html") {
    aResponse.write(
      "<html><head><title>file.inv</title></head><body>File</body></html>"
    );
  } else {
    aResponse.write("// Some Text");
  }
}

function handleBasicImageRequest(aRequest, aResponse) {
  aResponse.setHeader("Content-Type", "image/png");
  aResponse.write(PNG_DATA);
}

function handleRedirect(aRequest, aResponse) {
  const queryString = new URLSearchParams(aRequest.queryString);
  let filename = queryString.get("filename");

  aResponse.setStatusLine(aRequest.httpVersion, 302);
  aResponse.setHeader("Location", "/bell" + filename[0] + "?" + queryString);
}

// nsIFile::CreateUnique crops long filenames if the path is too long, but
// we don't know exactly how long depending on the full path length, so
// for those save methods that use CreateUnique, instead just verify that
// the filename starts with the right string and has the correct extension.
function checkShortenedFilename(actual, expected) {
  if (actual.length < expected.length) {
    let actualDot = actual.lastIndexOf(".");
    let actualExtension = actual.substring(actualDot);
    let expectedExtension = expected.substring(expected.lastIndexOf("."));
    if (
      actualExtension == expectedExtension &&
      expected.startsWith(actual.substring(0, actualDot))
    ) {
      return true;
    }
  }

  return false;
}

add_setup(async function () {
  const { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );
  httpServer = new HttpServer();
  httpServer.start(8000);

  // Need to load the page from localhost:8000 as the download attribute
  // only applies to links from the same domain.
  let saveFilenamesPage = await IOUtils.getFile(
    Services.dirsvc.get("CurWorkD", Ci.nsIFile).path,
    ..."browser/uriloader/exthandler/tests/mochitest/save_filenames.html".split(
      "/"
    )
  );
  httpServer.registerFile("/save_filenames.html", saveFilenamesPage);

  // A variety of different scripts are set up to better ensure uniqueness.
  httpServer.registerPathHandler("/save_filename.sjs", handleRequest);
  httpServer.registerPathHandler("/save_thename.sjs", handleRequest);
  httpServer.registerPathHandler("/getdata.png", handleRequest);
  httpServer.registerPathHandler("/base", handleRequest);
  httpServer.registerPathHandler("/basedata", handleRequest);
  httpServer.registerPathHandler("/basetext", handleRequest);
  httpServer.registerPathHandler("/text2.txt", handleRequest);
  httpServer.registerPathHandler("/text3.gonk", handleRequest);
  httpServer.registerPathHandler("/basic.png", handleBasicImageRequest);
  httpServer.registerPathHandler("/aquamarine.jpeg", handleBasicImageRequest);
  httpServer.registerPathHandler("/lazuli.exe", handleBasicImageRequest);
  httpServer.registerPathHandler("/redir", handleRedirect);
  httpServer.registerPathHandler("/bellr", handleRequest);
  httpServer.registerPathHandler("/bellg", handleRequest);
  httpServer.registerPathHandler("/bellb", handleRequest);
  httpServer.registerPathHandler("/executable.exe", handleRequest);

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://localhost:8000/save_filenames.html"
  );

  expectedItems = await getItems("items");
});

function getItems(parentid) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [parentid, AppConstants.platform],
    (id, platform) => {
      let elements = [];
      let elem = content.document.getElementById(id).firstElementChild;
      while (elem) {
        let filename =
          elem.dataset["filenamePlatform" + platform] || elem.dataset.filename;
        let url = elem.getAttribute("src") || elem.getAttribute("href");
        let draggable =
          elem.localName == "img" && elem.dataset.nodrag != "true";
        let unknown = elem.dataset.unknown;
        let noattach = elem.dataset.noattach;
        let savepagename = elem.dataset.savepagename;
        let pickedfilename = elem.dataset.pickedfilename;
        elements.push({
          draggable,
          unknown,
          filename,
          url,
          noattach,
          savepagename,
          pickedfilename,
        });
        elem = elem.nextElementSibling;
      }
      return elements;
    }
  );
}

function getDirectoryEntries(dir) {
  let files = [];
  let entries = dir.directoryEntries;
  while (true) {
    let file = entries.nextFile;
    if (!file) {
      break;
    }
    files.push(file.leafName);
  }
  entries.close();
  return files;
}

// This test saves the document as a complete web page and verifies
// that the resources are saved with the correct filename.
add_task(async function save_document() {
  let browser = gBrowser.selectedBrowser;

  let tmp = SpecialPowers.Services.dirsvc.get("TmpD", Ci.nsIFile);
  const baseFilename = "test_save_filenames_" + Date.now();

  let tmpFile = tmp.clone();
  tmpFile.append(baseFilename + "_document.html");
  let tmpDir = tmp.clone();
  tmpDir.append(baseFilename + "_document_files");

  MockFilePicker.displayDirectory = tmpDir;
  MockFilePicker.showCallback = function (fp) {
    MockFilePicker.setFiles([tmpFile]);
    MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
  };

  let downloadsList = await Downloads.getList(Downloads.PUBLIC);
  let savePromise = promiseDownloadFinished(downloadsList);
  saveBrowser(browser);
  await savePromise;

  let filesSaved = getDirectoryEntries(tmpDir);

  for (let idx = 0; idx < expectedItems.length; idx++) {
    let filename = expectedItems[idx].filename;
    if (idx == 66 && AppConstants.platform == "win") {
      // This is special-cased on Windows. The default filename will be used, since
      // the filename is invalid, but since the previous test file has the same issue,
      // this second file will be saved with a number suffix added to it.
      filename = "Untitled_002";
    }

    let file = tmpDir.clone();
    file.append(filename);

    let fileIdx = -1;
    // Use checkShortenedFilename to check long filenames.
    if (filename.length > 240) {
      for (let t = 0; t < filesSaved.length; t++) {
        if (
          filesSaved[t].length > 60 &&
          checkShortenedFilename(filesSaved[t], filename)
        ) {
          fileIdx = t;
          break;
        }
      }
    } else {
      fileIdx = filesSaved.indexOf(filename);
    }

    ok(
      fileIdx >= 0,
      "file i" +
        idx +
        " " +
        filename +
        " was saved with the correct name using saveDocument"
    );
    if (fileIdx >= 0) {
      // If found, remove it from the list. At end of the test, the
      // list should be empty.
      filesSaved.splice(fileIdx, 1);
    }
  }

  is(filesSaved.length, 0, "all files accounted for");
  tmpDir.remove(true);
  tmpFile.remove(false);
  downloadsList.removeFinished();
});

// This test simulates dragging the images in the document and ensuring that
// the correct filename is used for each one.
// On Mac, the data is added in the parent process instead, so we cannot
// test dragging directly.
if (AppConstants.platform != "macosx") {
  add_task(async function drag_files() {
    let browser = gBrowser.selectedBrowser;

    await SpecialPowers.spawn(browser, [PROMISE_FILENAME_TYPE], type => {
      content.addEventListener("dragstart", event => {
        content.draggedFile = event.dataTransfer.getData(type);
        event.preventDefault();
      });
    });

    for (let idx = 0; idx < expectedItems.length; idx++) {
      if (!expectedItems[idx].draggable) {
        // You can't drag non-images and invalid images.
        continue;
      }

      await BrowserTestUtils.synthesizeMouse(
        "#i" + idx,
        1,
        1,
        { type: "mousedown" },
        browser
      );
      await BrowserTestUtils.synthesizeMouse(
        "#i" + idx,
        11,
        11,
        { type: "mousemove" },
        browser
      );
      await BrowserTestUtils.synthesizeMouse(
        "#i" + idx,
        20,
        20,
        { type: "mousemove" },
        browser
      );
      await BrowserTestUtils.synthesizeMouse(
        "#i" + idx,
        20,
        20,
        { type: "mouseup" },
        browser
      );

      let draggedFile = await SpecialPowers.spawn(browser, [], () => {
        let file = content.draggedFile;
        content.draggedFile = null;
        return file;
      });

      is(
        draggedFile,
        expectedItems[idx].filename,
        "i" +
          idx +
          " " +
          expectedItems[idx].filename +
          " was saved with the correct name when dragging"
      );
    }
  });
}

// This test checks that copying an image provides the right filename
// for pasting to the local file system. This is only implemented on Windows.
const imageAsFileEnabled = SpecialPowers.getBoolPref(
  "clipboard.imageAsFile.enabled",
  false
);
if (AppConstants.platform == "win" && imageAsFileEnabled) {
  add_task(async function copy_image() {
    for (let idx = 0; idx < expectedItems.length; idx++) {
      if (!expectedItems[idx].draggable) {
        // You can't context-click on non-images.
        continue;
      }

      let data = await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [idx, PROMISE_FILENAME_TYPE],
        (imagenum, type) => {
          // No need to wait for the data to be really on the clipboard, we only
          // need the promise data added when the command is performed.
          SpecialPowers.setCommandNode(
            content,
            content.document.getElementById("i" + imagenum)
          );
          SpecialPowers.doCommand(content, "cmd_copyImageContents");

          return SpecialPowers.getClipboardData(type);
        }
      );

      is(
        data,
        expectedItems[idx].filename,
        "i" +
          idx +
          " " +
          expectedItems[idx].filename +
          " was saved with the correct name when copying"
      );
    }
  });
}

// This test checks the default filename selected when selecting to save
// a file from either the context menu or what would happen when save page
// as was selected from the file menu. Note that this tests a filename assigned
// when using content-disposition: inline.
add_task(async function saveas_files() {
  // Iterate over each item and try saving them from the context menu,
  // and the Save Page As command on the File menu.
  for (let testname of ["context menu", "save page as"]) {
    for (let idx = 0; idx < expectedItems.length; idx++) {
      let menu;
      if (testname == "context menu") {
        if (!expectedItems[idx].draggable) {
          // You can't context-click on non-images.
          continue;
        }

        menu = document.getElementById("contentAreaContextMenu");
        let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
        BrowserTestUtils.synthesizeMouse(
          "#i" + idx,
          5,
          5,
          { type: "contextmenu", button: 2 },
          gBrowser.selectedBrowser
        );
        await popupShown;
      } else {
        if (expectedItems[idx].unknown == "typeonly") {
          // Items marked with unknown="typeonly" have unknown content types and
          // will be downloaded instead of opened in a tab.
          let list = await Downloads.getList(Downloads.PUBLIC);
          let downloadFinishedPromise = promiseDownloadFinished(list);

          await BrowserTestUtils.openNewForegroundTab({
            gBrowser,
            opening: expectedItems[idx].url,
            waitForLoad: false,
            waitForStateStop: true,
          });

          let download = await downloadFinishedPromise;

          let filename = PathUtils.filename(download.target.path);

          let expectedFilename = expectedItems[idx].filename;
          if (expectedFilename.length > 240) {
            ok(
              checkShortenedFilename(filename, expectedFilename),
              "open link" +
                idx +
                " " +
                expectedFilename +
                " was downloaded with the correct name when opened as a url (with long name)"
            );
          } else {
            is(
              filename,
              expectedFilename,
              "open link" +
                idx +
                " " +
                expectedFilename +
                " was downloaded with the correct name when opened as a url"
            );
          }

          try {
            await IOUtils.remove(download.target.path);
          } catch (ex) {}

          await BrowserTestUtils.removeTab(gBrowser.selectedTab);
          continue;
        }

        await BrowserTestUtils.openNewForegroundTab({
          gBrowser,
          opening: expectedItems[idx].url,
          waitForLoad: false,
          waitForStateStop: true,
        });
      }

      let filename = await new Promise(resolve => {
        MockFilePicker.showCallback = function (fp) {
          setTimeout(() => {
            resolve(fp.defaultString);
          }, 0);
          return Ci.nsIFilePicker.returnCancel;
        };

        if (testname == "context menu") {
          let menuitem = document.getElementById("context-saveimage");
          menu.activateItem(menuitem);
        } else if (testname == "save page as") {
          document.getElementById("Browser:SavePage").doCommand();
        }
      });

      // Trying to open an unknown or binary type will just open a blank
      // page, so trying to save will just save the blank page with the
      // filename Untitled.html.
      let expectedFilename = expectedItems[idx].unknown
        ? DEFAULT_FILENAME
        : expectedItems[idx].savepagename || expectedItems[idx].filename;

      // When saving via contentAreaUtils.js, the content disposition name
      // field is used as an alternate.
      if (expectedFilename == "save_thename.png") {
        expectedFilename = "withname.png";
      }

      is(
        filename,
        expectedFilename,
        "i" +
          idx +
          " " +
          expectedFilename +
          " was saved with the correct name " +
          testname
      );

      if (testname == "save page as") {
        await BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }
    }
  }
});

// This test checks that the filename is saved correctly when it
// has been modified within the file picker.
add_task(async function saveas_files_modified_in_filepicker() {
  let items = await getItems("modifieditems");
  for (let idx = 0; idx < items.length; idx++) {
    await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      opening: items[idx].url,
      waitForLoad: false,
      waitForStateStop: true,
    });

    let savedFile = SpecialPowers.Services.dirsvc.get("TmpD", Ci.nsIFile);

    let downloadsList = await Downloads.getList(Downloads.PUBLIC);
    let savePromise = promiseDownloadFinished(downloadsList);

    await new Promise(resolve => {
      MockFilePicker.displayDirectory = savedFile;

      MockFilePicker.showCallback = function (fp) {
        MockFilePicker.filterIndex = 0; // kSaveAsType_Complete
        savedFile.append(items[idx].pickedfilename);
        MockFilePicker.setFiles([savedFile]);
        setTimeout(() => {
          resolve(items[idx].pickedfilename);
        }, 0);

        return Ci.nsIFilePicker.returnOK;
      };

      document.getElementById("Browser:SavePage").doCommand();
    });

    await savePromise;

    savedFile.leafName = items[idx].filename;
    ok(
      savedFile.exists(),
      "i" +
        idx +
        " '" +
        savedFile.leafName +
        "' was saved when modified with the correct name "
    );
    if (savedFile.exists()) {
      savedFile.remove(false);
    }

    await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

// This test checks that links that result in files with
// content-disposition: attachment are saved with the right filenames.
add_task(async function save_links() {
  sendAsAttachment = true;

  // Create some links based on each image and insert them into the document.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let doc = content.document;
    let insertPos = doc.getElementById("attachment-links");

    let idx = 0;
    let elem = doc.getElementById("items").firstElementChild;
    while (elem) {
      let attachmentlink = doc.createElement("a");
      attachmentlink.id = "attachmentlink" + idx;
      attachmentlink.href = elem.localName == "object" ? elem.data : elem.src;
      attachmentlink.textContent = elem.dataset.filename;
      insertPos.appendChild(attachmentlink);
      insertPos.appendChild(doc.createTextNode("  "));

      elem = elem.nextElementSibling;
      idx++;
    }
  });

  let list = await Downloads.getList(Downloads.PUBLIC);

  for (let idx = 0; idx < expectedItems.length; idx++) {
    // Skip the items that won't have a content-disposition.
    if (expectedItems[idx].noattach) {
      continue;
    }

    let downloadFinishedPromise = promiseDownloadFinished(list);

    BrowserTestUtils.synthesizeMouse(
      "#attachmentlink" + idx,
      5,
      5,
      {},
      gBrowser.selectedBrowser
    );

    let download = await downloadFinishedPromise;

    let filename = PathUtils.filename(download.target.path);

    let expectedFilename = expectedItems[idx].filename;
    // Use checkShortenedFilename to check long filenames.
    if (expectedItems[idx].filename.length > 240) {
      ok(
        checkShortenedFilename(filename, expectedFilename),
        "attachmentlink" +
          idx +
          " " +
          expectedFilename +
          " was saved with the correct name when opened as attachment (with long name)"
      );
    } else {
      is(
        filename,
        expectedFilename,
        "attachmentlink" +
          idx +
          " " +
          expectedFilename +
          " was saved with the correct name when opened as attachment"
      );
    }

    try {
      await IOUtils.remove(download.target.path);
    } catch (ex) {}
  }

  sendAsAttachment = false;
});

// This test checks some cases where links to images are saved using Save Link As,
// and when opening them in a new tab and then using Save Page As.
add_task(async function saveas_image_links() {
  let links = await getItems("links");

  // Iterate over each link and try saving the links from the context menu,
  // and then after opening a new tab for that link and then selecting
  // the Save Page As command on the File menu.
  for (let testname of ["save link as", "save link then save page as"]) {
    for (let idx = 0; idx < links.length; idx++) {
      let menu = document.getElementById("contentAreaContextMenu");
      let popupShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouse(
        "#link" + idx,
        5,
        5,
        { type: "contextmenu", button: 2 },
        gBrowser.selectedBrowser
      );
      await popupShown;

      let promptPromise = new Promise(resolve => {
        MockFilePicker.showCallback = function (fp) {
          setTimeout(() => {
            resolve(fp.defaultString);
          }, 0);
          return Ci.nsIFilePicker.returnCancel;
        };
      });

      if (testname == "save link as") {
        let menuitem = document.getElementById("context-savelink");
        menu.activateItem(menuitem);
      } else {
        let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser);

        let menuitem = document.getElementById("context-openlinkintab");
        menu.activateItem(menuitem);

        let tab = await newTabPromise;
        await BrowserTestUtils.switchTab(gBrowser, tab);

        document.getElementById("Browser:SavePage").doCommand();
      }

      let filename = await promptPromise;

      let expectedFilename = links[idx].filename;
      // Only codepaths that go through contentAreaUtils.js use the
      // name from the content disposition.
      if (testname == "save link as" && expectedFilename == "four.png") {
        expectedFilename = "save_filename.png";
      }

      is(
        filename,
        expectedFilename,
        "i" +
          idx +
          " " +
          expectedFilename +
          " link was saved with the correct name " +
          testname
      );

      if (testname == "save link then save page as") {
        await BrowserTestUtils.removeTab(gBrowser.selectedTab);
      }
    }
  }
});

// This test checks that links that with a download attribute
// are saved with the right filenames.
add_task(async function save_download_links() {
  let downloads = await getItems("downloads");

  let list = await Downloads.getList(Downloads.PUBLIC);
  for (let idx = 0; idx < downloads.length; idx++) {
    let downloadFinishedPromise = promiseDownloadFinished(list);

    BrowserTestUtils.synthesizeMouse(
      "#download" + idx,
      2,
      2,
      {},
      gBrowser.selectedBrowser
    );

    let download = await downloadFinishedPromise;

    let filename = PathUtils.filename(download.target.path);

    if (downloads[idx].filename.length > 240) {
      ok(
        checkShortenedFilename(filename, downloads[idx].filename),
        "download" +
          idx +
          " " +
          downloads[idx].filename +
          " was saved with the correct name when link has download attribute"
      );
    } else {
      if (idx == 66 && filename == "Untitled(1)") {
        // Sometimes, the previous test's file still exists or wasn't created in time
        // and a non-duplicated name is created. Allow this rather than figuring out
        // how to avoid it since it doesn't affect what is being tested here.
        filename = "Untitled";
      }

      is(
        filename,
        downloads[idx].filename,
        "download" +
          idx +
          " " +
          downloads[idx].filename +
          " was saved with the correct name when link has download attribute"
      );
    }

    try {
      await IOUtils.remove(download.target.path);
    } catch (ex) {}
  }
});

// This test verifies that invalid extensions are not removed when they
// have been entered in the file picker.
add_task(async function save_page_with_invalid_after_filepicker() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://localhost:8000/save_filename.sjs?type=html&filename=invfile.lnk"
  );

  let filename = await new Promise(resolve => {
    MockFilePicker.showCallback = function (fp) {
      let expectedFilename =
        AppConstants.platform == "win" ? "invfile.lnk.htm" : "invfile.lnk.html";
      is(fp.defaultString, expectedFilename, "supplied filename is correct");
      setTimeout(() => {
        resolve("otherfile.local");
      }, 0);
      return Ci.nsIFilePicker.returnCancel;
    };

    document.getElementById("Browser:SavePage").doCommand();
  });

  is(filename, "otherfile.local", "lnk extension has been preserved");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function save_page_with_invalid_extension() {
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://localhost:8000/save_filename.sjs?type=html"
  );

  let filename = await new Promise(resolve => {
    MockFilePicker.showCallback = function (fp) {
      setTimeout(() => {
        resolve(fp.defaultString);
      }, 0);
      return Ci.nsIFilePicker.returnCancel;
    };

    document.getElementById("Browser:SavePage").doCommand();
  });

  is(
    filename,
    AppConstants.platform == "win" ? "file.inv.htm" : "file.inv.html",
    "html extension has been added"
  );

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async () => {
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  MockFilePicker.cleanup();
  await new Promise(resolve => httpServer.stop(resolve));
});
