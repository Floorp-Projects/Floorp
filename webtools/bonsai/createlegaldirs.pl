#!/usr/bonsaitools/bin/perl
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

sub add_module {
     my ($str) = @_;
     my $module;

     $str =~ s/^\s*(\S+)\s*(-\S*\s*)?//;
     $module = $1;

     $::Modules{$module} = $str;
}

sub init_modules {
     my ($cvsroot, $curline);
     my $cvscommand = Param('cvscommand');
     
     undef %::Modules;
     $cvsroot = $::TreeInfo{$::TreeID}{'repository'};
     
     $::CVSCOMMAND = "$cvscommand -d $cvsroot checkout -c";
     open(MODULES, "$::CVSCOMMAND |") ||
          die "Unable to read modules list from CVS\n";
     
     $curline = "";
     while (<MODULES>) {
          chop;
          
          if (/^\s+/) {
               # Replace any leading whitespace with a single space before 
               # appending to curline. This is necessary for long All lines
               # which get split over reads from the CVSCOMMAND.
               # The match of oldlist and newlist in find_dirs will fail 
               # if this is not done.
               s/^\s+/ /;
               $curline .= $_;
          } else {
               add_module($curline) if ($curline);
               $curline = $_;
          }
     }
     add_module($curline) if ($curline);
     close(MODULES);
}

sub init {
     $::TreeID = $ARGV[0];
     die "Must specify a treeid...\n"
          unless ($::TreeID);

     LoadTreeConfig();

     $::ModuleName = $::TreeInfo{$::TreeID}{'module'};
     init_modules();
     die "modules file no longer includes `$::ModuleName' ???
      Used `$::CVSCOMMAND' to try to find it\n"
          unless (exists($::Modules{$::ModuleName}));

     $::DataDir = DataDir();
}

sub find_dirs {
     my ($oldlist, $list, $i);
     
     $oldlist = '';
     $list = $::ModuleName;

     until ($list eq $oldlist) {
          $oldlist = $list;
          $list = '';
          foreach $i (split(/\s+/, $oldlist)) {
               if (exists($::Modules{$i})) {
                    $list .= "$::Modules{$i} ";
                    # Do an undef to prevent infinite recursion.
                    undef($::Modules{$i});
               } else {
                    $list .= "$i ";
               }
          }

          $list =~ s/\s+$//;
     }

     return ($list);
}

sub create_legal_dirs {
     my ($dirs);
     
     $list = find_dirs();
     Lock();
     unless (open(LDIR, "> $::DataDir/legaldirs")) {
          Unlock();
          die "Couldn't create $::DataDir/legaldirs";
     }
     chmod(0666,"$::DataDir/legaldirs");
     
     foreach $i (split(/\s+/, $list)) {
          print LDIR "$i\n";
          print LDIR "$i/*\n";
     }
     close(LDIR);
     Unlock();
}

##
## Main program...
##
Log("Attempting to recreate legaldirs...");
init();
create_legal_dirs();
Log("...legaldirs recreated.");
exit(0);
