#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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

require 'globals.pl';

use vars qw($BatchID @TreeList @LegalDirs);

use File::Path;

if (@::CheckInList) {
     die '@::CheckInList is valid ?!?';
}

my $inheader = 1;
my $foundlogline = 0;
my @filelist = ();
my $log = '';
my $appendjunk = '';
my $repository = pickDefaultRepository();
my %group = ();
my $forcetreeid = '';
my ($chtype, $date, $name, $dir, $file);
my ($version, $sticky, $branch, $addlines, $removelines);
my ($key, $junk, $tagtime, $tagname, @data);
my ($mungedname, $filename, @treestocheck);
my (@files, @fullinfo, $i, $okdir, $f, $full, $d, $info, $id);
my ($mail, %substs, %headers, $body);
               

if (($#ARGV >= 1) && ($ARGV[0] eq '-treeid')) {
     $forcetreeid = $ARGV[1];
     shift; shift;
}

# Read in from remaining file arguments
DATAFILE:
for ( ; $#ARGV >= 0; shift) {
     next DATAFILE
          unless (open(FILE, $ARGV[0]));
     
LINE:
     while (<FILE>) {
          my $line = $_;
          chop($line);
          $line = trim($line);
          
          if ($inheader) {
               $inheader = 0 if ($line =~ /^$/);
               next LINE;
          }
          
          unless ($foundlogline) {
               if ($line =~ /^.\|/) {
                    $appendjunk .= "$line\n";
                    ($chtype, $date, $name, $repository, $dir, $file,
                     $version, $sticky, $branch, $addlines, $removelines) =
                          split(/\|/, $line);
                    $key = "$date|$branch|$repository|$dir|$name";
                    $group{$key} .= 
                         "$file|$version|$addlines|$removelines|$sticky\n";
               } elsif ($line =~ /^Tag\|/) {
                    ($junk, $repository, $tagtime, $tagname, @data) = 
                         split(/\|/, $line);
                    
                    ($mungedname = $repository) =~ s!/!_!g;
                    $filename = "data/taginfo/$mungedname/" .
                         MungeTagName($tagname);
                    
                    Lock();
                    unless (-d "data/taginfo/$mungedname") {
                        mkpath(["data/taginfo/$mungedname"], 1, 0777);
                    }
                    if (open(TAGFILE, ">> $filename")) {
                         print TAGFILE "$tagtime|" . join('|', @data) .  "\n";
                         close(TAGFILE);
                         chmod(0666, $filename);
                    }
                    Unlock();
               } elsif ($line =~ /^LOGCOMMENT/) {
                    $foundlogline = 1;
               }
               next LINE;
          }
          
          last LINE if ($line eq ":ENDLOGCOMMENT");
          $log .= "$line\n";
     }
     
     close(FILE);
#     unlink($ARGV[0]);
     
     my $plainlog = $log;
     $log = MarkUpText(html_quote(trim($log)));
     
     next DATAFILE unless ($plainlog && $appendjunk);

     Lock();
     LoadTreeConfig();
     unless ($forcetreeid) {
          ($mungedname = $repository) =~ s!/!_!g;
          $mungedname =~ s!^_!!;
          $filename = "data/checkinlog/$mungedname";
          unless (-d "data/checkinlog") {
              mkpath(["data/checkinlog"], 1, 0777);
          }
          if (open(TID, ">> $filename")) {
               print TID "${appendjunk}LOGCOMMENT\n$plainlog:ENDLOGCOMMENT\n";
               close(TID);
               chmod(0666, $filename);
          }
          
          ConnectToDatabase();
          AddToDatabase($appendjunk, $plainlog);
          DisconnectFromDatabase(); # Minimize time connected to the DB, and
                                # only do it while Lock()'d.  That way,
                                # zillions of addcheckin processes can't 
                                # lock up mysqld.
          @treestocheck = @::TreeList;
     }
     Unlock();
     
     @treestocheck = ($forcetreeid) if $forcetreeid;
     
     
     foreach $key (keys(%group)) {
          ($date, $branch, $repository, $dir, $name) = split(/\|/, $key);
          
          @files = ();
          @fullinfo = ();
          
          foreach $i (split(/\n/, $group{$key})) {
               ($file, $version, $addlines, $removelines) = split(/\|/, $i);
               push @files, $file;
               push @fullinfo, $i;
          }
          
TREE:
          foreach $::TreeID (@treestocheck) {
               next TREE if exists($::TreeInfo{$::TreeID}{nobonsai});
               next TREE 
                    unless ($branch =~ /^.?$::TreeInfo{$::TreeID}{branch}$/);
               next TREE 
                    unless ($repository eq $::TreeInfo{$::TreeID}{repository});
               
               LoadDirList();
               $okdir = 0;
               
FILE:
               foreach $f (@files) {
                    $full = "$dir/$f";
LEGALDIR:
                    foreach $d (sort( grep(!/\*$/, @::LegalDirs))) {
                         if ($full =~ m!^$d\b!) {
                              $okdir = 1;
                              last LEGALDIR;
                         }
                    }
                    last FILE if $okdir;
               }
               
               next TREE unless $okdir;
               
               Lock();
               undef $::BatchID;
               undef @::CheckInList;
               LoadCheckins();
               $id = "::checkin_${date}_$$";
               push @::CheckInList, $id;
               
               $info = eval("\\\%$id");
               %$info = (
                         person   => $name,
                         date     => $date,
                         dir      => $dir,
                         files    => join('!NeXt!', @files),
                         'log'    => $log,
                         treeopen => $::TreeOpen,
                         fullinfo => join('!NeXt!', @fullinfo)
                        );
               WriteCheckins();
               Log("Added checkin $name $dir " . join(' + ', @files));
               Unlock();
               
               if ($::TreeOpen) {
                    $filename = DataDir() . "/openmessage";
                    foreach $i (@::CheckInList) {
                         $filename = "this file doesn't exist"
# XXX verify...
                              if ((eval("\$$i" . "{person}") eq $name) &&
                                  ($i ne $id));
                    }
               } else {
                    $filename = DataDir() . "/closemessage";
               }
               
               if (!$forcetreeid && -f $filename && open(MAIL, "$filename")) {
                    $mail = join("", <MAIL>);
                    close(MAIL);
                    
                    %substs = (
                               profile    => GenerateProfileHTML($name),
                               nextclose  => "We don't remember close " .
                                             "times any more...",
                               name       => EmailFromUsername($name),
                               dir        => $dir,
                               files      => join(',', @files),
                               'log'      => $log,
                              );
                    $mail = PerformSubsts($mail, \%substs);
                    
                    %headers = ParseMailHeaders($mail);
                    %headers = CleanMailHeaders(%headers);
                    $body = FindMailBody($mail);
                    
                    my $mail_relay = Param("mailrelay");
                    my $mailer = Mail::Mailer->new("smtp",
                                                   Server => $mail_relay);
                    $mailer->open(\%headers)
                         or warn "Can't send hook mail: $!\n";
                    print $mailer "$body\n";
                    $mailer->close();
               }
          }
     }
}
