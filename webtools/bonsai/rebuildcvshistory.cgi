#!/usr/bonsaitools/bin/perl -w
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::RepositoryID;
    $zz = $::StartingDir;
}

use File::Basename;

require "CGI.pl";

sub ProcessOneFile {
     my ($filename) = @_;
     my $rlog = Param('rlogcommand') . " " . shell_escape($filename) . " |";
     my $doingtags = 0;
     my $filehead = dirname($filename);
     my (%branchname, $filerealname, $filetail, $line, $trimmed);
     my ($tag, $version, $branchid, $dirid, $fileid, $indesc, $desc);
     my ($author, $revision, $datestr, $date, $pluscount, $minuscount);
     my ($branch);

     print "$filename\n";

     die "Unable to run rlog command '$rlog': $!\n" 
          unless (open(RLOG_PROC, "$rlog"));
     undef (%branchname);

     ($filerealname = $filename) =~ s/,v$//g;
     $filehead =~ s!^$::Repository/*!!;
     $filehead = '.'
          unless ($filehead);
     $filetail = basename($filerealname);

     while (<RLOG_PROC>) {
          chop;
          $line = $_;
          $trimmed = trim($line);

          if ($doingtags) {
               if ($line !~ /^\t/) {
                    $doingtags = 0;
               } else {
                    $trimmed =~ /^([^:]*):([^:]*)/;
                    $tag = trim($1);
                    $version = trim($2);

                    next
                         unless (length($tag) && length($version));

                    $branchid = GetId('branches', 'branch', $tag);
                    $dirid    = GetId('dirs', 'dir', $filehead);
                    $fileid   = GetId('files', 'file', $filetail);
#
# Don't touch the tags database for now.  Nothing uses it, and it just takes
# up too much damn space.
#
#                SendSQL "replace into tags (branchid, repositoryid,
#                dirid, fileid, revision) values ($branchid,
#                $repositoryid, $dirid, $fileid, '$version')"
#

                    # Aha!  Second-to-last being a zero is CVS's special way
                    # of remembering a branch tag.
                    $version =~ /(.*)\.(\d+)(\.\d+)$/;
                    $branchname{"$1$3"} = $tag
                         if ($2 eq '0');
                    next;
               }
          }

          if ($line =~ /^symbolic names/) {
               $doingtags = 1;
               next;
          } elsif ($line =~ /^revision ([0-9.]*)$/) {
               $pluscount = ($minuscount = ($date = ($indesc = 0)));
               $desc = ($branch = ($author = ($datestr = ($revision = ''))));

               while (1) {
                    # Dealing with descriptions in rlog output for a
                    # revision...
                    if ($indesc) {
                         if (($line =~ /^-{27,30}$/) ||
                             ($line =~ /^={75,80}$/)) {
                              # OK, we're done.  Write it out.
                              if ($author && $datestr && $revision) {
                                   $datestr =~ s!^(\d{4})/(\d+/\d+)!$2/$1!;
                                   $date = str2time($datestr, "GMT");
                                   if ($date >= $::StartFrom) {
                                        AddToDatabase("C|$date|$author|$::Repository|$filehead|$filetail|$revision||$branch|+$pluscount|-$minuscount", $desc);
                                   }
                              }
                              $indesc = 0;
                         } else {
                              $desc .= $line . "\n";
                         }
                    }

                    # Dealing with revision information for a specific
                    # revision...
                    else {
                         if ($line =~ /^revision ([0-9.]*)$/) {
                              $pluscount = ($minuscount = 0);
                              $date = ($indesc = 0);
                              $datestr = ($desc = ($branch = ($author = "")));
                              $revision = $1;

                              $revision =~ /(.*)\.\d*$/;
                              $branch = $branchname{$1}
                                   if (exists($branchname{$1}));
                         }

                         elsif ($line =~ /^date:/) {
                              $line =~ s!^date: ([0-9 /:]*);\s+!!;
                              $datestr = $1;

                              $line =~ s!^author: ([^;]*);\s+!!;
                              $author = $1;

                              if ($line =~ /lines: \+(\d+) -(\d+)/) {
                                   $pluscount = $1;
                                   $minuscount = $2;
                              }
                         }

                         elsif ($line =~ /^branches: [0-9 .;]*$/) {
                              # Ignore these lines; make sure they don't
                              # become part of the description.
                         }

                         else {
                              $indesc = 1;
                              $desc = "$line\n";
                         }
                    }

		    $line = <RLOG_PROC>;
		    if (!defined $line) {
			last;
		    }
                    chop($line);
               }
          }
     }

     close(RLOG_PROC);
}


sub ProcessDirectory {
     my ($dir) = @_;
     my ($file, @files);

     die "$dir: not a directory" unless (-d $dir);
     die "$dir: Couldn't open for reading: $!"
          unless (opendir(DIR, $dir));
     @files = readdir(DIR);
     closedir (DIR);

     foreach $file (@files) {
          next if $file eq '.';
          next if $file eq '..';

          $file = "$dir/$file";
          if (-d $file) {
               &ProcessDirectory($file);
          } else {
               next unless ($file =~ /,v$/);

               if ($::FirstFile && ($::FirstFile ne $file)) {
                    print "Skipping $file...\n";
                    next;
               }
               $::FirstFile = 0;
               ProcessOneFile($file);
          }
     }
}


$| = 1;

if ($#ARGV == 4) {
     $::TreeID                   = $ARGV[0];
     $::FORM{'startfrom'}        = $ARGV[1];
     $::FORM{'firstfile'}        = $ARGV[2];
     $::FORM{'subdir'}           = $ARGV[3];
     $::FORM{'modules'}          = $ARGV[4];
} else {
     print "Content-type: text/html

<HTML>";
     CheckPassword(FormData('password'));
     print "
<title>Rebuilding CVS history database... please be patient...</title>
<body>
<pre>\n";
}

$::StartFrom   = ParseTimeAndCheck(FormData('startfrom'));
$::FirstFile   = trim(FormData('firstfile'));
$::SubDir      = trim(FormData('subdir'));
$::Modules     = '';

if (defined($::FORM{'modules'})) {
     $::Modules = trim(FormData('modules'));
}

Lock();
LoadTreeConfig();
Unlock();

ConnectToDatabase();

$::Repository    = $::TreeInfo{$::TreeID}{'repository'};
$::Description   = $::TreeInfo{$::TreeID}{'description'};
$::RepositoryID  = GetId('repositories', 'repository', $::Repository);
$::StartingDir   = 0;

print "
Rebuilding entire checkin history in $::Description, (`$::TreeID' tree) ...
";

Log("Rebuilding cvs history in $::Description, (`$::TreeID' tree)...");

LoadDirList();
my @Dirs = grep(!/\*$/, @::LegalDirs);
@Dirs = split(/,\s*/, $::Modules) if $::Modules;
my $StartingDir;
($StartingDir = "$::Repository/$::SubDir") =~ s!/.?$!! if $::SubDir;


print "Doing directories: @Dirs ...\n";
foreach my $Dir (@Dirs) {
     my $dir = "$::Repository/$Dir";

     unless (grep $Dir, @::LegalDirs) {
          print "$Dir: is invalid, skipping...\n";
     }

     if (-f $dir) {
          ProcessOneFile($dir);
     } elsif (-d $dir) {
          ProcessDirectory($dir);
     } elsif (!-r $dir) {
          print "$Dir: not readable, skipping...\n";
     } else {
          print "$Dir: not a file or directory, skipping...\n";
     }
}

exit 0;
