const Cc = Components.classes;
const Ci = Components.interfaces;
const CC = Components.Constructor;

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                              "nsIBinaryInputStream",
                              "setInputStream");

function parseHeaders(data, start)
{
  let headers = {};

  while (true) {
    let done = false;
    let end = data.indexOf("\r\n", start);
    if (end == -1) {
      done = true;
      end = data.length;
    }
    let line = data.substring(start, end);
    start = end + 2;
    if (line == "")
      // empty line, we're done
      break;

    //XXX: this doesn't handle multi-line headers. do we care?
    let [name, value] = line.split(':');
    //XXX: not normalized, should probably use nsHttpHeaders or something
    headers[name] = value.trimLeft();
  }
  return [headers, start];
}

function parseMultipartForm(request)
{
  let boundary = null;
  // See if this is a multipart/form-data request, and if so, find the
  // boundary string
  if (request.hasHeader("Content-Type")) {
    var contenttype = request.getHeader("Content-Type");
    var bits = contenttype.split(";");
    if (bits[0] == "multipart/form-data") {
      for (var i = 1; i < bits.length; i++) {
        var b = bits[i].trimLeft();
        if (b.indexOf("boundary=") == 0) {
          // grab everything after boundary=
          boundary = "--" + b.substring(9);
          break;
        }
      }
    }
  }
  if (boundary == null)
    return null;

  let body = new BinaryInputStream(request.bodyInputStream);
  let avail;
  let bytes = [];
  while ((avail = body.available()) > 0) {
    let readBytes = body.readByteArray(avail);
    for (let b of readBytes) {
      bytes.push(b);
    }
  }
  let data = "";
  for (let b of bytes) {
    data += String.fromCharCode(b);
  }
  let formData = {};
  let done = false;
  let start = 0;
  while (true) {
    // read first line
    let end = data.indexOf("\r\n", start);
    if (end == -1) {
      done = true;
      end = data.length;
    }

    let line = data.substring(start, end);
    // look for closing boundary delimiter line
    if (line == boundary + "--") {
      break;
    }

    if (line != boundary) {
      dump("expected boundary line but didn't find it!");
      break;
    }

    // parse headers
    start = end + 2;
    let headers = null;
    [headers, start] = parseHeaders(data, start);

    // find next boundary string
    end = data.indexOf("\r\n" + boundary, start);
    if (end == -1) {
      dump("couldn't find next boundary string\n");
      break;
    }

    // read part data, stick in formData using Content-Disposition header
    let part = data.substring(start, end);
    start = end + 2;

    if ("Content-Disposition" in headers) {
      let bits = headers["Content-Disposition"].split(';');
      if (bits[0] == 'form-data') {
        for (let i = 0; i < bits.length; i++) {
          let b = bits[i].trimLeft();
          if (b.indexOf('name=') == 0) {
            //TODO: handle non-ascii here?
            let name = b.substring(6, b.length - 1);
            //TODO: handle multiple-value properties?
            formData[name] = part;
          }
          //TODO: handle filename= ?
          //TODO: handle multipart/mixed for multi-file uploads?
        }
      }
    }
  }
  return formData;
}

function handleRequest(request, response)
{
  if (request.method == "GET") {
    let id = null;
    for each(p in request.queryString.split('&')) {
      let [key, value] = p.split('=');
      if (key == 'id')
        id = value;
    }
    if (id == null) {
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      response.write("Missing id parameter");
    }
    else {
      let data = getState(id);
      if (data == "") {
        response.setStatusLine(request.httpVersion, 404, "Not Found");
        response.write("Not Found");
      }
      else {
        response.setHeader("Content-Type", "text/plain", false);
        response.write(data);
      }
    }
  }
  else if (request.method == "POST") {
    let formData = parseMultipartForm(request);

    if (formData && 'upload_file_minidump' in formData) {
      response.setHeader("Content-Type", "text/plain", false);

      let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator);
      let uuid = uuidGenerator.generateUUID().toString();
      // ditch the {}, add bp- prefix
      uuid = 'bp-' + uuid.substring(1,uuid.length-2);

      let d = JSON.stringify(formData);
      //dump('saving crash report ' + uuid + ': ' + d + '\n');
      setState(uuid, d);

      response.write("CrashID=" + uuid + "\n");
    }
    else {
      dump('*** crashreport.sjs: Malformed request?\n');
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      response.write("Missing minidump file");
    }
  }
  else {
    response.setStatusLine(request.httpVersion, 405, "Method not allowed");
    response.write("Can't handle HTTP method " + request.method);
  }
}
