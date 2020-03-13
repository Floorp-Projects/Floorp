browser.tabs.onActivated.addListener(async tabChange => {
  const activeTabs = await browser.tabs.query({ active: true });
  const currentWindow = await browser.tabs.query({
    currentWindow: true,
    active: true,
  });

  if (
    activeTabs.length === 1 &&
    activeTabs[0].id == tabChange.tabId &&
    currentWindow.length === 1 &&
    currentWindow[0].id === tabChange.tabId
  ) {
    browser.tabs.remove(tabChange.tabId);
  }
});
