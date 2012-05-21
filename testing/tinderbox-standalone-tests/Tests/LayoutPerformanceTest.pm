# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

sub LayoutPerformanceTest {
    my ($build_dir, $binary) = @_;

    my $layout_test_result;
    my $layout_time_details;
    my $binary_basename = File::Basename::basename($binary);
    my $binary_dir = File::Basename::dirname($binary);
    #my $url = "http://$Settings::pageload_server/page-loader/loader.pl?delay=1000&nocache=0&maxcyc=4&timeout=$Settings::LayoutPerformanceTestPageTimeout&auto=1";
    my $timeout_secs = 4500;
    my $url = 'file:///home/rhelmer/src/mozilla/tools/performance/pageload/cycler.html';
    $args = [$binary, $url];
    
    # Settle OS.
    system('/bin/sh -c "sync; sleep 5"');
    

    my $result = Util::run_cmd($build_dir, $binary_dir, $args, $timeout_secs);

    my $layout_time = Util::extract_token(
                                        $result->{output}, 
                                        "_x_x_mozilla_page_load",
                                        ",");
    
    if($layout_time) {
      chomp($layout_time);
      my @times = split(',', $layout_time);
      $layout_time = $times[0];  # Set layout time to first number. 
      print("Layout time: $layout_time");
    } else {
      Util::print_log("TinderboxPrint:Tp:[CRASH]\n");
    }
    
    # Set test status.
    if($layout_time) {
      $layout_test_result = 'success';
      Util::print_log("LayoutPerformanceTest: test succeeded\n");
    } else {
      $layout_test_result = 'testfailed';
      Util::print_log("LayoutPerformanceTest: test failed\n");
    }
    
    if($layout_test_result eq 'success') {
      my $tp_prefix = "";
      if($Settings::BinaryName eq "TestGtkEmbed") {
        $tp_prefix = "m";
      }
      
      Util::print_log_test_result_ms('pageload',
                               'Avg of the median per url pageload time',
                               $layout_time, 'Tp');
      
      # Pull out detail data from log.
      my $raw_data = Util::extract_token($result->{output}, 
                                         "_x_x_mozilla_page_load_details", 
                                         ",");
      chomp($raw_data);
      
      if($Settings::TestsPhoneHome) {
        Util::send_results_to_server($layout_time, $raw_data, "pageload");
      }
    }

    return $layout_test_result;
}

1;
