#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Mozilla graph tool.
#
# The Initial Developer of the Original Code is Apple Computer, Inc.
#
# Portions created by Apple Computer, Inc. are Copyright (C) 2005 Apple Computer, Inc.
# All Rights Reserved.
#
# Contributor(s): 
#   Apple Computer, Inc.
#   Chris McAfee <mcafee@mocha.com>, <sleestack@apple.com>
#
#

#
# CGI to interactively build up a machine/test matrix to compare performance graphs.
# URLs can be saved off to static pages, and you can also alter queries easily
# to compare a machine with another machine not in the default query, likewise for tests.
#
# To-do: convert this to use the standard CGI package, the one you
#        get from CPAN.
#

use strict;

use CGI::Carp qw(fatalsToBrowser);      # CPAN
use CGI::Request;                       # http://stein.cshl.org/WWW/software/CGI::modules/

my $req = new CGI::Request;


my $TBOXES = lc($req->param('tboxes'));
chomp($TBOXES);

my $TESTS = $req->param('tests');
chomp($TESTS);

my %englishTestnames = ();    # English for testnames.
my %machineConfigs   = ();    # Machine configs.

# Pick a default 2 machines, our main performance machine.
unless($TBOXES) {
    # hmm not sure we want a default here, start with nothing?
    #$TBOXES = "machine1, machine2";
} else {
    # Convert spaces to , list.  This will let the user type in 
    # space-delimited list in the text field.
    $TBOXES =~ s/\ /,/g;

    # Reduce multiple , to one ,
    $TBOXES =~ s/,+/,/g;
}

# Pick a default test.
unless($TESTS) {
    # hmm don't do this right now.
    #$TESTS = "sample_test_name";
} else {

    # Strip \n, change to space, we clean up spaces below.
    $TESTS =~ s/\xd/\ /g;

    # Last character should not be " ".
    $TESTS =~ s/\ $//g;

    # Strip LF
    $TESTS =~ s/\xa//g;

    # Convert spaces to , list.  This will let the user type in 
    # space-delimited list in the text field.
    $TESTS =~ s/\ /,/g;

    # Reduce multiple ,, to one ,
    $TESTS =~ s/,+/,/g;

}


sub getMachineOptionValues {
    # assume we just want a list of machines from the first test,
    # the plasma test.  if you don't do this test, you lose.
    # 
    # To-do: Fix this dependency.  The way the data in the db directory is
    #          db/testnames/machine-names
    #        We could walk this directory to get a list of machines instead.
    #        Or, at data-collection time, start maintaining a list of machine names.
    #

    # Get plasma machines.
    my @rawFiles;
    my @files;
    print "<!-- getMachineOptionValues -->\n";
    opendir(DIRNAME, "db/a-common-test-here") || die "can't open directory $!";
    @rawFiles = map("$_", sort grep !/^\.\.?$/, readdir DIRNAME);
    closedir(DIRNAME);

    # Yank _avg from @files
    foreach(@rawFiles) {
        unless(/_avg$/) {
            push(@files, $_);
        }
    }

    # Add option entries for each plasma machine.
    foreach(@files) {
        print "<option value=$_>$_</option>\n";
    }
}


# Provide an English-to-testname mapping if the testnames
# are too long, or you want to clean the name up.
sub loadEnglishTestnames {
    $englishTestnames{"testname01_as_cgi_submitted"} = 
        "<b>Nicer test01 name</b>";
}


# Store up some machine configs.  These are displayed as
# mouse-overs on the machine column header.
sub loadMachineConfigs {
    $machineConfigs{"localhost.foo.bar"} = "2x1.8GHz G5, 1GB";
}


# Lookup English name, fall back to submitted name.
sub getEnglishTestname {
    my ($testname) = @_;

    my $englishTestname = $englishTestnames{$testname};

    # If we don't have a better name, fallback to basic version.
    if($englishTestname eq "") {
        $englishTestname = $testname;

        # Try to reduce horizontal space here.
        $englishTestname =~ s/\./\.\<br\>/g;
    }

    return $englishTestname;
}


# Lookup machine config, do nothing if not found.
# This is used for mouse-over title tags on the machine column.
sub getMachineConfig {
    my ($machineName) = @_;

    my $machineConfig = $machineConfigs{$machineName};
    my $rv = "";

    # If we don't have a hit, return nothing.
    unless($machineConfig eq "") {
        $rv = "title=\"$machineConfig\"";
    }

    return $rv;
}



# Get a list of tests.
sub getTestOptionValues {

    my @rawFiles;
    my @files;
    print "<!-- getTestOptionValues -->\n";
    opendir(DIRNAME, "db") || die "can't open directory $!";
    @rawFiles = map("$_", sort grep !/^\.\.?$/, readdir DIRNAME);
    closedir(DIRNAME);

    foreach(@rawFiles) {
        unless(/_avg$/) {
            push(@files, $_);
        }
    }

    # Add option entries for each test.
    foreach(@files) {
        print "<option value=$_>$_</option>\n";
    }
}


# Title, machine names form.  First row of table.
sub printLeader {
    print "Content-type: text/html\n\n<HTML>\n";
    print "<head>\n";
    print "<title>Compare graphs for $TBOXES</title><br>\n";
    print "</head>\n";
    print "<body>\n";

    print "<table width=\"\" border=\"0\">\n";
    print "<tr>\n";
    print "<td width=\"\"><img src=\"http://www.mozilla.org/images/mozilla-16.png\"></td>\n";
    print "<td valign=\"\"><b><font size=\"+2\">Compare Graphs</font></b></td>\n";
    print "<td>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\n";
    print "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp&nbsp;&nbsp;</td>\n";
    print "<td width=\"\" valign=\"\" align=\"right\"><img src=\"images/up.jpg\"></td>\n";
    print "<td width=\"\">= good if data moves up,</td>\n";
    print "<td>&nbsp;&nbsp;</td>\n";
    print "<td width=\"\"><img src=\"images/down.jpg\"></td>\n";
    print "<td>= good if data moves down</td>\n";
    print "</tr><br>\n";
    print "</table>\n";
    print "<br>\n";

    print "<SCRIPT LANGUAGE=\"JavaScript\">\n";
    print "function addToTextfield(select, textfield) {\n";
    ##print "  alert(select.value);\n";  # Testing.
    print "  if(select.value != \"\") {\n";
    print "    if(textfield.value != \"\") {\n";
    print "      textfield.value += \", \"\n";
    print "    }\n";
    print "    textfield.value += select.value;\n";
    print "  }\n";
    print "}\n";
    print "</script>\n";

    print "<table>\n";

    #
    # First row, machines
    #
    print "<form method=\"get\" action=\"compare-graphs.cgi\">\n";
    print "<tr>\n";

    print "<td align=\"right\">machines:</td>\n";
    print "<td>\n";
    print "<input type=text value=\"$TBOXES\" name=\"tboxes\" size=82>\n";
    #print "<input type=hidden name=\"tests\" value=\"$TESTS\">";
    print "</td>\n";
    print "</tr>\n";

    print "<tr>\n";
    
    print "<td></td>\n";
    print "<td>\n";
    print "<select class=\"menu\" id=\"machineSelect\" onChange=\"addToTextfield(this.form.machineSelect, this.form.tboxes)\">\n";

    # Dummy label, no value.
    print "<option value=\"\"\">Add a machine</option>\n";
    
    getMachineOptionValues();

    print "</select>\n";

    print "</td>\n";
    print "</tr>\n";

    # Dummy row.
    print "<tr><td>&nbsp;</td><td>&nbsp;</td></tr>\n";

    #
    # 2nd row, tests.
    #
    print "<tr>\n";

    print "<td align=\"right\">tests:</td>\n";
    print "<td>\n";
    print "<textarea name=\"tests\" rows=\"3\" cols=\"80\">\n";
    print "$TESTS";
    print "</textarea>\n";
    
    #print "<input type=hidden name=\"tboxes\" value=\"$TBOXES\">";
    print "</td>\n";
    print "</tr>\n";

    print "<tr>\n";
    
    print "<td></td>\n";
    print "<td>\n";
    print "<select class=\"menu\" id=\"testSelect\" onChange=\"addToTextfield(this.form.testSelect, this.form.tests)\">\n";

    # Dummy label, no value.
    print "<option value=\"\"\">Add a test</option>\n";
    
    getTestOptionValues();

    print "</select>\n";

    print "</td>\n";
    print "</tr>\n";

    print "<tr>\n";
    print "<td></td>\n";
    print "<td>\n";
    print "<input type=\"submit\" value=\"show graphs\">\n";
    print "</td>\n";
    print "</tr>\n";

    print "</form>\n";

    print "</table>\n";
    print "<br>\n";


    # Start the graphs table.
    print "<a name=\"graphs\">\n";
    print "<table border=1 cellspacing=0>\n";

}


sub createGraphLink {
    my ($testname, $machineName) = @_;

    my $link = "<a href=\"http://localhost/query.cgi?tbox=$machineName&testname=$testname&autoscale=&size=&days=14&units=&ltype=&points=1=&avg=1&showpoint=\"><img src=\"http://localhost/graph.cgi?tbox=$machineName&testname=$testname&autoscale=&size=0.5&notitle=1&nolabel=1&days=14&units=&ltype=&points=1=&avg=1&showpoint=\"></a>";

    return $link;
}



# Fill out table, for each test, show graph for each machine in order.
sub printGraphs {

    my @machines = split(",", $TBOXES);
    my @tests = split(",", $TESTS);

    # TOC row
    print "<tr>\n";
    print "<td><b>Test name</b></td>\n";

    # TOC content
    foreach(@machines) {
        my $machine = $_;
        $machine =~ s/_.*$//g;
        my $config = getMachineConfig($machine);
        if($config eq "") {
            print "<td>$_</td>\n";
        } else {
            print "<td><a $config>$_</a></td>\n";
        }
    }
    print "</tr>\n";


    # loop over tests.
    foreach(@tests) {
        my $test = $_;

        #
        # For each test, print out the machine row.
        #
        
        print "<tr>\n";

        # First print the LHS column.
        print "<td>\n";
        print getEnglishTestname($test);
        print "</td>\n";
        
        
        # Now the machines.
        foreach(@machines) {
            my $machine = $_;

            print "<td>\n";
            print createGraphLink($test, $machine);
            print "</td>\n";

        }

        print "</tr>\n";
        

    }


}


# Finish.
sub printTrailer {

    # End the table.
    print "</table>\n";

}


# main
{
    loadEnglishTestnames();
    loadMachineConfigs();

    printLeader();

    printGraphs();

    print "</body>\n";
}
