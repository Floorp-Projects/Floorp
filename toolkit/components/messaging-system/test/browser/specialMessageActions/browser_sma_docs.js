const TEST_URL =
  "https://example.com/browser/toolkit/components/messaging-system/test/browser/specialMessageActions/SpecialMessageActionSchemas.md";

const { SpecialMessageActionSchemas } = ChromeUtils.import(
  "resource://testing-common/SpecialMessageActionSchemas.js"
);

function getHeadingsFromDocs(docs) {
  const re = /### `(\w+)`/g;
  const found = [];
  let match = 1;
  while (match) {
    match = re.exec(docs);
    if (match) {
      found.push(match[1]);
    }
  }
  return found;
}

add_task(async function test_sma_docs() {
  let request = await fetch(TEST_URL);
  let docs = await request.text();
  let headings = getHeadingsFromDocs(docs);
  for (let action_name of Object.keys(SpecialMessageActionSchemas)) {
    Assert.ok(
      headings.includes(action_name),
      `${action_name} not found in SpecialMessageActionSchemas.md`
    );
  }
});
