browser.tabs.query({ url: "*://*/*?tabToClose" }).then(([tab]) => {
  browser.tabs.remove(tab.id);
});
