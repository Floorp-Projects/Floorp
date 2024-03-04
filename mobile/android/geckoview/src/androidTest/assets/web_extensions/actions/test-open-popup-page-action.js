window.addEventListener("DOMContentLoaded", init);

function init() {
  document.body.addEventListener("click", () => {
    browser.pageAction.openPopup();
  });
}
