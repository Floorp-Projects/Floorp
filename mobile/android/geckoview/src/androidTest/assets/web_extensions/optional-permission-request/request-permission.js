window.onload = () => {
  document.body.addEventListener("click", async () => {
    const perms = {
      permissions: ["activeTab"],
      origins: ["*://example.com/*"],
    };
    const response = await browser.permissions.request(perms);
    browser.runtime.sendNativeMessage("browser", `${response}`);
  });
};
