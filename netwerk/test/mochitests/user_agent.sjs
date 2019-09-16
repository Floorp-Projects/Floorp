
function handleRequest(request, response)
{
    // avoid confusing cache behaviors
    response.setHeader("Cache-Control", "no-cache", false);
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);

    // used by test_user_agent tests
    response.write(
      "<html><body>\
        <script type='text/javascript'>\
          var msg = {\
            header: '" + request.getHeader('User-Agent') + "',\
            nav: navigator.userAgent\
          };\
          self.parent.postMessage(msg, '*');\
        </script>\
      </body></html>"
    );
}
