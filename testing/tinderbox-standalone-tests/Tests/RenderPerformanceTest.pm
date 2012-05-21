# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


#
# Trender test
#

sub RenderPerformanceTest {
  my ($test_name, $build_dir, $binary_dir, $args) = @_;
  my $render_test_result;
  my $render_time;
  my $render_gfx_time;
  my $render_details;
  my $binary_log = "$build_dir/$test_name.log";
  my $url;

  # Find Trender.xml
  if (-f "/cygdrive/c/builds/tinderbox/Trender/Trender.xml") {
    $url = "file:///C:/builds/tinderbox/Trender/Trender.xml#tinderbox=1";
  } elsif (-f "/builds/tinderbox/Trender/Trender.xml") {
    $url = "file:///builds/tinderbox/Trender/Trender.xml#tinderbox=1";
  } else {
    Util::print_log("TinderboxPrint:Trender:[NOTFOUND]\n");
    return 'testfailed';
  }

  # Settle OS.
  system('/bin/sh -c "sync; sleep 5"');

  $render_test_result = FileBasedTest($test_name, $build_dir, $binary_dir,
                                      [@$args, $url],
                                      $Settings::RenderTestTimeout,
                                      "_x_x_mozilla_trender", 1, 1);

  # double check to make sure the test didn't really succeed
  # even though the scripts think it failed.  Prevents various breakage
  # (e.g. when a timeout happens on the mac, killing the process returns
  # a bogus result code).  FileBasedTest checks the status code
  # before the token
  my $found_token = file_has_token($binary_log, "_x_x_mozilla_trender");
  if ($found_token) {
    $render_test_result = 'success';
  }

  if ($render_test_result eq 'testfailed') {
    Util::print_log("TinderboxPrint:Trender:[FAILED]\n");
    return 'testfailed';
  }

  $render_time = Util::extract_token_from_file($binary_log, "_x_x_mozilla_trender", ",");
  if ($render_time) {
    chomp($render_time);
    my @times = split(',', $render_time);
    $render_time = $times[0];
  }
  $render_time =~ s/[\r\n]//g;

  $render_gfx_time = Util::extract_token_from_file($binary_log, "_x_x_mozilla_trender_gfx", ",");
  if ($render_gfx_time) {
    chomp($render_gfx_time);
    my @times = split(',', $render_gfx_time);
    $render_gfx_time = $times[0];
  }
  $render_gfx_time =~ s/[\r\n]//g;

  if (!$render_time || !$render_gfx_time) {
    Util::print_log("TinderboxPrint:Trender:[FAILED]\n");
    return 'testfailed';
  }

  Util::print_log_test_result_ms('render', 'Avg page render time in ms',
                           $render_time, 'Tr');

  Util::print_log_test_result_ms('rendergfx', 'Avg gfx render time in ms',
                           $render_gfx_time, 'Tgfx');

  if($Settings::TestsPhoneHome) {
    # Pull out detail data from log; this includes results for all sets
    my $raw_data = Util::extract_token_from_file($binary_log, "_x_x_mozilla_trender_details", ",");
    chomp($raw_data);

    Util::send_results_to_server($render_time, $raw_data, "render");
    Util::send_results_to_server($render_gfx_time, $raw_data, "rendergfx");
  }

  return 'success';
}

1;
