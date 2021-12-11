/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const testData = {
  "http://foo.com/": ["tag1", "tag 2", "Space ☺️ Between"].sort(),
  "http://bar.com/": ["tag1", "tag 2"].sort(),
  "http://baz.com/": ["tag 2", "Space ☺️ Between"].sort(),
  "http://qux.com/": ["Space ☺️ Between"],
};

const formattedTestData = [];
for (const [uri, tagArray] of Object.entries(testData)) {
  formattedTestData.push({
    title: `Title of ${uri}`,
    uri,
    isBookmark: true,
    isTag: true,
    tagArray,
  });
}

add_task(async function test_results_as_tags_root() {
  await task_populateDB(formattedTestData);

  // Construct URL - tag mapping from tag query.
  const actualData = {};
  for (const uri in testData) {
    if (testData.hasOwnProperty(uri)) {
      actualData[uri] = [];
    }
  }

  const options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_TAGS_ROOT;
  const query = PlacesUtils.history.getNewQuery();
  const root = PlacesUtils.history.executeQuery(query, options).root;

  root.containerOpen = true;
  Assert.equal(root.childCount, 3, "We should get as many results as tags.");
  displayResultSet(root);

  for (let i = 0; i < root.childCount; ++i) {
    const node = root.getChild(i);
    const tagName = node.title;
    Assert.equal(
      node.type,
      node.RESULT_TYPE_QUERY,
      "Result type should be RESULT_TYPE_QUERY."
    );
    const subRoot = node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
    subRoot.containerOpen = true;
    for (let j = 0; j < subRoot.childCount; ++j) {
      actualData[subRoot.getChild(j).uri].push(tagName);
      actualData[subRoot.getChild(j).uri].sort();
    }
  }

  Assert.deepEqual(
    actualData,
    testData,
    "URI-tag mapping should be same from query and initial data."
  );
});
