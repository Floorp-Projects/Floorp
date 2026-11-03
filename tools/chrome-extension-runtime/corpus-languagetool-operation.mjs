// SPDX-License-Identifier: MPL-2.0
// The driver sends native user input and observes DOM. It never calls product APIs.
/** @param {import("./network-test-types.ts").LanguageDriver} driver */
export async function runLanguageToolOperation(driver) {
  const { item, snapshot, until, type, click, navigate, pause, url, text } =
    driver;
  /** @param {string} label */
  async function correct(label) {
    await until(
      async () => (await snapshot()).installed === "true",
      "unchanged LanguageTool initialization",
    );
    await click({ selector: "#editor" });
    await pause(500);
    await type(text);
    item.typed = await snapshot();
    const errors = await until(async () => {
      const value = await snapshot();
      item.lastSnapshot = value;
      if (
        value.product.some(
          (e) =>
            /needs-attention/.test(e.cls || "") &&
            /confirm privacy policy/i.test(e.html || ""),
        )
      ) {
        throw Error(
          "Actual product privacy setting did not persist after native welcome confirmation",
        );
      }
      return (
        value.product.some((e) =>
          /lt-toolbar__status-icon--has-\d+-errors/.test(e.cls || ""),
        ) && value
      );
    }, label + " real grammar error indicator");
    await click({ selector: ".lt-toolbar__status-icon" });
    item.suggestions = await until(async () => {
      const value = await snapshot();
      item.lastSnapshot = value;
      return (
        value.product.some(
          (e) =>
            /lt-dialog__fix/.test(e.cls || "") && e.text?.trim() === "goes",
        ) && value
      );
    }, label + " actual correction control");
    await click({ selector: ".lt-dialog__fix", text: "goes" });
    const corrected = await until(async () => {
      const value = await snapshot();
      item.lastSnapshot = value;
      return value.text === "She goes to school." && value;
    }, label + " genuine textarea correction");
    return { errors, corrected };
  }
  item.transformed = await correct("First document");
  item.checks.push(
    "Unchanged LanguageTool flags the authored grammatical error and its actual suggestion control changes the native textarea to 'She goes to school.'",
  );
  await navigate(url + "second");
  item.afterNavigation = await correct("New document");
  item.checks.push(
    "Real document navigation retains native grammar checking and correction",
  );
  return async function verifyRemoval() {
    await navigate(url + "removed");
    await click({ selector: "#editor" });
    await type(text);
    await pause(1500);
    item.afterRemoval = await snapshot();
    if (
      item.afterRemoval.installed ||
      item.afterRemoval.product.length ||
      item.afterRemoval.text !== text
    ) {
      throw Error(
        "Actual uninstall did not restore an unmodified native textarea",
      );
    }
    item.checks.push(
      "Actual uninstall restores the authored textarea without product UI or correction",
    );
  };
}
