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
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>

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

######################################################################
# Begin Data/Security Validation
######################################################################

# Create a list of IDs of all bugs being modified in this request.
# This list will either consist of a single bug number from the "id"
# form/URL field or a series of numbers from multiple form/URL fields
# named "id_x" where "x" is the bug number.
my @idlist;
if (defined $::FORM{'id'}) {
    push @idlist, $::FORM{'id'};
} else {
    foreach my $i (keys %::FORM) {
        if ($i =~ /^id_([1-9][0-9]*)/) {
            push @idlist, $1;
        }
    }
}

# Make sure there are bugs to process.
scalar(@idlist)
  || DisplayError("You did not select any bugs to modify.")
  && exit;

# For each bug being modified, make sure its ID is a valid bug number 
# representing an existing bug that the user is authorized to access.
foreach my $id (@idlist) {
    ValidateBugID($id);
}

# If we are duping bugs, let's also make sure that we can change 
# the original.  This takes care of issue A on bug 96085.
if (defined $::FORM{'dup_id'} && $::FORM{'knob'} eq "duplicate") {
    ValidateBugID($::FORM{'dup_id'});

    # Also, let's see if the reporter has authorization to see the bug
    # to which we are duping.  If not we need to prompt.
    DuplicateUserConfirm();
}

# If the user has a bug list and is processing one bug, then after
# we process the bug we are going to show them the next bug on their
# list.  Thus we have to make sure this bug ID is also valid,
# since a malicious cracker might alter their cookies for the purpose
# gaining access to bugs they are not authorized to access.
if ( defined $::COOKIE{"BUGLIST"} && defined $::FORM{'id'} ) {
    my @buglist = split( /:/ , $::COOKIE{"BUGLIST"} );
    my $idx = lsearch( \@buglist , $::FORM{"id"} );
    if ($idx < $#buglist) {
        my $nextbugid = $buglist[$idx + 1];
        ValidateBugID($nextbugid);
    }
}

######################################################################
# End Data/Security Validation
######################################################################

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

ConnectToDatabase();

# Figure out whether or not the user is trying to change the product
# (either the "product" variable is not set to "don't change" or the
# user is changing a single bug and has changed the bug's product),
# and make the user verify the version, component, target milestone,
# and bug groups if so.
if ( $::FORM{'id'} ) {
    SendSQL("SELECT product FROM bugs WHERE bug_id = $::FORM{'id'}");
    $::oldproduct = FetchSQLData();
}
if ( ($::FORM{'id'} && $::FORM{'product'} ne $::oldproduct) 
       || (!$::FORM{'id'} && $::FORM{'product'} ne $::dontchange) ) {
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

    # If anything needs to be verified, generate a form for verifying it.
    if (!$vok || !$cok || !$mok || (Param('usebuggroups') && !defined($::FORM{'addtonewgroup'}))) {

        # Start the form.
        print qq|<form action="process_bug.cgi" method="post">\n|;

        # Add all form fields to the form as hidden fields (except those 
        # being verified), so the user's changes are preserved.
        foreach my $i (keys %::FORM) {
            if ($i ne 'version' && $i ne 'component' && $i ne 'target_milestone') {
                print qq|<input type="hidden" name="$i" value="| . value_quote($::FORM{$i}) . qq|">\n|;
            }
        }

        # Display UI for verifying the version, component, and target milestone fields.
        if (!$vok || !$cok || !$mok) {
            my ($sectiontitle, $sectiondescription);
            if ( Param('usetargetmilestone') ) {
                $sectiontitle = "Verify Version, Component, Target Milestone";
                $sectiondescription = qq|
                  You are moving the bug(s) to the product <b>$prod</b>, and now the 
                  version, component, and/or target milestone fields are not correct 
                  (or perhaps they were not correct in the first place).  In any case, 
                  please set the correct version, component, and target milestone now:
                |;
            } else {
                $sectiontitle = "Verify Version, Component";
                $sectiondescription = qq|
                  You are moving the bug(s) to the product <b>$prod</b>, and now the 
                  version, and component fields are not correct (or perhaps they were 
                  not correct in the first place).  In any case, please set the correct 
                  version and component now:
                |;
            }

            my $versionmenu = Version_element($::FORM{'version'}, $prod);
            my $componentmenu = Component_element($::FORM{'component'}, $prod);

            print qq|
              <h3>$sectiontitle</h3>

              <p>
                $sectiondescription
              <p>

              <table><tr>
              <td>
                <b>Version:</b><br>
                $versionmenu
              </td>
              <td>
                <b>Component:</b><br>
                $componentmenu
              </td>
            |;

            if ( Param("usetargetmilestone") ) {
                my $milestonemenu = Milestone_element($::FORM{'target_milestone'}, $prod);
                print qq|
                  <td>
                    <b>Target Milestone:</b><br>
                    $milestonemenu
                  </td>
                |;
            }

            print qq|
              </tr></table>
            |;
        }

        # Display UI for determining whether or not to remove the bug from 
        # its old product's group and/or add it to its new product's group.
        if (Param('usebuggroups') && !defined($::FORM{'addtonewgroup'})) {
            print qq|
              <h3>Verify Bug Group</h3>

              <p>
                Do you want to add the bug to its new product's group (if any)?
              </p>

              <p>
                <input type="radio" name="addtonewgroup" value="no"><b>no</b><br>
                <input type="radio" name="addtonewgroup" value="yes"><b>yes</b><br>
                <input type="radio" name="addtonewgroup" value="yesifinold" checked>
                  <b>yes, but only if the bug was in its old product's group</b><br>
              </p>
            |;
        }

        # End the form.
        print qq|
          <input type="submit" value="Commit">
          </form>
          <hr>
          <a href="query.cgi">Cancel and Return to the Query Page</a>
        |;

        # End the page and stop processing.
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
    if ($f eq "resolution") { # always OK this.  if they really can't,
        return 1;             # it'll flag it when "status" is checked.
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

# Confirm that the reporter of the current bug can access the bug we are duping to.
sub DuplicateUserConfirm {
    # if we've already been through here, then exit
    if (defined $::FORM{'confirm_add_duplicate'}) {
        return;
    }

    my $dupe = trim($::FORM{'id'});
    my $original = trim($::FORM{'dup_id'});
    
    SendSQL("SELECT reporter FROM bugs WHERE bug_id = " . SqlQuote($dupe));
    my $reporter = FetchOneColumn();
    SendSQL("SELECT profiles.groupset FROM profiles WHERE profiles.userid =".SqlQuote($reporter));
    my $reportergroupset = FetchOneColumn();

    if (CanSeeBug($original, $reporter, $reportergroupset)) {
        $::FORM{'confirm_add_duplicate'} = "1";
        return;
    }

    SendSQL("SELECT cclist_accessible FROM bugs WHERE bug_id = $original");
    my $cclist_accessible = FetchOneColumn();
    
    # Once in this part of the subroutine, the user has not been auto-validated
    # and the duper has not chosen whether or not to add to CC list, so let's
    # ask the duper what he/she wants to do.
    
    # First, will the user gain access to this bug immediately by being CC'd?
    my $reporter_access = $cclist_accessible ? "will immediately" : "might, in the future,";

    print "Content-type: text/html\n\n";
    PutHeader("Duplicate Warning");
    print "<P>
When marking a bug as a duplicate, the reporter of the 
duplicate is normally added to the CC list of the original. 
The permissions on bug #$original (the original) are currently set 
such that the reporter would not normally be able to see it. 
<P><B>Adding the reporter to the CC list of bug #$original 
$reporter_access allow him/her access to view this bug.</B>
Do you wish to do this?</P>
</P>
";
    print "<form method=post>\n\n";

    foreach my $i (keys %::FORM) {
        # Make sure we don't include the username/password fields in the
        # HTML.  If cookies are off, they'll have to reauthenticate after
        # hitting "submit changes anyway".
        # see http://bugzilla.mozilla.org/show_bug.cgi?id=15980
        if ($i !~ /^(Bugzilla|LDAP)_(login|password)$/) {
            my $value = value_quote($::FORM{$i});
            print qq{<input type=hidden name="$i" value="$value">\n};
        }
    }

    print qq{<p><input type=radio name="confirm_add_duplicate" value="1"> Yes, add the reporter to CC list on bug $original</p>\n};
    print qq{<p><input type=radio name="confirm_add_duplicate" value="0" checked="checked"> No, do not add the reporter to CC list on bug $original</p>\n};
    print qq{\n<p><a href="show_bug.cgi?id=$dupe">Throw away my changes, and go revisit bug $dupe</a>\n};
    print qq{\n<p><input type="submit" value="Submit"></p></form>\n};
    PutFooter();
    exit;
} # end DuplicateUserConfirm()

if (defined $::FORM{'id'} && Param('strictvaluechecks')) {
    # since this means that we were called from show_bug.cgi, now is a good
    # time to do a whole bunch of error checking that can't easily happen when
    # we've been called from buglist.cgi, because buglist.cgi only tweaks
    # values that have been changed instead of submitting all the new values.
    # (XXX those error checks need to happen too, but implementing them 
    # is more work in the current architecture of this script...)
    #
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
        # Ugly, but functional.  We don't want to change Status if we are
        # reasigning non-open bugs via the mass change form.
        if ( ($::FORM{knob} eq 'reassign' || $::FORM{knob} eq 'reassignbycomponent') &&
             ! defined $::FORM{id} && $str eq 'NEW' ) {
            # If we got to here, we're dealing with a reassign from the mass
            # change page.  We don't know (and can't easily figure out) if this
            # bug is open or closed.  If it's closed, we don't want to change
            # its status to NEW.  We have to put some logic into the SQL itself
            # to handle that.
            my @open_state = map(SqlQuote($_), OpenStates());
            my $open_state = join(", ", @open_state);
            $::query .= "bug_status = IF(bug_status IN($open_state), '$str', bug_status)";
        } elsif (IsOpenedState($str)) {
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

# Changing this so that it will process groups from checkboxes instead of
# select lists.  This means that instead of looking for the bit-X values in
# the form, we need to loop through all the bug groups this user has access
# to, and for each one, see if it's selected.
# In addition, adding a little extra work so that we don't clobber groupsets
# for bugs where the user doesn't have access to the group, but does to the
# bug (as with the proposed reporter access patch.)
if($::usergroupset ne '0') {
  # We want to start from zero and build up, since if all boxes have been
  # unchecked, we want to revert to 0.
  DoComma();
  $::query .= "groupset = 0";
  my ($id) = (@idlist);
  SendSQL(<<_EOQ_);
    SELECT bit, bit & $::usergroupset != 0, bit & bugs.groupset != 0
    FROM groups, bugs
    WHERE isbuggroup != 0 AND bug_id = $id
    ORDER BY bit
_EOQ_
  while (my ($b, $userhasgroup, $bughasgroup) = FetchSQLData()) {
    if (!$::FORM{"bit-$b"}) {
      # If we make it here, the item didn't exist on the form or the user
      # said to clear it.  The only time we add this group back in is if
      # the bug already has this group on it and the user can't access it.
      if ($bughasgroup && !$userhasgroup) {
        $::query .= " + $b";
      }
    } elsif ($::FORM{"bit-$b"} == -1) {
      # If we get here, the user came from the change several bugs form, and
      # said not to change this group restriction.  So we'll add this group
      # back in only if the bug already has it.
      if ($bughasgroup) {
        $::query .= " + $b";
      }
    } else {
      # If we get here, the user said to set this group.  If they don't have
      # access to it, we'll use what's already on the bug, otherwise we'll
      # add this one in.
      if ($userhasgroup || $bughasgroup) {
        $::query .= " + $b";
      }
    }
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


# If the user is submitting changes from show_bug.cgi for a single bug,
# and that bug is restricted to a group, process the checkboxes that
# allowed the user to set whether or not the reporter, assignee, QA contact, 
# and cc list can see the bug even if they are not members of all groups 
# to which the bug is restricted.
if ( $::FORM{'id'} ) {
    SendSQL("SELECT groupset FROM bugs WHERE bug_id = $::FORM{'id'}");
    my ($groupset) = FetchSQLData();
    if ( $groupset ) {
        DoComma();
        $::FORM{'reporter_accessible'} = $::FORM{'reporter_accessible'} ? '1' : '0';
        $::query .= "reporter_accessible = $::FORM{'reporter_accessible'}";

        DoComma();
        $::FORM{'assignee_accessible'} = $::FORM{'assignee_accessible'} ? '1' : '0';
        $::query .= "assignee_accessible = $::FORM{'assignee_accessible'}";

        DoComma();
        $::FORM{'qacontact_accessible'} = $::FORM{'qacontact_accessible'} ? '1' : '0';
        $::query .= "qacontact_accessible = $::FORM{'qacontact_accessible'}";

        DoComma();
        $::FORM{'cclist_accessible'} = $::FORM{'cclist_accessible'} ? '1' : '0';
        $::query .= "cclist_accessible = $::FORM{'cclist_accessible'}";
    }
}


my $duplicate = 0;

# We need to check the addresses involved in a CC change before we touch any bugs.
# What we'll do here is formulate the CC data into two hashes of ID's involved
# in this CC change.  Then those hashes can be used later on for the actual change.
my (%cc_add, %cc_remove);
if (defined $::FORM{newcc} || defined $::FORM{removecc} || defined $::FORM{masscc}) {
    # If masscc is defined, then we came from buglist and need to either add or
    # remove cc's... otherwise, we came from bugform and may need to do both.
    my ($cc_add, $cc_remove) = "";
    if (defined $::FORM{masscc}) {
        if ($::FORM{ccaction} eq 'add') {
            $cc_add = $::FORM{masscc};
        } elsif ($::FORM{ccaction} eq 'remove') {
            $cc_remove = $::FORM{masscc};
        }
    } else {
        $cc_add = $::FORM{newcc};
        # We came from bug_form which uses a select box to determine what cc's
        # need to be removed...
        if (defined $::FORM{removecc} && $::FORM{cc}) {
            $cc_remove = join (",", @{$::MFORM{cc}});
        }
    }

    if ($cc_add) {
        $cc_add =~ s/[\s,]+/ /g; # Change all delimiters to a single space
        foreach my $person ( split(" ", $cc_add) ) {
            my $pid = DBNameToIdAndCheck($person);
            $cc_add{$pid} = $person;
        }
    }
    if ($cc_remove) {
        $cc_remove =~ s/[\s,]+/ /g; # Change all delimiters to a single space
        foreach my $person ( split(" ", $cc_remove) ) {
            my $pid = DBNameToIdAndCheck($person);
            $cc_remove{$pid} = $person;
        }
    }
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
        my $newid = FetchOneColumn();
        my $newname = DBID_to_name($newid);
        DoComma();
        $::query .= "assigned_to = $newid";
        if (Param("useqacontact")) {
            SendSQL("select initialqacontact from components where program=" .
                    SqlQuote($::FORM{'product'}) .
                    " and value=" . SqlQuote($::FORM{'component'}));
            my $qacontact = FetchOneColumn();
            if (defined $qacontact && $qacontact != 0) {
                DoComma();
                $::query .= "qa_contact = $qacontact";
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
            PuntTryAgain("You must specify a valid bug number of which this bug " .
                         "is a duplicate.  The bug has not been changed.")
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
        $::FORM{'comment'} .= "\n\n*** This bug has been marked as a duplicate of $num ***";
        $duplicate = $num;

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
            PuntTryAgain("Unknown keyword named <code>" .
                         html_quote($keyword) . "</code>. " .
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

sub FindWrapPoint {
    my ($string, $startpos) = @_;
    if (!$string) { return 0 }
    if (length($string) < $startpos) { return length($string) }
    my $wrappoint = rindex($string, ",", $startpos); # look for comma
    if ($wrappoint < 0) {  # can't find comma
        $wrappoint = rindex($string, " ", $startpos); # look for space
        if ($wrappoint < 0) {  # can't find space
            $wrappoint = rindex($string, "-", $startpos); # look for hyphen
            if ($wrappoint < 0) {  # can't find hyphen
                $wrappoint = $startpos;  # just truncate it
            } else {
                $wrappoint++; # leave hyphen on the left side
            }
        }
    }
    return $wrappoint;
}

sub LogActivityEntry {
    my ($i,$col,$removed,$added) = @_;
    # in the case of CCs, deps, and keywords, there's a possibility that someone
    # might try to add or remove a lot of them at once, which might take more
    # space than the activity table allows.  We'll solve this by splitting it
    # into multiple entries if it's too long.
    while ($removed || $added) {
        my ($removestr, $addstr) = ($removed, $added);
        if (length($removestr) > 254) {
            my $commaposition = FindWrapPoint($removed, 254);
            $removestr = substr($removed,0,$commaposition);
            $removed = substr($removed,$commaposition);
            $removed =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $removed = ""; # no more entries
        }
        if (length($addstr) > 254) {
            my $commaposition = FindWrapPoint($added, 254);
            $addstr = substr($added,0,$commaposition);
            $added = substr($added,$commaposition);
            $added =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $added = ""; # no more entries
        }
        $addstr = SqlQuote($addstr);
        $removestr = SqlQuote($removestr);
        my $fieldid = GetFieldID($col);
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($i,$whoid,$timestamp,$fieldid,$removestr,$addstr)");
    }
}

sub LogDependencyActivity {
    my ($i, $oldstr, $target, $me) = (@_);
    my $newstr = SnapShotDeps($i, $target, $me);
    if ($oldstr ne $newstr) {
        # Figure out what's really different...
        my ($removed, $added) = DiffStrings($oldstr, $newstr);
        LogActivityEntry($i,$target,$removed,$added);
        return 1;
    }
    return 0;
}

# this loop iterates once for each bug to be processed (eg when this script
# is called with multiple bugs selected from buglist.cgi instead of
# show_bug.cgi).
#
foreach my $id (@idlist) {
    my %dependencychanged;
    my $write = "WRITE";        # Might want to make a param to control
                                # whether we do LOW_PRIORITY ...
    SendSQL("LOCK TABLES bugs $write, bugs_activity $write, cc $write, " .
            "cc AS selectVisible_cc $write, " .
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
            # Make sure we don't include the username/password fields in the
            # HTML.  If cookies are off, they'll have to reauthenticate after
            # hitting "submit changes anyway".
            # see http://bugzilla.mozilla.org/show_bug.cgi?id=15980
            if ($i !~ /^(Bugzilla|LDAP)_(login|password)$/) {
              my $value = value_quote($::FORM{$i});
              print qq{<input type=hidden name="$i" value="$value">\n};
            }
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
                if ($id eq $i) {
                    PuntTryAgain("You can't make a bug blocked or dependent on itself.");
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

            if ($me eq 'dependson') {
                my @deps   =  @{$deps{'dependson'}};
                my @blocks =  @{$deps{'blocked'}};
                my @union = ();
                my @isect = ();
                my %union = ();
                my %isect = ();
                foreach my $b (@deps, @blocks) { $union{$b}++ && $isect{$b}++ }
                @union = keys %union;
                @isect = keys %isect;
                if (@isect > 0) {
                    my $both;
                    foreach my $i (@isect) {
                       $both = $both . "#" . $i . " ";
                    }
                    PuntTryAgain("Dependency loop detected!<P>" .
                                 "This bug can't be both blocked and dependent " .
                                 "on bug "  . $both . "!");
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
    
    my $removedCcString = "";
    if (defined $::FORM{newcc} || defined $::FORM{removecc} || defined $::FORM{masscc}) {
        # Get the current CC list for this bug
        my %oncc;
        SendSQL("SELECT who FROM cc WHERE bug_id = $id");
        while (MoreSQLData()) {
            $oncc{FetchOneColumn()} = 1;
        }

        my (@added, @removed) = ();
        foreach my $pid (keys %cc_add) {
            # If this person isn't already on the cc list, add them
            if (! $oncc{$pid}) {
                SendSQL("INSERT INTO cc (bug_id, who) VALUES ($id, $pid)");
                push (@added, $cc_add{$pid});
                $oncc{$pid} = 1;
            }
        }
        foreach my $pid (keys %cc_remove) {
            # If the person is on the cc list, remove them
            if ($oncc{$pid}) {
                SendSQL("DELETE FROM cc WHERE bug_id = $id AND who = $pid");
                push (@removed, $cc_remove{$pid});
                $oncc{$pid} = 0;
            }
        }
        # Save off the removedCcString so it can be fed to processmail
        $removedCcString = join (",", @removed);

        # If any changes were found, record it in the activity log
        if (scalar(@removed) || scalar(@added)) {
            my $removed = join(", ", @removed);
            my $added = join(", ", @added);
            LogActivityEntry($id,"cc",$removed,$added);
        }
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

    # When a bug changes products and the old or new product is associated
    # with a bug group, it may be necessary to remove the bug from the old
    # group or add it to the new one.  There are a very specific series of
    # conditions under which these activities take place, more information
    # about which can be found in comments within the conditionals below.
    if ( 
      # the "usebuggroups" parameter is on, indicating that products
      # are associated with groups of the same name;
      Param('usebuggroups')

      # the user has changed the product to which the bug belongs;
      && defined $::FORM{'product'} 
        && $::FORM{'product'} ne $::dontchange 
          && $::FORM{'product'} ne $oldhash{'product'} 
    ) {
        if (
          # the user wants to add the bug to the new product's group;
          ($::FORM{'addtonewgroup'} eq 'yes' 
            || ($::FORM{'addtonewgroup'} eq 'yesifinold' 
                  && GroupNameToBit($oldhash{'product'}) & $oldhash{'groupset'})) 

          # the new product is associated with a group;
          && GroupExists($::FORM{'product'})

          # the bug is not already in the group; (This can happen when the user
          # goes to the "edit multiple bugs" form with a list of bugs at least
          # one of which is in the new group.  In this situation, the user can
          # simultaneously change the bugs to a new product and move the bugs
          # into that product's group, which happens earlier in this script
          # and thus is already done.  If we didn't check for this, then this
          # situation would cause us to add the bug to the group twice, which
          # would result in the bug being added to a totally different group.)
          && !BugInGroup($id, $::FORM{'product'})

          # the user is a member of the associated group, indicating they
          # are authorized to add bugs to that group, *or* the "usebuggroupsentry"
          # parameter is off, indicating that users can add bugs to a product 
          # regardless of whether or not they belong to its associated group;
          && (UserInGroup($::FORM{'product'}) || !Param('usebuggroupsentry'))

          # the associated group is active, indicating it can accept new bugs;
          && GroupIsActive(GroupNameToBit($::FORM{'product'}))
        ) { 
            # Add the bug to the group associated with its new product.
            my $groupbit = GroupNameToBit($::FORM{'product'});
            SendSQL("UPDATE bugs SET groupset = groupset + $groupbit WHERE bug_id = $id");
        }

        if ( 
          # the old product is associated with a group;
          GroupExists($oldhash{'product'})

          # the bug is a member of that group;
          && BugInGroup($id, $oldhash{'product'}) 
        ) { 
            # Remove the bug from the group associated with its old product.
            my $groupbit = GroupNameToBit($oldhash{'product'});
            SendSQL("UPDATE bugs SET groupset = groupset - $groupbit WHERE bug_id = $id");
        }

        print qq|</p>|;
    }
  
    # get a snapshot of the newly set values out of the database, 
    # and then generate any necessary bug activity entries by seeing 
    # what has changed since before we wrote out the new values.
    #
    my @newvalues = SnapShotBug($id);

    # for passing to processmail to ensure that when someone is removed
    # from one of these fields, they get notified of that fact (if desired)
    #
    my $origOwner = "";
    my $origQaContact = "";

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

            # save off the old value for passing to processmail so the old
            # owner can be notified
            #
            if ($col eq 'assigned_to') {
                $old = ($old) ? DBID_to_name($old) : "";
                $new = ($new) ? DBID_to_name($new) : "";
                $origOwner = $old;
            }

            # ditto for the old qa contact
            #
            if ($col eq 'qa_contact') {
                $old = ($old) ? DBID_to_name($old) : "";
                $new = ($new) ? DBID_to_name($new) : "";
                $origQaContact = $old;
            }

            # If this is the keyword field, only record the changes, not everything.
            if ($col eq 'keywords') {
                ($old, $new) = DiffStrings($old, $new);
            }

            if ($col eq 'product') {
                RemoveVotes($id, 0,
                            "This bug has been moved to a different product");
            }
            LogActivityEntry($id,$col,$old,$new);
        }
    }
    
    print "<TABLE BORDER=1><TD><H2>Changes to bug $id submitted</H2>\n";
    SendSQL("unlock tables");

    my @ARGLIST = ();
    if ( $removedCcString ne "" ) {
        push @ARGLIST, ("-forcecc", $removedCcString);
    }
    if ( $origOwner ne "" ) {
        push @ARGLIST, ("-forceowner", $origOwner);
    }
    if ( $origQaContact ne "") { 
        push @ARGLIST, ( "-forceqacontact", $origQaContact);
    }
    push @ARGLIST, ($id, $::FORM{'who'});
    system ("./processmail",@ARGLIST);

    print "<TD><A HREF=\"show_bug.cgi?id=$id\">Back To BUG# $id</A></TABLE>\n";

    if ($duplicate) {
        # Check to see if Reporter of this bug is reporter of Dupe 
        SendSQL("SELECT reporter FROM bugs WHERE bug_id = " . SqlQuote($::FORM{'id'}));
        my $reporter = FetchOneColumn();
        SendSQL("SELECT reporter FROM bugs WHERE bug_id = " . SqlQuote($duplicate) . " and reporter = $reporter");
        my $isreporter = FetchOneColumn();
        SendSQL("SELECT who FROM cc WHERE bug_id = " . SqlQuote($duplicate) . " and who = $reporter");
        my $isoncc = FetchOneColumn();
        unless ($isreporter || $isoncc || ! $::FORM{'confirm_add_duplicate'}) {
            # The reporter is oblivious to the existance of the new bug and is permitted access
            # ... add 'em to the cc (and record activity)
            LogActivityEntry($duplicate,"cc","",DBID_to_name($reporter));
            SendSQL("INSERT INTO cc (who, bug_id) VALUES ($reporter, " . SqlQuote($duplicate) . ")");
        }
        AppendComment($duplicate, $::FORM{'who'}, "*** Bug $::FORM{'id'} has been marked as a duplicate of this bug. ***");
        if ( Param('strictvaluechecks') ) {
          CheckFormFieldDefined(\%::FORM,'comment');
        }
        SendSQL("INSERT INTO duplicates VALUES ($duplicate, $::FORM{'id'})");
        print "<TABLE BORDER=1><TD><H2>Duplicate notation added to bug $duplicate</H2>\n";
        system("./processmail", $duplicate, $::FORM{'who'});
        print "<TD><A HREF=\"show_bug.cgi?id=$duplicate\">Go To BUG# $duplicate</A></TABLE>\n";
    }

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
