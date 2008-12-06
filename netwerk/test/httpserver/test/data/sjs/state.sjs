function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  var oldval = getState("foopy");
  response.setHeader("X-Old-Value", oldval, false);

  var newval = "ERROR NOT SET";
  switch (request.queryString)
  {
    case "1":
      newval = "first set!";
      break;

    case "2":
      newval = "changed!";
      break;

    case "3":
      newval = "done!";
      break;

    default:
      throw "Ruh-roh!  " + request.queryString;
  }

  setState("foopy", newval);
  response.setHeader("X-New-Value", newval, false);
}
