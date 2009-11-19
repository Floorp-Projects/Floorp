const Ci = Components.interfaces;

function handleRequest(request, response)
{
  var exstr, qid;

  response.setStatusLine(request.httpVersion, 500, "FAIL");

  var passed = false;
  try
  {
    qid = request.QueryInterface(Ci.nsIHttpRequest);
    passed = qid === request;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "request doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "request QI'd wrongly?");
    return;
  }

  passed = false;
  try
  {
    qid = response.QueryInterface(Ci.nsIHttpResponse);
    passed = qid === response;
  }
  catch (e)
  {
    exstr = ("" + e).split(/[\x09\x20-\x7f\x81-\xff]+/)[0];
    response.setStatusLine(request.httpVersion, 500,
                           "response doesn't QI: " + exstr);
    return;
  }
  if (!passed)
  {
    response.setStatusLine(request.httpVersion, 500, "response QI'd wrongly?");
    return;
  }

  response.setStatusLine(request.httpVersion, 200, "SJS QI Tests Passed");
}
