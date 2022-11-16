function handleRequest(request, response) {
  var hosts = getState("hosts");
  hosts = hosts ? JSON.parse(hosts) : [];

  if (request.queryString == "result") {
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/json", false);
    response.write(JSON.stringify(hosts));

    setState("hosts", "[]");
  } else {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", "blue.png", false);

    hosts.push(request.host);
    setState("hosts", JSON.stringify(hosts));
  }
}
