/*
 * Tests for bug 1241100: Post to local file should not overwrite the file.
 */

Components.utils.import("resource://gre/modules/osfile.jsm");

async function createTestFile(filename, content) {
  let path = OS.Path.join(OS.Constants.Path.tmpDir, filename);
  await OS.File.writeAtomic(path, content);
  return path;
}

async function readFile(path) {
  var array = await OS.File.read(path);
  var decoder = new TextDecoder();
  return decoder.decode(array);
}

function frameScript() {
  addMessageListener("Test:WaitForIFrame", function() {
    var check = function() {
      if (content) {
        var frame = content.document.getElementById("frame");
        if (frame) {
          var okBox = frame.contentDocument.getElementById("action_file_ok");
          if (okBox) {
            sendAsyncMessage("Test:IFrameLoaded");
            return;
          }
        }
      }

      setTimeout(check, 100);
    };

    check();
  });
}

add_task(async function() {
  var postFilename = "post_file.html";
  var actionFilename = "action_file.html";

  var postFileContent = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>post file</title>
</head>
<body onload="document.getElementById('form').submit();">
<form id="form" action="${actionFilename}" method="post" enctype="text/plain" target="frame">
<input type="hidden" name="foo" value="bar">
<input type="submit">
</form>
<iframe id="frame" name="frame"></iframe>
</body>
</html>
`;

  var actionFileContent = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>action file</title>
</head>
<body>
<div id="action_file_ok">ok</div>
</body>
</html>
`;

  var postPath = await createTestFile(postFilename, postFileContent);
  var actionPath = await createTestFile(actionFilename, actionFileContent);

  var postURI = OS.Path.toFileURI(postPath);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, postURI);
  let browser = gBrowser.selectedBrowser;
  browser.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")();", true);
  await new Promise(resolve => {
    let manager = browser.messageManager;

    function listener() {
      manager.removeMessageListener("Test:IFrameLoaded", listener);
      resolve();
    }

    manager.addMessageListener("Test:IFrameLoaded", listener);
    manager.sendAsyncMessage("Test:WaitForIFrame");
  });

  var actionFileContentAfter = await readFile(actionPath);
  is(actionFileContentAfter, actionFileContent, "action file is not modified");

  await OS.File.remove(postPath);
  await OS.File.remove(actionPath);

  gBrowser.removeCurrentTab();
});
