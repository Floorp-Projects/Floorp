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

#
# Unroll a module
#

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::TreeID;
    $zz = $::TreeInfo;
}

require 'CGI.pl';

$|=1;

print "Content-type: text/html\n\n";

my $CVS_ROOT = $::FORM{'cvsroot'};
$CVS_ROOT = pickDefaultRepository() unless $CVS_ROOT;

PutsHeader("CVS Module Analyzer", $CVS_ROOT);

cvsmenu("align=right width=20%");

print "
<p><b>This tool will show you the directories and files that make up a given
cvs module.</b>
";


print "
<p>
<FORM METHOD=GET ACTION='moduleanalyse.cgi'>
";


#
# module selector
#
print "

<nobr><b>Module:</b>
<SELECT name='module' size=5>
";

my $Module = 'default';
if( $::FORM{module} eq 'all' || $::FORM{module} eq '' ){
    print "<OPTION SELECTED VALUE='all'>All Files in the Repository\n";
}
else {
    print "<OPTION VALUE='all'>All Files in the Repository\n";
    print "<OPTION SELECTED VALUE='$::FORM{module}'>$::FORM{module}\n";
    $Module = $::FORM{module};
}

#
# Print out all the Different Modules
#
$::TreeID = $Module if (exists($::TreeInfo{$Module}{'repository'}));
LoadDirList();
for my $k  (sort( grep(!/\*$/, @::LegalDirs) ) ){
    print "<OPTION value='$k'>$k\n" if ($k ne $Module);
}

print "</SELECT></NOBR>\n";


print "
<br>
<br>
<INPUT TYPE=HIDDEN NAME=cvsroot VALUE='$CVS_ROOT'>
<INPUT TYPE=SUBMIT VALUE='Examine Module'>
</FORM>";


if( $::FORM{module} ne ''  ){
    my $mod = $::FORM{module};
    print "<h1>Examining Module '$mod'</h1>\n\n";

    for my $i (sort( grep(!/\*$/, @::LegalDirs) ) ){
        if( -d "$CVS_ROOT/$i"){
            print "<dt><tt>Dir:&nbsp;&nbsp;&nbsp;</tt>";
            print "<a href=rview.cgi?dir=$i&cvsroot=$CVS_ROOT>$i</a>";
        }
        elsif ( -r "$CVS_ROOT/$i,v" ){
            print "<dt><font color=blue><tt>File:&nbsp;&nbsp;</tt></font>";
            print "<a href=cvsblame.cgi?file=$i&root=$CVS_ROOT>$i</a>";
        }
        else {
            print "<dt><font color=red><tt>Error: </tt></font>";
            print "$i : Not a file or a directory.";
        }

#          if( $mod_map->{$i} == $IS_LOCAL ){
#              print "<font color=blue><tt> LOCAL</tt></font>";
#          }
        print "\n";
    }
}


sub sortTest {
    if( $_[0] eq $::FORM{sortby} ){
        return " SELECTED";
    }
    else {
        return "";
    }
}

sub dateTest {
    if( $_[0] eq $::FORM{date} ){
        return " CHECKED value=$_[0]";
    }
    else {
        return "value=$_[0]";
    }
}
