(function() {
  const MESSAGE = "damp@mozilla.org:html-loaded";

  addEventListener("load", function onload(e) {
    if (e.target.documentURI.indexOf("chrome://damp/content/damp.html")) {
      return;
    }
    removeEventListener("load", onload);

    sendAsyncMessage(MESSAGE);
  }, true);
})();
