#!/usr/bin/perl

# This script makes mochitest test case templates. See
# https://developer.mozilla.org/en-US/docs/Mochitest#Test_templates
#
# It takes two arguments:
#
#   -b:     a bugnumber
#   -type:  template type. One of {plain|xhtml|xul|th|chrome|chromexul}.
#           Defaults to th (testharness.js).
#
# For example, this command:
#
#  perl gen_template.pl -b 345876 -type xul
#
# writes a XUL test case template for bug 345876 to stdout.

use FindBin;
use Getopt::Long;
GetOptions("b=i"=> \$bug_number,
           "type:s"=> \$template_type);

if ($template_type eq "xul") {
  $template_type = "$FindBin::RealBin/static/xul.template.txt";
} elsif ($template_type eq "xhtml") {
  $template_type = "$FindBin::RealBin/static/xhtml.template.txt";
} elsif ($template_type eq "chrome") {
  $template_type = "$FindBin::RealBin/static/chrome.template.txt";
} elsif ($template_type eq "chromexul") {
  $template_type = "$FindBin::RealBin/static/chromexul.template.txt";
} elsif ($template_type eq "plain") {
  $template_type = "$FindBin::RealBin/static/test.template.txt";
} else {
  $template_type = "$FindBin::RealBin/static/th.template.txt";
}

open(IN,$template_type) or die("Failed to open myfile for reading.");
while((defined(IN)) && ($line = <IN>)) {
        $line =~ s/{BUGNUMBER}/$bug_number/g;
        print STDOUT $line;
}
close(IN);
