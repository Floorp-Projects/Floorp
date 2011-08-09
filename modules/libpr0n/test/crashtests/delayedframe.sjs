function getFileStream(filename)
{
  // Get the location of this sjs file, and then use that to figure out where
  // to find where our other files are.
  var self = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  self.initWithPath(getState("__LOCATION__"));
  var file = self.parent;
  file.append(filename);
  dump(file.path + "\n");

  var fileStream = Components.classes['@mozilla.org/network/file-input-stream;1']
                     .createInstance(Components.interfaces.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);

  return fileStream;
}

var gTimer;

function handleRequest(request, response)
{
  response.processAsync();
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/gif", false);

  var firststream = getFileStream("threeframes-start.gif");
  response.bodyOutputStream.writeFrom(firststream, firststream.available())
  firststream.close();

  gTimer = Components.classes["@mozilla.org/timer;1"].createInstance(Components.interfaces.nsITimer);
  gTimer.initWithCallback(function()
  {
    var secondstream = getFileStream("threeframes-end.gif");
    response.bodyOutputStream.writeFrom(secondstream, secondstream.available())
    secondstream.close();
    response.finish();

    // This time needs to be longer than the animation timer in
    // threeframes-start.gif. That's specified as 100ms; just use 5 seconds as
    // a reasonable upper bound. Since this is just a crashtest, timeouts
    // aren't a big deal.
  }, 5 * 1000 /* milliseconds */, Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}
