#!c:\perl\bin\perl
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Navigator.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corp.  Portions created by Netscape Communications Corp. are
# Copyright (C) 1998, 1999, 2000, 2001 Netscape Communications Corp.  All
# Rights Reserved.
# 
# Contributor(s):
#   Sean Su <ssu@netscape.com>
# 

use Cwd;

if($#ARGV < 1)
{
    print_usage();
    exit(1);
}

print "searching for longfilenames in:\n";
print "  $ARGV[0]\n";

$outFile     = $ARGV[1];
$start_dir   = $ARGV[0];
$start_dir   =~ s/\\/\//g;

if(substr($start_dir, -1, 1) ne '/')
{
    $start_dir = $start_dir . "/";
}

($MOZ_TOOLS = $ENV{"MOZ_TOOLS"}) =~ s/\\/\//g;

# Open the output file
open(fpOutFile, ">>$outFile") || die "\nCould not open $outFile: $!\n";

# $rv has the following possible values:
#   0 - Found at least one long filename file
#   1 - Error locating GetShortPathName.exe
#   2 - No long filename file found
$rv = check_dir_structure($ARGV[0]);

print "\n";
close(fpOutFile);
exit($rv);
# end

sub check_dir_structure
{
    my($curr_dir) = @_;
    my($foundFirstFile) = 2; # This var is also used as a return value:
                             #   0 - Found at least one long filename file
                             #   1 - Error locating GetShortPathName.exe
                             #   2 - No long filename file found

    $save_cwd     = cwd();
    $save_cwd     =~ s/\\/\//g;
    if((-e "$curr_dir") && (-d "$curr_dir"))
    {
        $foundFirstFile = check_all_dir($curr_dir, $foundFirstFile);
        chdir($save_cwd);
        print " done!";
    }
    else
    {
        if(!(-e "$curr_dir"))
        {
            print "\n";
            print "$curr_dir does not exist!";
        }
        elsif(!(-d "$curr_dir"))
        {
            print "\n";
            print "$curr_dir is not a valid directory!";
        }
    }

    # print out the closing brace for the last file written out.
    if($foundFirstFile == 0)
    {
        # at least one file was found
        # print the "];\n" for the previous file found.
        print fpOutFile "];\n";
    }

    return($foundFirstFile);
}

sub check_all_dir
{
    my($curr_dir, $foundFirstFile) = @_;
    my(@dirlist);
    my($file);

    chdir("$curr_dir");
    @dirlist = <*>;
    foreach $file (@dirlist)
    {
        if(-d "$file")
        {
            print ".";
            $foundFirstFile = check_all_dir($file, $foundFirstFile);
        }
        elsif(-e $file)
        {
            if(check_extension($file))
            {
                $short_filename = `$MOZ_TOOLS/bin/GetShortPathName.exe $file`;
                if(($?/256) == 1)
                {
                    print "$MOZ_TOOLS/bin/GetShortPathName.exe: $!\n";
                    exit(1);
                }

                if(!($file =~ /$short_filename/i))
                {
                    if($foundFirstFile == 0)
                    {
                        # at least one file was found
                        # print the ",\n" for the previous file found.
                        print fpOutFile ",\n";
                    }

                    $curr_path = cwd();
                    # perl has problems dealing with '\\''s in split(), so we need to
                    # convert them to '/''s.
                    $curr_path =~ s/\\/\//g;
                    @relative_path = split(/$start_dir/,$curr_path);
                    if($relative_path[1] eq "")
                    {
                        print_file($file, $foundFirstFile);
                    }
                    else
                    {
                        print_file("$relative_path[1]/$file", $foundFirstFile);
                    }
                    $foundFirstFile = 0;
                }
            }
        }
    }
    chdir("..");
    return($foundFirstFile);
}

sub check_extension
{
  my($file)      = @_;
  my($var);
  @listExtension = ("dll",
                    "com",
                    "jar",
                    "exe");

  @arrayExtension = split(/\./,$file);
  $extension = @arrayExtension[$#arrayExtension];
  foreach $var (@listExtension)
  {
    if($extension eq $var)
    {
      return(1);
    }
  }
  return(0);
}

sub print_file
{
  my($file, $foundFirstFile) = @_;

  if($foundFirstFile)
  {
      # first file in list
      print fpOutFile "// This list contains filenames that are long filenames ( > 8.3) critical during installation time.\n";
      print fpOutFile "// This list is automatically generated during the build process.\n";
      print fpOutFile "// The filenames should include paths relative to the Netscape 6 folder.\n";
      print fpOutFile "// '/' must be used as path delimiters regardless of platform.\n";
      print fpOutFile "var listLongFilePaths = [\"$file\"";
  }
  else
  {
      # middle file in list
      print fpOutFile "                         \"$file\"";
  }
}

sub print_usage
{
    print "\n usage: $0 <dir> <outfile>\n";
    print "\n        dir     - full path to look for files\n";
    print "        outfile - file to append to\n";
}
