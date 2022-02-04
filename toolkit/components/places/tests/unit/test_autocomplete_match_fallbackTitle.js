/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is a test for the fallbackTitle argument of autocomplete_match.

add_task(async function test_match() {
  async function search(text) {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute(
      `
      SELECT AUTOCOMPLETE_MATCH(:text, 'http://mozilla.org/', 'Main title',
                                NULL, NULL, 1, 1, NULL,
                                :matchBehavior, :searchBehavior,
                                'Fallback title')
      `,
      {
        text,
        matchBehavior: Ci.mozIPlacesAutoComplete.MATCH_ANYWHERE,
        searchBehavior: 643,
      }
    );
    return !!rows[0].getResultByIndex(0);
  }
  Assert.ok(await search("mai"), "Match on main title");
  Assert.ok(await search("fall"), "Match on fallback title");
});
