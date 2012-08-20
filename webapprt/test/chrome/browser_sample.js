// This is a sample WebappRT chrome test.  It's just a browser-chrome mochitest.

Cu.import("resource://webapprt/modules/WebappRT.jsm");

function test() {
  waitForExplicitFinish();
  ok(true, "true is true!");
  loadWebapp("sample.webapp", undefined, function onLoad() {
    is(document.documentElement.getAttribute("title"),
       WebappRT.config.app.manifest.name,
       "Window title should be webapp name");
    let msg = gAppBrowser.contentDocument.getElementById("msg");
    var observer = new MutationObserver(function (mutations) {
      ok(/^Webapp getSelf OK:/.test(msg.textContent),
         "The webapp should have successfully installed and updated its msg");
      finish();
    });
    observer.observe(msg, { childList: true });
  });
}
