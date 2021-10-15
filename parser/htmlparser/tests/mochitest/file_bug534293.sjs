function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/javascript", false);
  if (request.queryString.indexOf("report") != -1) {
    if (getState("loaded") == "loaded") {
      response.write("ok(false, 'This script was not supposed to get fetched.');");      
    } else {
      response.write("ok(true, 'This script was not supposed to get fetched.');");      
    }
  } else {
    setState("loaded", "loaded");
    response.write("ok(false, 'This script is not supposed to run.');");
  }
}
