const PR_RDONLY = 0x1;

var etld = Cc["@mozilla.org/network/effective-tld-service;1"]
             .getService(Ci.nsIEffectiveTLDService);
var idn = Cc["@mozilla.org/network/idn-service;1"]
             .getService(Ci.nsIIDNService);

function run_test()
{
  var fis = Cc["@mozilla.org/network/file-input-stream;1"]
              .createInstance(Ci.nsIFileInputStream);
  fis.init(do_get_file("effective_tld_names.dat"),
           PR_RDONLY, 0o444, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  var lis = Cc["@mozilla.org/intl/converter-input-stream;1"]
              .createInstance(Ci.nsIConverterInputStream);
  lis.init(fis, "UTF-8", 1024, 0);
  lis.QueryInterface(Ci.nsIUnicharLineInputStream);

  var out = { value: "" };
  do
  {
    var more = lis.readLine(out);
    var line = out.value;

    line = line.replace(/^\s+/, "");
    var firstTwo = line.substring(0, 2); // a misnomer, but whatever
    if (firstTwo == "" || firstTwo == "//")
      continue;

    var space = line.search(/[ \t]/);
    line = line.substring(0, space == -1 ? line.length : space);

    if ("*." == firstTwo)
    {
      let rest = line.substring(2);
      checkPublicSuffix("foo.SUPER-SPECIAL-AWESOME-PREFIX." + rest,
                        "SUPER-SPECIAL-AWESOME-PREFIX." + rest);
    }
    else if ("!" == line.charAt(0))
    {
      checkPublicSuffix(line.substring(1),
                        line.substring(line.indexOf(".") + 1));
    }
    else
    {
      checkPublicSuffix("SUPER-SPECIAL-AWESOME-PREFIX." + line, line);
    }
  }
  while (more);
}

function checkPublicSuffix(host, expectedSuffix)
{
  expectedSuffix = idn.convertUTF8toACE(expectedSuffix).toLowerCase();
  var actualSuffix = etld.getPublicSuffixFromHost(host);
  do_check_eq(actualSuffix, expectedSuffix);
}
