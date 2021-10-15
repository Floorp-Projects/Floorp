function handleRequest(request, response)
{
  var file;
  getObjectState("SERVER_ROOT", function(serverRoot)
  {
    var ref = request.getHeader("Referer").split("?")[0];
    // 8 is "https://".length which is the longest string before the host.
    var pathStart = ref.indexOf("/", 8) + 1;
    var pathEnd = ref.lastIndexOf("/") + 1;
    file = serverRoot.getFile(ref.substring(pathStart, pathEnd) + "x");
  });

  response.setHeader("Content-Type", "text/plain", false);
  response.write(file.path.substr(0, file.path.length-1));
}
