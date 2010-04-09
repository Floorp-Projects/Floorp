/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/*
 * This test validates that OpenSSL running on this system can
 * validate the server certificate in crashreports.crt against the
 * list of CA certificates in crashreporter.crt. On Maemo systems,
 * OpenSSL has a bug where the ordering of certain certificates
 * in the CA certificate file can cause validation to fail.
 * This test is intended to catch that condition in case the NSS
 * certificate list changes in a way that would trigger the bug.
 */
function run_test() {
  let file = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath("/bin/sh");

  let process = Components.classes["@mozilla.org/process/util;1"]
                        .createInstance(Components.interfaces.nsIProcess);
  process.init(file);

  let shscript = do_get_file("opensslverify.sh");
  let cacerts = do_get_file("crashreporter.crt");
  let servercert = do_get_file("crashreports.crt");
  let args = [shscript.path, cacerts.path, servercert.path];
  process.run(true, args, args.length);

  dump('If the following test fails, the logic in toolkit/crashreporter/client/certdata2pem.py needs to be fixed, otherwise crash report submission on Maemo will fail.\n');
  do_check_eq(process.exitValue, 0);
}
