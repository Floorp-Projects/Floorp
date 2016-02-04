/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function parseQuery(request, key) {
  var params = request.queryString.split('&');
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
	if (p == key)
	  return true;
    if (p.indexOf(key + "=") == 0)
	  return p.substring(key.length + 1);
	if (p.indexOf("=") < 0 && key == "")
	  return p;
  }
  return false;
}

function handleRequest(request, response) {
  // Pretend to be the type requested from the test
  var type = parseQuery(request, "type");

  response.setHeader("Content-Type", type, false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  response.write("fake video");
}
