#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Dave Miller <dave@intrec.com>

use diagnostics;
use strict;

my $UserInEditGroupSet = -1;
my $UserInCanConfirmGroupSet = -1;

require "CGI.pl";
use RelationSet;

# Shut up misguided -w warnings about "used only once":

use vars %::versions,
    %::components,
    %::COOKIE,
    %::MFORM,
    %::legal_keywords,
    %::legal_opsys,
    %::legal_platform,
    %::legal_priority,
    %::target_milestone,
    %::legal_severity;

my $whoid = confirm_login();

my $requiremilestone = 0;

print "Content-type: text/html\n\n";

PutHeader ("Bug processed");

GetVersionTable();

if ( Param("strictvaluechecks") ) {
    CheckFormFieldDefined(\%::FORM, 'product');
    CheckFormFieldDefined(\%::FORM, 'version');
    CheckFormFieldDefined(\%::FORM, 'component');

    # check if target milestone is defined - matthew@zeroknowledge.com
    if ( Param("usetargetmilestone") ) {
        CheckFormFieldDefined(\%::FORM, 'target_milestone');
    }
}

if ($::FORM{'product'} ne $::dontchange) {
    if ( Param("strictvaluechecks") ) {
        CheckFormField(\%::FORM, 'product', \@::legal_product);
    }
    my $prod = $::FORM{'product'};

    # note that when this script is called from buglist.cgi (rather
    # than show_bug.cgi), it's possible that the product will be changed
    # but that the version and/or component will be set to 
    # "--dont_change--" but still happen to be correct.  in this case,
    # the if statement will incorrectly trigger anyway.  this is a 
    # pretty weird case, and not terribly unreasonable behavior, but 
    # worthy of a comment, perhaps.
    #
    my $vok = lsearch($::versions{$prod}, $::FORM{'version'}) >= 0;
    my $cok = lsearch($::components{$prod}, $::FORM{'component'}) >= 0;

    my $mok = 1;   # so it won't affect the 'if' statement if milestones aren't used
    if ( Param("usetargetmilestone") ) {
       $mok = lsearch($::target_milestone{$prod}, $::FORM{'target_milestone'}) >= 0;
    }

    if (!$vok || !$cok || !$mok) {
        print "<H1>Changing product means changing version, target milestone and component.</H1>\n";
        print "You have chosen a new product, and now the version, target milestone and/or\n";
        print "component fields are not correct.  (Or, possibly, the bug did\n";
        print "not have a valid target milestone, component or version field in the first place.)\n";
        print "Anyway, please set the version, target milestone and component now.<p>\n";
        print "<form>\n";
        print "<table>\n";
        print "<tr>\n";
        print "<td align=right><b>Product:</b></td>\n";
        print "<td>$prod</td>\n";
        print "</tr><tr>\n";
        print "<td align=right><b>Version:</b></td>\n";
        print "<td>" . Version_element($::FORM{'version'}, $prod) . "</td>\n";
        print "</tr><tr>\n";

        if ( Param("usetargetmilestone") ) {
            print "<td align=right><b>Target Milestone:</b></td>\n";
            print "<td>" . Milestone_element($::FORM{'target_milestone'}, $prod) . "</td>\n";
            print "</tr><tr>\n";
        }

        print "<td align=right><b>Component:</b></td>\n";
        print "<td>" . Component_element($::FORM{'component'}, $prod) . "</td>\n";
        print "</tr>\n";
        print "</table>\n";
        foreach my $i (keys %::FORM) {
            if ($i ne 'version' && $i ne 'component' && $i ne 'target_milestone') {
                print "<input type=hidden name=$i value=\"" .
                value_quote($::FORM{$i}) . "\">\n";
            }
        }
        print "<input type=submit value=Commit>\n";
        print "</form>\n";
        print "</hr>\n";
        print "<a href=query.cgi>Cancel all this and go to the query page.</a>\n";
        PutFooter();
        exit;
    }
}


# Checks that the user is allowed to change the given field.  Actually, right
# now, the rules are pretty simple, and don't look at the field itself very
# much, but that could be enhanced.

my $lastbugid = 0;
my $ownerid;
my $reporterid;
my $qacontactid;

sub CheckCanChangeField {
    my ($f, $bugid, $oldvalue, $newvalue) = (@_);
    if ($f eq "assigned_to" || $f eq "reporter" || $f eq "qa_contact") {
        if ($oldvalue =~ /^\d+$/) {
            if ($oldvalue == 0) {
                $oldvalue = "";
            } else {
                $oldvalue = DBID_to_name($oldvalue);
            }
        }
    }
    if ($oldvalue eq $newvalue) {
        return 1;
    }
    if (trim($oldvalue) eq trim($newvalue)) {
        return 1;
    }
    if ($f =~ /^longdesc/) {
        return 1;
    }
    if ($UserInEditGroupSet < 0) {
        $UserInEditGroupSet = UserInGroup("editbugs");
    }
    if ($UserInEditGroupSet) {
        return 1;
    }
    if ($lastbugid != $bugid) {
        SendSQL("SELECT reporter, assigned_to, qa_contact FROM bugs " .
                "WHERE bug_id = $bugid");
        ($reporterid, $ownerid, $qacontactid) = (FetchSQLData());
    }
    # Let reporter change bug status, even if they can't edit bugs.
    # If reporter can't re-open their bug they will just file a duplicate.
    # While we're at it, let them close their own bugs as well.
    if ( ($f eq "bug_status") && ($whoid eq $reporterid) ) {
        return 1;
    }
    if ($f eq "bug_status" && $newvalue ne $::unconfirmedstate &&
        IsOpenedState($newvalue)) {

        # Hmm.  They are trying to set this bug to some opened state
        # that isn't the UNCONFIRMED state.  Are they in the right
        # group?  Or, has it ever been confirmed?  If not, then this
        # isn't legal.

        if ($UserInCanConfirmGroupSet < 0) {
            $UserInCanConfirmGroupSet = UserInGroup("canconfirm");
        }
        if ($UserInCanConfirmGroupSet) {
            return 1;
        }
        SendSQL("SELECT everconfirmed FROM bugs WHERE bug_id = $bugid");
        my $everconfirmed = FetchOneColumn();
        if ($everconfirmed) {
            return 1;
        }
    } elsif ($reporterid eq $whoid || $ownerid eq $whoid ||
             $qacontactid eq $whoid) {
        return 1;
    }
    SendSQL("UNLOCK TABLES");
    $oldvalue = value_quote($oldvalue);
    $newvalue = value_quote($newvalue);
    print PuntTryAgain(qq{
Only the owner or submitter of the bug, or a sufficiently
empowered user, may make that change to the $f field.
<TABLE>
<TR><TH ALIGN="right">Old value:</TH><TD>$oldvalue</TD></TR>
<TR><TH ALIGN="right">New value:</TH><TD>$newvalue</TD></TR>
</TABLE>
});
    PutFooter();
    exit();
}


    
    


my @idlist;
if (defined $::FORM{'id'}) {

    # since this means that we were called from show_bug.cgi, now is a good
    # time to do a whole bunch of error checking that can't easily happen when
    # we've been called from buglist.cgi, because buglist.cgi only tweaks
    # values that have been changed instead of submitting all the new values.
    # (XXX those error checks need to happen too, but implementing them 
    # is more work in the current architecture of this script...)
    #
    if ( Param('strictvaluechecks') ) { 
        CheckFormField(\%::FORM, 'rep_platform', \@::legal_platform);
        CheckFormField(\%::FORM, 'priority', \@::legal_priority);
        CheckFormField(\%::FORM, 'bug_severity', \@::legal_severity);
        CheckFormField(\%::FORM, 'component', 
                       \@{$::components{$::FORM{'product'}}});
        CheckFormFieldDefined(\%::FORM, 'bug_file_loc');
        CheckFormFieldDefined(\%::FORM, 'short_desc');
        CheckFormField(\%::FORM, 'product', \@::legal_product);
        CheckFormField(\%::FORM, 'version', 
                       \@{$::versions{$::FORM{'product'}}});
        CheckFormField(\%::FORM, 'op_sys', \@::legal_opsys);
        CheckFormFieldDefined(\%::FORM, 'longdesclength');
        CheckPosInt($::FORM{'id'});
    }
    push @idlist, $::FORM{'id'};
} else {
    foreach my $i (keys %::FORM) {
        if ($i =~ /^id_/) {
            if ( Param('strictvaluechecks') ) { 
                CheckPosInt(substr($i, 3));
            }
            push @idlist, substr($i, 3);
        }
    }
}

my $action  = '';
if (defined $::FORM{action}) {
  $action  = trim($::FORM{action});
}
if ($action eq Param("move-button-text")) {
  $::FORM{'buglist'} = join (":", @idlist);
  do "move.pl" || die "Error executing move.cgi: $!";
  PutFooter();
  exit;
}


if (!defined $::FORM{'who'}) {
    $::FORM{'who'} = $::COOKIE{'Bugzilla_login'};
}

# the common updates to all bugs in @idlist start here
#
print "<TITLE>Update Bug " . join(" ", @idlist) . "</TITLE>\n";
if (defined $::FORM{'id'}) {
    navigation_header();
}
print "<HR>\n";
$::query = "update bugs\nset";
$::comma = "";
umask(0);

sub DoComma {
    $::query .= "$::comma\n    ";
    $::comma = ",";
}

sub DoConfirm {
    if ($UserInEditGroupSet < 0) {
        $UserInEditGroupSet = UserInGroup("editbugs");
    }
    if ($UserInCanConfirmGroupSet < 0) {
        $UserInCanConfirmGroupSet = UserInGroup("canconfirm");
    }
    if ($UserInEditGroupSet || $UserInCanConfirmGroupSet) {
        DoComma();
        $::query .= "everconfirmed = 1";
    }
}


sub ChangeStatus {
    my ($str) = (@_);
    if ($str ne $::dontchange) {
        DoComma();
        if (IsOpenedState($str)) {
            $::query .= "bug_status = IF(everconfirmed = 1, '$str', '$::unconfirmedstate')";
        } else {
            $::query .= "bug_status = '$str'";
        }
        $::FORM{'bug_status'} = $str; # Used later for call to
                                      # CheckCanChangeField to make sure this
                                      # is really kosher.
    }
}

sub ChangeResolution {
    my ($str) = (@_);
    if ($str ne $::dontchange) {
        DoComma();
        $::query .= "resolution = '$str'";
    }
}

#
# This function checks if there is a comment required for a specific
# function and tests, if the comment was given.
# If comments are required for functions  is defined by params.
#
sub CheckonComment( $ ) {
    my ($function) = (@_);
    
    # Param is 1 if comment should be added !
    my $ret = Param( "commenton" . $function );

    # Allow without comment in case of undefined Params.
    $ret = 0 unless ( defined( $ret ));

    if( $ret ) {
        if (!defined $::FORM{'comment'} || $::FORM{'comment'} =~ /^\s*$/) {
            # No comment - sorry, action not allowed !
            PuntTryAgain("You have to specify a <b>comment</b> on this " .
                         "change.  Please give some words " .
                         "on the reason for your change.");
        } else {
            $ret = 0;
        }
    }
    return( ! $ret ); # Return val has to be inverted
}


my $foundbit = 0;
foreach my $b (grep(/^bit-\d*$/, keys %::FORM)) {
    if (!$foundbit) {
        $foundbit = 1;
        DoComma();
        $::query .= "groupset = 0";
    }
    if ($::FORM{$b}) {
        my $v = substr($b, 4);
        $::query .= "+ $v";     # Carefully written so that the math is
                                # done by MySQL, which can handle 64-bit math,
                                # and not by Perl, which I *think* can not.
    }
}

foreach my $field ("rep_platform", "priority", "bug_severity",          
                   "summary", "component", "bug_file_loc", "short_desc",
                   "product", "version", "op_sys",
                   "target_milestone", "status_whiteboard") {
    if (defined $::FORM{$field}) {
        if ($::FORM{$field} ne $::dontchange) {
            DoComma();
            $::query .= "$field = " . SqlQuote(trim($::FORM{$field}));
        }
    }
}


if (defined $::FORM{'qa_contact'}) {
    my $name = trim($::FORM{'qa_contact'});
    if ($name ne $::dontchange) {
        my $id = 0;
        if ($name ne "") {
            $id = DBNameToIdAndCheck($name);
        }
        DoComma();
        $::query .= "qa_contact = $id";
    }
}


ConnectToDatabase();

my $formCcSet = new RelationSet;
my $origCcSet = new RelationSet;
my $origCcString;

# We make sure to check out the CC list before we actually start touching any
# bugs.  mergeFromString() ultimately searches the database using a quoted
# form of the data it gets from $::FORM{'cc'}, so anything bogus from a 
# security standpoint should trigger an abort there.
#
if (defined $::FORM{'newcc'} && defined $::FORM{'id'}) {
    $origCcSet->mergeFromDB("select who from cc where bug_id = $::FORM{'id'}");
    $formCcSet->mergeFromDB("select who from cc where bug_id = $::FORM{'id'}");
    $origCcString = $origCcSet->toString();  # cache a copy of the string vers
    if ((exists $::FORM{'removecc'}) && (exists $::FORM{'cc'})) {
      $formCcSet->removeItemsInArray(@{$::MFORM{'cc'}});
    }
    $formCcSet->mergeFromString($::FORM{'newcc'});
}

if ( Param('strictvaluechecks') ) {
    CheckFormFieldDefined(\%::FORM, 'knob');
}
SWITCH: for ($::FORM{'knob'}) {
    /^none$/ && do {
        last SWITCH;
    };
    /^confirm$/ && CheckonComment( "confirm" ) && do {
        DoConfirm();
        ChangeStatus('NEW');
        last SWITCH;
    };
    /^accept$/ && CheckonComment( "accept" ) && do {
        DoConfirm();
        ChangeStatus('ASSIGNED');
        if (Param("musthavemilestoneonaccept")) {
            if (Param("usetargetmilestone")) {
                $requiremilestone = 1;
            }
        }
        last SWITCH;
    };
    /^clearresolution$/ && CheckonComment( "clearresolution" ) && do {
        ChangeResolution('');
        last SWITCH;
    };
    /^resolve$/ && CheckonComment( "resolve" ) && do {
        ChangeStatus('RESOLVED');
        ChangeResolution($::FORM{'resolution'});
        last SWITCH;
    };
    /^reassign$/ && CheckonComment( "reassign" ) && do {
        if ($::FORM{'andconfirm'}) {
            DoConfirm();
        }
        ChangeStatus('NEW');
        DoComma();
        if ( Param("strictvaluechecks") ) {
          if ( !defined$::FORM{'assigned_to'} ||
               trim($::FORM{'assigned_to'}) eq "") {
            PuntTryAgain("You cannot reassign to a bug to nobody.  Unless " .
                         "you intentionally cleared out the " .
                         "\"Reassign bug to\" field, " .
                         Param("browserbugmessage"));
          }
        }
        my $newid = DBNameToIdAndCheck($::FORM{'assigned_to'});
        $::query .= "assigned_to = $newid";
        last SWITCH;
    };
    /^reassignbycomponent$/  && CheckonComment( "reassignbycomponent" ) && do {
        if ($::FORM{'product'} eq $::dontchange) {
            PuntTryAgain("You must specify a product to help determine the " .
                         "new owner of these bugs.");
        }
        if ($::FORM{'component'} eq $::dontchange) {
            PuntTryAgain("You must specify a component whose owner should " .
                         "get assigned these bugs.");
        }
        if ($::FORM{'compconfirm'}) {
            DoConfirm();
        }
        ChangeStatus('NEW');
        SendSQL("select initialowner from components where program=" .
                SqlQuote($::FORM{'product'}) . " and value=" .
                SqlQuote($::FORM{'component'}));
        my $newname = FetchOneColumn();
        my $newid = DBNameToIdAndCheck($newname, 1);
        DoComma();
        $::query .= "assigned_to = $newid";
        if (Param("useqacontact")) {
            SendSQL("select initialqacontact from components where program=" .
                    SqlQuote($::FORM{'product'}) .
                    " and value=" . SqlQuote($::FORM{'component'}));
            my $qacontact = FetchOneColumn();
            if (defined $qacontact && $qacontact ne "") {
                my $newqa = DBNameToIdAndCheck($qacontact, 1);
                DoComma();
                $::query .= "qa_contact = $newqa";
            }
        }
        last SWITCH;
    };   
    /^reopen$/  && CheckonComment( "reopen" ) && do {
		SendSQL("SELECT resolution FROM bugs WHERE bug_id = $::FORM{'id'}");
        ChangeStatus('REOPENED');
        ChangeResolution('');
		if (FetchOneColumn() eq 'DUPLICATE') {
			SendSQL("DELETE FROM duplicates WHERE dupe = $::FORM{'id'}");
		}		
        last SWITCH;
    };
    /^verify$/ && CheckonComment( "verify" ) && do {
        ChangeStatus('VERIFIED');
        last SWITCH;
    };
    /^close$/ && CheckonComment( "close" ) && do {
        ChangeStatus('CLOSED');
        last SWITCH;
    };
    /^duplicate$/ && CheckonComment( "duplicate" ) && do {
        ChangeStatus('RESOLVED');
        ChangeResolution('DUPLICATE');
        if ( Param('strictvaluechecks') ) {
            CheckFormFieldDefined(\%::FORM,'dup_id');
        }
        my $num = trim($::FORM{'dup_id'});
        SendSQL("SELECT bug_id FROM bugs WHERE bug_id = " . SqlQuote($num));
        $num = FetchOneColumn();
        if (!$num) {
            PuntTryAgain("You must specify a bug number of which this bug " .
                         "is a duplicate or a valid bug number.  The bug has not been changed.")
        }
        if (!defined($::FORM{'id'}) || $num == $::FORM{'id'}) {
            PuntTryAgain("Nice try, $::FORM{'who'}.  But it doesn't really ".
                         "make sense to mark a bug as a duplicate of " .
                         "itself, does it?");
        }
        my $checkid = trim($::FORM{'id'});
        SendSQL("SELECT bug_id FROM bugs where bug_id = " .  SqlQuote($checkid));
        $checkid = FetchOneColumn();
        if (!$checkid) {
            PuntTryAgain("The bug id $::FORM{'id'} is invalid. Please reload this bug ".
                         "and try again.");
        }
        AppendComment($num, $::FORM{'who'}, "*** Bug $::FORM{'id'} has been marked as a duplicate of this bug. ***");
        if ( Param('strictvaluechecks') ) {
          CheckFormFieldDefined(\%::FORM,'comment');
        }
        SendSQL("INSERT INTO duplicates VALUES ($num, $::FORM{'id'})");
        $::FORM{'comment'} .= "\n\n*** This bug has been marked as a duplicate of $num ***";
        print "<TABLE BORDER=1><TD><H2>Notation added to bug $num</H2>\n";
        system("./processmail", $num, $::FORM{'who'});
        print "<TD><A HREF=\"show_bug.cgi?id=$num\">Go To BUG# $num</A></TABLE>\n";

        last SWITCH;
    };
    # default
    print "Unknown action $::FORM{'knob'}!\n";
    PutFooter();
    exit;
}


if ($#idlist < 0) {
    PuntTryAgain("You apparently didn't choose any bugs to modify.");
}


my @keywordlist;
my %keywordseen;

if ($::FORM{'keywords'}) {
    foreach my $keyword (split(/[\s,]+/, $::FORM{'keywords'})) {
        if ($keyword eq '') {
            next;
        }
        my $i = GetKeywordIdFromName($keyword);
        if (!$i) {
            PuntTryAgain("Unknown keyword named <code>$keyword</code>. " .
                         "<P>The legal keyword names are " .
                         "<A HREF=describekeywords.cgi>" .
                         "listed here</A>.");
        }
        if (!$keywordseen{$i}) {
            push(@keywordlist, $i);
            $keywordseen{$i} = 1;
        }
    }
}

my $keywordaction = $::FORM{'keywordaction'} || "makeexact";

if ($::comma eq "" && 0 == @keywordlist && $keywordaction ne "makeexact") {
    if (!defined $::FORM{'comment'} || $::FORM{'comment'} =~ /^\s*$/) {
        PuntTryAgain("Um, you apparently did not change anything on the " .
                     "selected bugs.");
    }
}

my $basequery = $::query;
my $delta_ts;


sub SnapShotBug {
    my ($id) = (@_);
    SendSQL("select delta_ts, " . join(',', @::log_columns) .
            " from bugs where bug_id = $id");
    my @row = FetchSQLData();
    $delta_ts = shift @row;

    return @row;
}


sub SnapShotDeps {
    my ($i, $target, $me) = (@_);
    SendSQL("select $target from dependencies where $me = $i order by $target");
    my @list;
    while (MoreSQLData()) {
        push(@list, FetchOneColumn());
    }
    return join(',', @list);
}


my $timestamp;

sub LogDependencyActivity {
    my ($i, $oldstr, $target, $me) = (@_);
    my $newstr = SnapShotDeps($i, $target, $me);
    if ($oldstr ne $newstr) {
        my $fieldid = GetFieldID($target);
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,oldvalue,newvalue) VALUES " .
                "($i,$whoid,$timestamp,$fieldid,'$oldstr','$newstr')");
        return 1;
    }
    return 0;
}

delete $::FORM{'resolution'};   # Make sure we don't test the resolution
                                # against our permissions; we've already done
                                # that kind of testing, and this form field
                                # is actually usually not used.


# this loop iterates once for each bug to be processed (eg when this script
# is called with multiple bugs selected from buglist.cgi instead of
# show_bug.cgi).
#
foreach my $id (@idlist) {
    my %dependencychanged;
    my $write = "WRITE";        # Might want to make a param to control
                                # whether we do LOW_PRIORITY ...
    SendSQL("LOCK TABLES bugs $write, bugs_activity $write, cc $write, " .
            "profiles $write, dependencies $write, votes $write, " .
            "keywords $write, longdescs $write, fielddefs $write, " .
            "keyworddefs READ, groups READ, attachments READ, products READ");
    my @oldvalues = SnapShotBug($id);
    my %oldhash;
    my $i = 0;
    foreach my $col (@::log_columns) {
        $oldhash{$col} = $oldvalues[$i];
        if (exists $::FORM{$col}) {
            CheckCanChangeField($col, $id, $oldvalues[$i], $::FORM{$col});
        }
        $i++;
    }
    if ($requiremilestone) {
        my $value = $::FORM{'target_milestone'};
        if (!defined $value || $value eq $::dontchange) {
            $value = $oldhash{'target_milestone'};
        }
        SendSQL("SELECT defaultmilestone FROM products WHERE product = " .
                SqlQuote($oldhash{'product'}));
        if ($value eq FetchOneColumn()) {
            SendSQL("UNLOCK TABLES");
            PuntTryAgain("You must determine a target milestone for bug $id " .
                         "if you are going to accept it.  (Part of " .
                         "accepting a bug is giving an estimate of when it " .
                         "will be fixed.)");
        }
    }   
    if (defined $::FORM{'delta_ts'} && $::FORM{'delta_ts'} ne $delta_ts) {
        print "
<H1>Mid-air collision detected!</H1>
Someone else has made changes to this bug at the same time you were trying to.
The changes made were:
<p>
";
        DumpBugActivity($id, $delta_ts);
        my $longdesc = GetLongDescriptionAsHTML($id);
        my $longchanged = 0;

        if (length($longdesc) > $::FORM{'longdesclength'}) {
            $longchanged = 1;
            print "<P>Added text to the long description:<blockquote>";
            print substr($longdesc, $::FORM{'longdesclength'});
            print "</blockquote>\n";
        }
        SendSQL("unlock tables");
        print "You have the following choices: <ul>\n";
        $::FORM{'delta_ts'} = $delta_ts;
        print "<li><form method=post>";
        foreach my $i (keys %::FORM) {
            my $value = value_quote($::FORM{$i});
            print qq{<input type=hidden name="$i" value="$value">\n};
        }
        print qq{<input type=submit value="Submit my changes anyway">\n};
        print " This will cause all of the above changes to be overwritten";
        if ($longchanged) {
            print ", except for the changes to the description";
        }
        print qq{.</form>\n<li><a href="show_bug.cgi?id=$id">Throw away my changes, and go revisit bug $id</a></ul>\n};
        PutFooter();
        exit;
    }
        
    my %deps;
    if (defined $::FORM{'dependson'}) {
        my $me = "blocked";
        my $target = "dependson";
        for (1..2) {
            $deps{$target} = [];
            my %seen;
            foreach my $i (split('[\s,]+', $::FORM{$target})) {
                if ($i eq "") {
                    next;

                }
                SendSQL("select bug_id from bugs where bug_id = " .
                        SqlQuote($i));
                my $comp = FetchOneColumn();
                if ($comp ne $i) {
                    PuntTryAgain("$i is not a legal bug number");
                }
                if (!exists $seen{$i}) {
                    push(@{$deps{$target}}, $i);
                    $seen{$i} = 1;
                }
            }
            my @stack = @{$deps{$target}};
            while (@stack) {
                my $i = shift @stack;
                SendSQL("select $target from dependencies where $me = $i");
                while (MoreSQLData()) {
                    my $t = FetchOneColumn();
                    if ($t == $id) {
                        PuntTryAgain("Dependency loop detected!<P>" .
                                     "The change you are making to " .
                                     "dependencies has caused a circular " .
                                     "dependency chain.");
                    }
                    if (!exists $seen{$t}) {
                        push @stack, $t;
                        $seen{$t} = 1;
                    }
                }
            }
                        

            my $tmp = $me;
            $me = $target;
            $target = $tmp;
        }
    }

    if (@::legal_keywords) {
        # There are three kinds of "keywordsaction": makeexact, add, delete.
        # For makeexact, we delete everything, and then add our things.
        # For add, we delete things we're adding (to make sure we don't
        # end up having them twice), and then we add them.
        # For delete, we just delete things on the list.
        my $changed = 0;
        if ($keywordaction eq "makeexact") {
            SendSQL("DELETE FROM keywords WHERE bug_id = $id");
            $changed = 1;
        }
        foreach my $keyword (@keywordlist) {
            if ($keywordaction ne "makeexact") {
                SendSQL("DELETE FROM keywords
                         WHERE bug_id = $id AND keywordid = $keyword");
                $changed = 1;
            }
            if ($keywordaction ne "delete") {
                SendSQL("INSERT INTO keywords 
                         (bug_id, keywordid) VALUES ($id, $keyword)");
                $changed = 1;
            }
        }
        if ($changed) {
            SendSQL("SELECT keyworddefs.name 
                     FROM keyworddefs, keywords
                     WHERE keywords.bug_id = $id
                         AND keyworddefs.id = keywords.keywordid
                     ORDER BY keyworddefs.name");
            my @list;
            while (MoreSQLData()) {
                push(@list, FetchOneColumn());
            }
            SendSQL("UPDATE bugs SET keywords = " .
                    SqlQuote(join(', ', @list)) .
                    " WHERE bug_id = $id");
        }
    }

    my $query = "$basequery\nwhere bug_id = $id";
    
# print "<PRE>$query</PRE>\n";

    if ($::comma ne "") {
        SendSQL($query);
        SendSQL("select delta_ts from bugs where bug_id = $id");
    } else {
        SendSQL("select now()");
    }
    $timestamp = FetchOneColumn();
    
    if (defined $::FORM{'comment'}) {
        AppendComment($id, $::FORM{'who'}, $::FORM{'comment'});
    }
    
    if (defined $::FORM{'newcc'} && defined $::FORM{'id'}
        && ! $origCcSet->isEqual($formCcSet) ) {

        # update the database to look like the form
        #
        my @CCDELTAS = $origCcSet->generateSqlDeltas($formCcSet, "cc", 
                                                     "bug_id", $::FORM{'id'},
                                                     "who");
        $CCDELTAS[0] eq "" || SendSQL($CCDELTAS[0]);
        $CCDELTAS[1] eq "" || SendSQL($CCDELTAS[1]);

        my $col = GetFieldID('cc');
        my $origq = SqlQuote($origCcString);
        my $newq = SqlQuote($formCcSet->toString());
        SendSQL("INSERT INTO bugs_activity " . 
                "(bug_id,who,bug_when,fieldid,oldvalue,newvalue) VALUES " . 
                "($id,$whoid,'$timestamp',$col,$origq,$newq)");
    }
  

    if (defined $::FORM{'dependson'}) {
        my $me = "blocked";
        my $target = "dependson";
        for (1..2) {
            SendSQL("select $target from dependencies where $me = $id order by $target");
            my %snapshot;
            my @oldlist;
            while (MoreSQLData()) {
                push(@oldlist, FetchOneColumn());
            }
            my @newlist = sort {$a <=> $b} @{$deps{$target}};
            @dependencychanged{@oldlist} = 1;
            @dependencychanged{@newlist} = 1;

            while (0 < @oldlist || 0 < @newlist) {
                if (@oldlist == 0 || (@newlist > 0 &&
                                      $oldlist[0] > $newlist[0])) {
                    $snapshot{$newlist[0]} = SnapShotDeps($newlist[0], $me,
                                                          $target);
                    shift @newlist;
                } elsif (@newlist == 0 || (@oldlist > 0 &&
                                           $newlist[0] > $oldlist[0])) {
                    $snapshot{$oldlist[0]} = SnapShotDeps($oldlist[0], $me,
                                                          $target);
                    shift @oldlist;
                } else {
                    if ($oldlist[0] != $newlist[0]) {
                        die "Error in list comparing code";
                    }
                    shift @oldlist;
                    shift @newlist;
                }
            }
            my @keys = keys(%snapshot);
            if (@keys) {
                my $oldsnap = SnapShotDeps($id, $target, $me);
                SendSQL("delete from dependencies where $me = $id");
                foreach my $i (@{$deps{$target}}) {
                    SendSQL("insert into dependencies ($me, $target) values ($id, $i)");
                }
                foreach my $k (@keys) {
                    LogDependencyActivity($k, $snapshot{$k}, $me, $target);
                }
                LogDependencyActivity($id, $oldsnap, $target, $me);
            }

            my $tmp = $me;
            $me = $target;
            $target = $tmp;
        }
    }


    # get a snapshot of the newly set values out of the database, 
    # and then generate any necessary bug activity entries by seeing 
    # what has changed since before we wrote out the new values.
    #
    my @newvalues = SnapShotBug($id);

    foreach my $c (@::log_columns) {
        my $col = $c;           # We modify it, don't want to modify array
                                # values in place.
        my $old = shift @oldvalues;
        my $new = shift @newvalues;
        if (!defined $old) {
            $old = "";
        }
        if (!defined $new) {
            $new = "";
        }
        if ($old ne $new) {
            if ($col eq 'assigned_to' || $col eq 'qa_contact') {
                $old = ($old) ? DBID_to_name($old) : "";
                $new = ($new) ? DBID_to_name($new) : "";
                $origCcString .= ",$old"; # make sure to send mail to people
                                          # if they are going to no longer get
                                          # updates about this bug.
            }
            if ($col eq 'product') {
                RemoveVotes($id, 0,
                            "This bug has been moved to a different product");
            }
            $col = GetFieldID($col);
            $old = SqlQuote($old);
            $new = SqlQuote($new);
            my $q = "insert into bugs_activity (bug_id,who,bug_when,fieldid,oldvalue,newvalue) values ($id,$whoid,'$timestamp',$col,$old,$new)";
            # puts "<pre>$q</pre>"
            SendSQL($q);
        }
    }
    
    print "<TABLE BORDER=1><TD><H2>Changes to bug $id submitted</H2>\n";
    SendSQL("unlock tables");
    system("./processmail", "-forcecc", $origCcString, $id, $::FORM{'who'});
    print "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>\n";

    foreach my $k (keys(%dependencychanged)) {
        print "<TABLE BORDER=1><TD><H2>Checking for dependency changes on bug $k</H2>\n";
        system("./processmail", $k, $::FORM{'who'});
        print "<TD><A HREF=\"show_bug.cgi?id=$k\">Go To BUG# $k</A></TABLE>\n";
    }

}

if (defined $::next_bug) {
    print("<P>The next bug in your list is:\n");
    $::FORM{'id'} = $::next_bug;
    print "<HR>\n";

    navigation_header();
    do "bug_form.pl";
} else {
    navigation_header();
    PutFooter();
}
