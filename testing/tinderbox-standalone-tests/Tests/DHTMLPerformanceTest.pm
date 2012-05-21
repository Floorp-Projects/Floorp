# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sub DHTMLPerformanceTest {
    my ($build_dir, $binary) = @_;
    my $dhtml_test_result;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    my $binary_log = "$build_dir/DHTMLPerformanceTest.log";
    my $url = "http://www.mozilla.org/performance/test-cases/dhtml/runTests.html";
    my $timeout_secs = 3;
    my $args = [$binary, $url];
    
    # Settle OS.
    system('/bin/sh -c "sync; sleep 5"');
    
    my $result = Util::run_cmd($build_dir, $binary_dir, $args, $timeout_secs);

    my $dhtml_time = Util::extract_token($result->{output},
                                         "_x_x_mozilla_dhtml",
                                         ",");

    if($dhtml_time) {
      $dhtml_test_result = 'success';
    } else {
      $dhtml_test_result = 'testfailed';
      Util::print_log("DHTMLTest: test failed\n");
    }
    
    if($dhtml_test_result eq 'success') {
      Util::print_log_test_result_ms('dhtml', 'DHTML time',
                               $dhtml_time, 'Tdhtml');
      if ($Settings::TestsPhoneHome) {
        Util::send_results_to_server($dhtml_time, "--", "dhtml");
      }      
    }

    return $dhtml_test_result;
}

1;
