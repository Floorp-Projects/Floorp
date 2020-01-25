browser.tabs.onActivated.addListener(async tabChange => {
  const activeTabs = await browser.tabs.query({ active: true });
  if (activeTabs.length === 1 && activeTabs[0].id == tabChange.tabId) {
    browser.tabs.remove(tabChange.tabId);
  }
});
