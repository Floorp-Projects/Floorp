add_task(async function check_fluent_ids() {
  await document.l10n.ready;
  MozXULElement.insertFTLIfNeeded("toolkit/main-window/autocomplete.ftl");

  const host = "testhost.com";
  for (const browser of ChromeUtils.import(
    "resource:///modules/ChromeMigrationUtils.jsm"
  ).ChromeMigrationUtils.CONTEXTUAL_LOGIN_IMPORT_BROWSERS) {
    const id = `autocomplete-import-logins-${browser}`;
    const message = await document.l10n.formatValue(id, { host });
    ok(message.includes(`data-l10n-name="line1"`), `${id} included line1`);
    ok(message.includes(`data-l10n-name="line2"`), `${id} included line2`);
    ok(message.includes(host), `${id} replaced host`);
  }
});
