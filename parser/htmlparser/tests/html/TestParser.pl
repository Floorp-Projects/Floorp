#! /usr/bin/perl

 use Cwd;

 die "\nUsage: perl TestParser.pl [-b|-v] <bin-path> <filelist>
 b        -> create baseline 
 v        -> verify changes
 bin-path -> Ex. <drive>:/<path>/mozilla/dist/bin
 filelist -> Run ListGen.pl which will yield file_list.txt\n"

 if(@ARGV < 3 || @ARGV > 3);

 open(FILE_LIST,$ARGV[2]) || die "\nCannot open $ARGV[2]\n";

 if($ARGV[0] eq "-b") {
   foreach $input(<FILE_LIST>) {
     $input =~s/\n//g;
     @output=split(/\./,$input);
     print "\n$input\n";
     system("$ARGV[1]/TestParser.exe $input $output[0].b");
   }
 }
 elsif($ARGV[0] eq "-v") {
   foreach $input(<FILE_LIST>) {
     $input =~s/\n//g;
     @output=split(/\./,$input);
     print "\n$input\n";
     system("$ARGV[1]/TestParser.exe $input $output[0].v");
     system("diff -u $output[0].b $output[0].v");
   }
 }
 else {
   print "\n\"$ARGV[0]\" unknown....\n";
   print "\nUsage: perl TestParser.pl [-b|-v] <bin-path> <filelist>\n\n";
 }
 
 close(FILE_LIST);
