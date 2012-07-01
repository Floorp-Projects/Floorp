// This is a sample WebappRT chrome test.  It's just a browser-chrome mochitest.

function test() {
  waitForExplicitFinish();
  ok(true, "true is true!");
  installWebapp("sample.webapp", undefined, function onInstall(appConfig) {
    is(document.documentElement.getAttribute("title"),
       appConfig.app.manifest.name,
       "Window title should be webapp name");
    let content = document.getElementById("content");
    let msg = content.contentDocument.getElementById("msg");
    var observer = new MutationObserver(function (mutations) {
      ok(/^Webapp getSelf OK:/.test(msg.textContent),
         "The webapp should have successfully installed and updated its msg");
      finish();
    });
    observer.observe(msg, { childList: true });
  });
}
