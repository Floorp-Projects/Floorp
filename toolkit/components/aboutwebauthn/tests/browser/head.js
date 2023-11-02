async function reset_about_page(doc) {
  let info_text = doc.getElementById("info-text-div");
  let msg = JSON.stringify({ action: "listen-finished-success" });
  let promise = BrowserTestUtils.waitForMutationCondition(
    info_text,
    { attributes: true, attributeFilter: ["hidden"] },
    () => info_text.hidden !== false
  );
  Services.obs.notifyObservers(null, "about-webauthn-prompt", msg);
  await promise;
}
