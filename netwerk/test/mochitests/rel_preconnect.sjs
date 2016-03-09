// Generate response header "Link: <HREF>; rel=preconnect"
// HREF is provided by the request header X-Link

function handleRequest(request, response)
{
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Link", "<" + 
                       request.queryString +
                       ">; rel=preconnect" + ", " +
                        "<" + 
                       request.queryString +
                       ">; rel=preconnect; crossOrigin=anonymous");
    response.write("check that header");
}

