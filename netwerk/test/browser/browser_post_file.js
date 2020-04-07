/*
 * Tests for bug 1241100: Post to local file should not overwrite the file.
 */
"use strict";
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

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
  var actionURI = OS.Path.toFileURI(actionPath);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    actionURI
  );
  BrowserTestUtils.loadURI(tab.linkedBrowser, postURI);
  await browserLoadedPromise;

  var actionFileContentAfter = await readFile(actionPath);
  is(actionFileContentAfter, actionFileContent, "action file is not modified");

  await OS.File.remove(postPath);
  await OS.File.remove(actionPath);

  gBrowser.removeCurrentTab();
});
