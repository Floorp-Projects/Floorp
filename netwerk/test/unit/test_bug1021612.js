/*
 * Bug 1021612 test.
 */

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newChannel(url, "", null);
}

function checkSuccess(request, buffer)
{
  var body = '';
  for (var i = 0; i < 3786; ++i)
    body += '.';

  do_check_eq(buffer, body);
  do_test_finished();
}

function run_test()
{
  var profile = do_get_profile();
  profile.append('cache2');
  profile.append('entries');
  var testFile = do_get_file('../unit/data/4k-cache-file');
  testFile.copyTo(profile, '895F51A24BCC63EE6D69B3AAF0007975F33708A7');
  var chan = make_channel('http://localhost/4k-cache-file.php');
  chan.loadFlags = chan.loadFlags | Ci.nsIRequest.LOAD_FROM_CACHE;
  chan.asyncOpen(new ChannelListener(checkSuccess, null),
                 null);
  do_test_pending();
}
