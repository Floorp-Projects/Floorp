#!/usr/bin/perl

use Cwd;

$curr_dir=`cd`;

open(OUTFILE,">file_list.txt") || die "Can't open file_list.txt $!";
opendir(D,".");

@files=readdir(D);
$curr_dir=~s/\\/\//g;
chomp($curr_dir);

foreach $file(@files) {
 if($file=~m/\.*m/) {
   print OUTFILE "$file\n";
 }
}


