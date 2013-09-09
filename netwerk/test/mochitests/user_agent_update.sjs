
function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "application/json", false);

  // used by test_user_agent_updates test
  response.write(decodeURIComponent(request.queryString));
}

