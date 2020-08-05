/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function channelname() {
  var file = new File(
    [new Blob(["test"], { type: "text/plain" })],
    "test-name"
  );
  var url = URL.createObjectURL(file);
  var channel = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });

  let inputStream = channel.open();
  ok(inputStream, "Should be able to open channel");
  ok(
    inputStream.QueryInterface(Ci.nsIAsyncInputStream),
    "Stream should support async operations"
  );

  await new Promise(resolve => {
    inputStream.asyncWait(
      () => {
        let available = inputStream.available();
        ok(available, "There should be data to read");
        Assert.equal(
          channel.contentDispositionFilename,
          "test-name",
          "filename matches"
        );
        resolve();
      },
      0,
      0,
      Services.tm.mainThread
    );
  });

  inputStream.close();
  channel.cancel(Cr.NS_ERROR_FAILURE);
});
