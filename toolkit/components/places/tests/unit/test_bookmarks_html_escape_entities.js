/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checks that html entities are escaped in bookmarks.html files.

add_task(async function() {
  // Removes bookmarks.html if the file already exists.
  let HTMLFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.html");
  await IOUtils.remove(HTMLFile, { ignoreAbsent: true });

  let unescaped = '<unescaped="test">';
  // Adds bookmarks and tags to the database.
  const url = 'http://www.google.it/"/';
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url,
    title: unescaped,
  });
  await PlacesUtils.keywords.insert({
    url,
    keyword: unescaped,
    postData: unescaped,
  });
  PlacesUtils.tagging.tagURI(Services.io.newURI(url), [unescaped]);
  await PlacesUtils.history.update({
    url,
    annotations: new Map([[PlacesUtils.CHARSET_ANNO, unescaped]]),
  });

  // Exports the bookmarks as a HTML file.
  await BookmarkHTMLUtils.exportToFile(HTMLFile);
  await PlacesUtils.bookmarks.remove(bm);

  // Check there are no unescaped entities in the html file.
  let xml = await new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.onload = () => {
      try {
        resolve(xhr.responseXML);
      } catch (e) {
        reject(e);
      }
    };
    xhr.onabort = xhr.onerror = xhr.ontimeout = () => {
      reject(new Error("xmlhttprequest failed"));
    };
    xhr.open("GET", OS.Path.toFileURI(HTMLFile));
    xhr.responseType = "document";
    xhr.overrideMimeType("text/html");
    xhr.send();
  });

  let checksCount = 5;
  for (
    let current = xml;
    current;
    current =
      current.firstChild ||
      current.nextSibling ||
      current.parentNode.nextSibling
  ) {
    switch (current.nodeType) {
      case current.ELEMENT_NODE:
        for (let { name, value } of current.attributes) {
          info("Found attribute: " + name);
          // Check tags, keyword, postData and charSet.
          if (
            ["tags", "last_charset", "shortcuturl", "post_data"].includes(name)
          ) {
            Assert.equal(
              value,
              unescaped,
              `Attribute ${name} should be complete`
            );
            checksCount--;
          }
        }
        break;
      case current.TEXT_NODE:
        // Check Title.
        if (!current.data.startsWith("\n") && current.data.includes("test")) {
          Assert.equal(
            current.data.trim(),
            unescaped,
            "Text node should be complete"
          );
          checksCount--;
        }
        break;
    }
  }
  Assert.equal(checksCount, 0, "All the checks ran");
});
