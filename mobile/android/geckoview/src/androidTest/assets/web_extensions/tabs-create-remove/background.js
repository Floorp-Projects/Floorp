browser.tabs.create({}).then(tab => {
  browser.tabs.remove(tab.id);
});
