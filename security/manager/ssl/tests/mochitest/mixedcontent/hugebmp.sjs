function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-type", "image/bitmap");
  
  let bmpheader = "\x42\x4D\x36\x10\x0E\x00\x00\x00\x00\x00\x36\x00\x00\x00\x28\x00\x00\x00\x80\x02\x00\x00\xE0\x01\x00\x00\x01\x00\x18\x00\x00\x00\x00\x00\x00\x10\x0E\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
  let bmpdatapiece = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

  response.bodyOutputStream.write(bmpheader, 54);
  // Fill 640*480*3 nulls
  for (let i = 0; i < (640 * 480 * 3) / 64; ++i)
    response.bodyOutputStream.write(bmpdatapiece, 64);
}
