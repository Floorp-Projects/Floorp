const TEST_URL =
  "https://example.com/browser/toolkit/components/messaging-system/schemas/SpecialMessageActionSchemas/test/browser/index.md";

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
  const schemaTypes = (await fetchSMASchema).anyOf.map(
    s => s.properties.type.enum[0]
  );
  for (let schemaType of schemaTypes) {
    Assert.ok(
      headings.includes(schemaType),
      `${schemaType} not found in SpecialMessageActionSchemas/index.md`
    );
  }
});
