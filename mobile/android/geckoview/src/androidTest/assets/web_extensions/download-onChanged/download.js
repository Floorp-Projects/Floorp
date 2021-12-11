async function test() {
  browser.downloads.onChanged.addListener(async delta => {
    const changes = { current: {}, previous: {} };
    changes.id = delta.id;
    delete delta.id;
    for (const prop in delta) {
      changes.current[prop] = delta[prop].current;
      changes.previous[prop] = delta[prop].previous;
    }
    await browser.runtime.sendNativeMessage("browser", changes);
  });

  await browser.downloads.download({
    url: "http://localhost:4245/assets/www/images/test.gif",
  });
}

test();
