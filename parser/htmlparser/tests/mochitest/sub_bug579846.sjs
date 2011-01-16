function handleRequest(request, response)
{
  var date = new Date();
  var now = date.getTime();
  date.setTime(date.getTime() + 5 * 60 * 1000);
  var exp = (date).toGMTString();

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Expires", exp, false);

  response.write(
    "<html>\n" +
    "<head><meta http-equiv='Cache-control' content='no-cache'></head>\n" +
    "<body onload='parent.testFrameOnLoad(document.body.innerHTML)'>" + now + "</body>" +
    "</html>"
  );
}
