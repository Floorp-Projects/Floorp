function fileURL(filename) {
  let ifile = getChromeDir(getResolvedURI(gTestPath));
  ifile.append(filename);
  return Services.io.newFileURI(ifile).spec;
}

function httpURL(filename, host = "https://example.com/") {
  let root = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    host
  );
  return root + filename;
}

function add307(url, host = "https://example.com/") {
  return httpURL("307redirect.sjs?" + url, host);
}
