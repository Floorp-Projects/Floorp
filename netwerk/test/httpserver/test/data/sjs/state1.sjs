function parseQueryString(str)
{
  var paramArray = str.split("&");
  var regex = /^([^=]+)=(.*)$/;
  var params = {};
  for (var i = 0, sz = paramArray.length; i < sz; i++)
  {
    var match = regex.exec(paramArray[i]);
    if (!match)
      throw "Bad parameter in queryString!  '" + paramArray[i] + "'";
    params[decodeURIComponent(match[1])] = decodeURIComponent(match[2]);
  }

  return params;
}

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  var params = parseQueryString(request.queryString);

  var oldShared = getSharedState("shared-value");
  response.setHeader("X-Old-Shared-Value", oldShared, false);

  var newShared = params.newShared;
  if (newShared !== undefined)
  {
    setSharedState("shared-value", newShared);
    response.setHeader("X-New-Shared-Value", newShared, false);
  }

  var oldPrivate = getState("private-value");
  response.setHeader("X-Old-Private-Value", oldPrivate, false);

  var newPrivate = params.newPrivate;
  if (newPrivate !== undefined)
  {
    setState("private-value", newPrivate);
    response.setHeader("X-New-Private-Value", newPrivate, false);
  }
}
