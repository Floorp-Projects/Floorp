browser.browsingData.removeDownloads({ since: 10001 });
browser.browsingData.removeFormData({ since: 10002 });
browser.browsingData.removeHistory({ since: 10003 });
browser.browsingData.removePasswords({ since: 10004 });
browser.browsingData.remove(
  { since: 10005 },
  { downloads: true, cookies: true }
);
