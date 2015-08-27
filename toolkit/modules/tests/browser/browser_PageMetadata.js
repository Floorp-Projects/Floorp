/**
 * Tests PageMetadata.jsm, which extracts metadata and microdata from a
 * document.
 */

let {PageMetadata} = Cu.import("resource://gre/modules/PageMetadata.jsm", {});

let rootURL = "http://example.com/browser/toolkit/modules/tests/browser/";

function promiseDocument(fileName) {
  let url = rootURL + fileName;

  return new Promise((resolve, reject) => {
    let xhr = new XMLHttpRequest();
    xhr.onload = () => resolve(xhr.responseXML);
    xhr.onerror = () => reject(new Error("Error loading document"));
    xhr.open("GET", url);
    xhr.responseType = "document";
    xhr.send();
  });
}

/**
 * Load a simple document.
 */
add_task(function* simpleDoc() {
  let fileName = "metadata_simple.html";
  info(`Loading a simple page, ${fileName}`);

  let doc = yield promiseDocument(fileName);
  Assert.notEqual(doc, null,
                  "Should have a document to analyse");

  let data = PageMetadata.getData(doc);
  Assert.notEqual(data, null,
                  "Should have non-null result");
  Assert.equal(data.url, rootURL + fileName,
               "Should have expected url property");
  Assert.equal(data.title, "Test Title",
               "Should have expected title property");
  Assert.equal(data.description, "A very simple test page",
               "Should have expected title property");
});

add_task(function* titlesDoc() {
  let fileName = "metadata_titles.html";
  info(`Loading titles page, ${fileName}`);

  let doc = yield promiseDocument(fileName);
  Assert.notEqual(doc, null,
                  "Should have a document to analyse");

  let data = PageMetadata.getData(doc);
  Assert.notEqual(data, null,
                  "Should have non-null result");
  Assert.equal(data.title, "Test Titles",
               "Should use the page title, not the open graph title");
});

add_task(function* titlesFallbackDoc() {
  let fileName = "metadata_titles_fallback.html";
  info(`Loading titles page, ${fileName}`);

  let doc = yield promiseDocument(fileName);
  Assert.notEqual(doc, null,
                  "Should have a document to analyse");

  let data = PageMetadata.getData(doc);
  Assert.notEqual(data, null,
                  "Should have non-null result");
  Assert.equal(data.title, "Title",
               "Should use the open graph title");
});
