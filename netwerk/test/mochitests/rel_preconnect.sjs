// Generate response header "Link: <HREF>; rel=preconnect"
// HREF is provided by the request header X-Link

function handleRequest(request, response)
{
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Link", "<" + 
                       request.getHeader('X-Link') +
                       ">; rel=preconnect" + ", " +
                        "<" + 
                       request.getHeader('X-Link') +
                       ">; rel=preconnect; crossOrigin=anonymous");
    response.write("check that header");
}

