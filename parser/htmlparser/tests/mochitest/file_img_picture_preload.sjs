// Return a PNG, saving an array of query strings we see as state. When query
// string is 'status', return array as JSON

function handleRequest(request, response)
{
  var seenImages = getState("seenImages");
  seenImages = seenImages ? JSON.parse(seenImages) : [];

  response.setHeader("Cache-Control", "must-revalidate", false);

  if (request.queryString == "status") {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write(JSON.stringify(seenImages));
  } else if (request.queryString == "reset") {
    // Respond with how many requests we had seen, drop them
    response.setHeader("Content-Type", "text/plain", false);
    response.write(String(seenImages.length));
    seenImages = [];
  } else {
    // Return an image
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", "blue.png", false);
    dump(request.queryString + '\n');
    seenImages.push(request.queryString);
  }

  setState("seenImages", JSON.stringify(seenImages));
}
