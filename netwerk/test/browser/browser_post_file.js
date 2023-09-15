/*
 * Tests for bug 1241100: Post to local file should not overwrite the file.
 */
"use strict";

async function createTestFile(filename, content) {
  let path = PathUtils.join(PathUtils.tempDir, filename);
  await IOUtils.writeUTF8(path, content);
  return path;
}

add_task(async function () {
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

  var postURI = PathUtils.toFileURI(postPath);
  var actionURI = PathUtils.toFileURI(actionPath);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    true,
    actionURI
  );
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, postURI);
  await browserLoadedPromise;

  var actionFileContentAfter = await IOUtils.readUTF8(actionPath);
  is(actionFileContentAfter, actionFileContent, "action file is not modified");

  await IOUtils.remove(postPath);
  await IOUtils.remove(actionPath);

  gBrowser.removeCurrentTab();
});
