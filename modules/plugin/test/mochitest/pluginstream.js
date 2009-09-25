  SimpleTest.waitForExplicitFinish();

  function frameLoaded() {
    var testframe = document.getElementById('testframe');
    var embed = document.getElementsByTagName('embed')[0];
    var content = testframe.contentDocument.body.innerHTML;
    if (!content.length)
      return;

    var filename = embed.getAttribute("src") ||
        embed.getAttribute("geturl") ||
        embed.getAttribute("geturlnotify");
    
    var req = new XMLHttpRequest();
    req.open('GET', filename, false);
    req.overrideMimeType('text/plain; charset=x-user-defined');
    req.send(null);
    is(req.status, 200, "bad XMLHttpRequest status");
    is(content, req.responseText.replace(/\r\n/g, "\n"),
       "content doesn't match");
    SimpleTest.finish();
  }
