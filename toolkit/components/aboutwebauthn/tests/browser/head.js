function set_text(field, text) {
  field.value = text;
  field.dispatchEvent(new Event("input"));
}

async function reset_about_page(doc) {
  let info_text = doc.getElementById("info-text-div");
  let msg = JSON.stringify({ type: "listen-success" });
  let promise = BrowserTestUtils.waitForMutationCondition(
    info_text,
    { attributes: true, attributeFilter: ["hidden"] },
    () => info_text.hidden !== false
  );
  Services.obs.notifyObservers(null, "about-webauthn-prompt", msg);
  await promise;
}

async function send_auth_info_and_check_categories(doc, ops) {
  let info_text = doc.getElementById("info-text-div");
  let msg = JSON.stringify({
    type: "selected-device",
    auth_info: { options: ops },
  });

  let promise = BrowserTestUtils.waitForMutationCondition(
    info_text,
    { attributes: true, attributeFilter: ["hidden"] },
    () => info_text.hidden
  );
  Services.obs.notifyObservers(null, "about-webauthn-prompt", msg);
  await promise;

  // Info should be shown always, so we use it as a canary
  let info_tab_button = doc.getElementById("info-tab-button");
  isnot(
    info_tab_button.style.display,
    "none",
    "Info button in the sidebar not visible"
  );
}
