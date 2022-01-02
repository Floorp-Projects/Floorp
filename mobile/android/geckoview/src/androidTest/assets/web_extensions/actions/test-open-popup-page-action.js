window.addEventListener("DOMContentLoaded", init);

function init() {
  document.body.addEventListener("click", event => {
    browser.pageAction.openPopup();
  });
}
