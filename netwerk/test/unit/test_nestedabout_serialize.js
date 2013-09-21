const BinaryInputStream =
  Components.Constructor("@mozilla.org/binaryinputstream;1",
                         "nsIBinaryInputStream", "setInputStream");
const BinaryOutputStream =
  Components.Constructor("@mozilla.org/binaryoutputstream;1",
                         "nsIBinaryOutputStream", "setOutputStream");

const Pipe =
  Components.Constructor("@mozilla.org/pipe;1", "nsIPipe", "init");

const kNestedAboutCID = "{2f277c00-0eaf-4ddb-b936-41326ba48aae}";

function run_test()
{
  var ios = Cc["@mozilla.org/network/io-service;1"].createInstance(Ci.nsIIOService);

  var baseURI = ios.newURI("http://example.com/", "UTF-8", null);

  // This depends on the redirector for about:license having the
  // nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT flag.
  var aboutLicense = ios.newURI("about:license", "UTF-8", baseURI);

  var pipe = new Pipe(false, false, 0, 0, null);
  var output = new BinaryOutputStream(pipe.outputStream);
  var input = new BinaryInputStream(pipe.inputStream);
  output.QueryInterface(Ci.nsIObjectOutputStream);
  input.QueryInterface(Ci.nsIObjectInputStream);

  output.writeCompoundObject(aboutLicense, Ci.nsIURI, true);
  var copy = input.readObject(true);
  copy.QueryInterface(Ci.nsIURI);

  do_check_eq(copy.asciiSpec, aboutLicense.asciiSpec);
  do_check_true(copy.equals(aboutLicense));
}
