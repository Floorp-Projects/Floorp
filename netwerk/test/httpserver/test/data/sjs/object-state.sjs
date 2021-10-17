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

/*
 * We're relying somewhat dubiously on all data being sent as soon as it's
 * available at numerous levels (in Necko in the server-side part of the
 * connection, in the OS's outgoing socket buffer, in the OS's incoming socket
 * buffer, and in Necko in the client-side part of the connection), but to the
 * best of my knowledge there's no way to force data flow at all those levels,
 * so this is the best we can do.
 */
function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  
  /*
   * NB: A Content-Type header is *necessary* to avoid content-sniffing, which
   *     will delay onStartRequest past the the point where the entire head of
   *     the response has been received.
   */
  response.setHeader("Content-Type", "text/plain", false);

  var params = parseQueryString(request.queryString);

  switch (params.state)
  {
    case "initial":
      response.processAsync();
      response.write("do");
      var state =
        {
          QueryInterface: function(iid)
          {
            if (iid.equals(Components.interfaces.nsISupports))
              return this;
            throw Components.results.NS_ERROR_NO_INTERFACE;
          },
          end: function()
          {
            response.write("ne");
            response.finish();
          }
        };
      state.wrappedJSObject = state;
      setObjectState("object-state-test", state);
      getObjectState("object-state-test", function(obj)
      {
        if (obj !== state)
        {
          response.write("FAIL bad state save");
          response.finish();
        }
      });
      break;

    case "intermediate":
      response.write("intermediate");
      break;

    case "trigger":
      response.write("trigger");
      getObjectState("object-state-test", function(obj)
      {
        obj.wrappedJSObject.end();
        setObjectState("object-state-test", null);
      });
      break;

    default:
      response.setStatusLine(request.httpVersion, 500, "Unexpected State");
      response.write("Bad state: " + params.state);
      break;
  }
}
