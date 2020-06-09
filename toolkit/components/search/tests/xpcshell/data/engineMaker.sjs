/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Dynamically create a search engine offering search suggestions via searchSuggestions.sjs.
 *
 * The engine is constructed by passing a JSON object with engine datails as the query string.
 */

function handleRequest(request, response) {
  let engineData = JSON.parse(unescape(request.queryString).replace("+", " "));

  if (!engineData.baseURL) {
    response.setStatusLine(request.httpVersion, 500, "baseURL required");
    return;
  }

  engineData.name = engineData.name || "Generated test engine";
  engineData.description = engineData.description || "Generated test engine description";
  engineData.method = engineData.method || "GET";

  response.setStatusLine(request.httpVersion, 200, "OK");
  createOpenSearchEngine(response, engineData);
}

/**
 * Create an OpenSearch engine for the given base URL.
 */
function createOpenSearchEngine(response, engineData) {
  let params = "", queryString = "";
  if (engineData.method == "POST") {
    params = "<Param name='q' value='{searchTerms}'/>";
  } else {
    queryString = "?q={searchTerms}";
  }
  let type = "type='application/x-suggestions+json'";
  if (engineData.alternativeJSONType) {
    type = "type='application/json' rel='suggestions'";
  }
  let image = "";
  if (engineData.image) {
    image = `<Image width="16" height="16">${engineData.baseURL}${engineData.image}</Image>`;
  }

  let result = `<?xml version='1.0' encoding='utf-8'?>
<OpenSearchDescription xmlns='http://a9.com/-/spec/opensearch/1.1/'>
  <ShortName>${engineData.name}</ShortName>
  <Description>${engineData.description}</Description>
  <InputEncoding>UTF-8</InputEncoding>
  <LongName>${engineData.name}</LongName>
  ${image}
  <Url ${type} method='${engineData.method}'
       template='${engineData.baseURL}searchSuggestions.sjs${queryString}'>
    ${params}
  </Url>
  <Url type='text/html' method='${engineData.method}'
       template='${engineData.baseURL}${queryString}'/>
</OpenSearchDescription>
`;
  response.write(result);
}
