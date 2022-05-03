window.onload = () => {
  document.body.addEventListener("click", requestPermissions);
  async function requestPermissions() {
    const perms = {
      permissions: ["geolocation"],
      origins: ["*://example.com/*"],
    };
    const response = await browser.permissions.request(perms);
    browser.runtime.sendNativeMessage("browser", `${response}`);
  }
};
