browser.downloads.download({
  url: "http://localhost:4245/assets/www/images/test.gif",
  filename: "banana.gif",
  method: "POST",
  body: "postbody",
  headers: [
    {
      name: "User-Agent",
      value: "Mozilla Firefox",
    },
  ],
  allowHttpErrors: true,
  conflictAction: "overwrite",
  saveAs: true,
  incognito: true,
});
