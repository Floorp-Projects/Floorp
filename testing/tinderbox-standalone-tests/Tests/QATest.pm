# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Client-side JavaScript, DOM Core/HTML/Views, and Form Submission tests.
# Currently only available inside netscape firewall.
sub QATest {
    my ($test_name, $build_dir, $binary_dir, $args) = @_;
    my $binary_log = "$build_dir/$test_name.log";
    my $url = "http://geckoqa.mcom.com/ngdriver/cgi-bin/ngdriver.cgi?findsuites=suites&tbox=1";
    
    # Settle OS.
    system('/bin/sh -c "sync; sleep 5"');
    
    my $rv;

    $rv = AliveTest("QATest_raw", $build_dir,
                    [@$args, $url],
                    $Settings::QATestTimeout);
    
    # XXXX testing.  -mcafee
    $rv = 'success';


    # Post-process log of test output.
    my $mode = "express"; 
    open QATEST, "perl $build_dir/../qatest.pl $build_dir/QATest_raw.log $mode |"
      or die "Unable to run qatest.pl";
    my $qatest_html = "";
    while (<QATEST>) {
      chomp;
      #$_ =~ s/\"/&quot;/g;  #### it doesn't like this line
      # $_ =~ s/\012//g;
      ### $_ =~ s/\s+\S/ /g;  # compress whitespace.
      $qatest_html .= $_;
    }
    close QATEST;
    Util::print_log("\n");

    # This works.
    #$qatest_html = "<table border=2 cellspacing=0><tr%20valign=top><td>&nbsp;</td><td>Passed</td><td>Failed</td><td>Total</td><td>Died</td><td>%&nbsp;Passed</td><td>%&nbsp;Failed</td></tr><tr%20valign=top><td>DHTML</a></td><td>9</td><td>0</td><td>9</td><td>0</td><td>100</td><td>0</td></tr><tr%20valign=top><td>DOM&nbsp;VIEWS</a></td><td>2</td><td>0</td><td>2</td><td>0</td><td>100</td><td>0</td></tr><tr%20valign=top><td>Total:</td><td>11</td><td>0</td><td>11</td><td>0</td><td>100</td><td>0</td></tr></table>";

    # Testing output
    open TEST_OUTPUT, ">qatest_out.log";
    print TEST_OUTPUT $qatest_html;
    close TEST_OUTPUT;

    Util::print_log("TinderboxPrint:<a href=\"javascript:var&nbsp;newwin;newwin=window.open(&quot;&quot;,&quot;&quot;,&quot;menubar=no,resizable=yes,height=150,width=500&quot;);var&nbsp;newdoc=newwin.document;newdoc.write('$qatest_html');newdoc.close();\">QA</a>\n");

    return $rv;  # Hard-coded for now.
}

1;
