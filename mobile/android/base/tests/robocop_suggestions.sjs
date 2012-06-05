/**
 * Used with testSearchSuggestions.
 * Returns a set of pre-defined suggestions for given prefixes.
 */

function handleRequest(request, response) {
  let query = request.queryString.match(/^query=(.*)$/)[1];
  query = decodeURIComponent(query).replace(/\+/g, " ");

  let suggestMap = {
    "f":       ["facebook", "fandango", "frys", "forever 21", "fafsa"],
    "fo":      ["forever 21", "food network", "fox news", "foothill college", "fox"],
    "foo":     ["food network", "foothill college", "foot locker", "footloose", "foo fighters"],
    "foo ":    ["foo fighters", "foo bar", "foo bat", "foo bay"],
    "foo b":   ["foo bar", "foo bat", "foo bay"],
    "foo ba":  ["foo bar", "foo bat", "foo bay"],
    "foo bar": ["foo bar"]
  };

  let suggestions = suggestMap[query];
  if (!suggestions)
    suggestions = [];
  suggestions = [query, suggestions];

  /*
   * Sample result:
   * ["foo",["food network","foothill college","foot locker",...]]
   */
  response.setHeader("Content-Type", "text/json", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(JSON.stringify(suggestions));
}
