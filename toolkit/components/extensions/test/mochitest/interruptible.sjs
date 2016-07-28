const TEST_DATA = "This is 31 bytes of sample data";
const TOTAL_LEN = TEST_DATA.length;
const PARTIAL_LEN = 15;

// A handler to let us systematically test pausing/resuming/canceling
// of downloads.  This target represents a small text file but a simple
// GET will stall after sending part of the data, to give the test code
// a chance to pause or do other operations on an in-progress download.
// A resumed download (ie, a GET with a Range: header) will allow the
// download to complete.
function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/plain", false);

  if (request.hasHeader("Range")) {
    let start, end;
    let matches = request.getHeader("Range")
        .match(/^\s*bytes=(\d+)?-(\d+)?\s*$/);
    if (matches != null) {
      start = matches[1] ? parseInt(matches[1], 10) : 0;
      end = matches[2] ? pareInt(matchs[2], 10) : (TOTAL_LEN - 1);
    }

    if (end == undefined || end >= TOTAL_LEN) {
      response.setStatusLine(request.httpVersion, 416, "Requested Range Not Satisfiable");
      response.setHeader("Content-Range", `*/${TOTAL_LEN}`, false);
      response.finish();
      return;
    }

    response.setStatusLine(request.httpVersion, 206, "Partial Content");
    response.setHeader("Content-Range", `${start}-${end}/${TOTAL_LEN}`, false);
    response.write(TEST_DATA.slice(start, end + 1));
  } else {
    response.processAsync();
    response.setHeader("Content-Length", `${TOTAL_LEN}`, false);
    response.write(TEST_DATA.slice(0, PARTIAL_LEN));
  }
}
