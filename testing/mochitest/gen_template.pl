#!/usr/bin/perl
#
#  gen_template.pl
#  Makes test case templates.
#  Takes one argument: a bug number
#
#  perl gen_template.pl 345876
#
#  sends a test case template for bug 345876 to stdout

$bug_number = $ARGV[0];

open(IN,"static/test.template.txt") or die("Failed to open myfile for reading.");
while((defined(IN)) && ($line = <IN>)) {
        $line =~ s/{BUGNUMBER}/$bug_number/g;
        print STDOUT $line;
}
close(IN);
