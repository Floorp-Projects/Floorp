#! /usr/bin/perl

 use Cwd;

 $path=`cd`;
 $path=~m/:/;

 $drive=$`;

 die "\nUsage: perl TestParser.pl [-b|-v|-c] <filelist>
 b -> creates baseline file(*.b)
 v -> creates verification file (*.v)
 c -> compare baseline vs verification\n"

 if(@ARGV < 2 || @ARGV > 2);

 open(FILE_LIST,$ARGV[1]) || die "\nCannot open $ARGV[1]\n";

 if($ARGV[0] eq "-b" || $ARGV[0] eq "-v") {
   $ARGV[0]=~s/-//g; # create file extension for the output

   foreach $input(<FILE_LIST>) {
     @output=split(/\./,$input);
     system("$drive://mozilla//dist//WIN32_D.obj//bin//TestParser.exe $input $output[0].$ARGV[0]");
   }
 }
 elsif($ARGV[0] eq "-c") {
  # use windows "fc" to compare files!!!
  foreach $input(<FILE_LIST>) {
     @output=split(/\./,$input);
     system("fc $output[0].b $output[0].v");
   }
 }
 else {
   print "\n\"$ARGV[0]\" unknown....\n";
   print "\nUsage: perl TestParser.pl [-b|-v|-c] <filelist>\n\n";
 }
 
 close(FILE_LIST);
