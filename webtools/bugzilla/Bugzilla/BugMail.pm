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
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Bryce Nesbitt <bryce-mozilla@nextbus.com>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Alan Raetz <al_raetz@yahoo.com>
#                 Jacob Steenhagen <jake@actex.net>
#                 Matthew Tuck <matty@chariot.net.au>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 J. Paul Reed <preed@sigkill.com>

use strict;

package Bugzilla::BugMail;

use Bugzilla::RelationSet;

use Bugzilla::Config qw(:DEFAULT $datadir);
use Bugzilla::Util;

# This code is really ugly. It was a commandline interface, then it was moved
# There are package-global variables which we rely on ProcessOneBug to clean
# up each time, and other sorts of fun.
# This really needs to be cleaned at some point.

my $nametoexclude = "";
my %nomail;
my $last_changed;

my @excludedAddresses = ();

# disable email flag for offline debugging work
my $enableSendMail = 1;

my %force;

my %seen;
my @sentlist;

# I got sick of adding &:: to everything.
# However, 'Yuck!'
# I can't require, cause that pulls it in only once, so it won't then be
# in the global package, and these aren't modules, so I can't use globals.pl
# Remove this evilness once our stuff uses real packages.
sub AUTOLOAD {
    no strict 'refs';
    use vars qw($AUTOLOAD);
    my $subName = $AUTOLOAD;
    $subName =~ s/.*::/::/; # remove package name
    *$AUTOLOAD = \&$subName;
    goto &$AUTOLOAD;
}

# This is run when we load the package
if (open(NOMAIL, '<', "$datadir/nomail")) {
    while (<NOMAIL>) {
        $nomail{trim($_)} = 1;
    }
    close(NOMAIL);
}


sub FormatTriple {
    my ($a, $b, $c) = (@_);
    $^A = "";
    my $temp = formline << 'END', $a, $b, $c;
^>>>>>>>>>>>>>>>>>>|^<<<<<<<<<<<<<<<<<<<<<<<<<<<|^<<<<<<<<<<<<<<<<<<<<<<<<<<<~~
END
    ; # This semicolon appeases my emacs editor macros. :-)
    return $^A;
}
    
sub FormatDouble {
    my ($a, $b) = (@_);
    $a .= ":";
    $^A = "";
    my $temp = formline << 'END', $a, $b;
^>>>>>>>>>>>>>>>>>> ^<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<~~
END
    ; # This semicolon appeases my emacs editor macros. :-)
    return $^A;
}

# This is a bit of a hack, basically keeping the old system()
# cmd line interface. Should clean this up at some point.
#
# args: bug_id, and an optional hash ref which may have keys for:
# changer, owner, qa, reporter, cc
# Optional hash contains values of people which will be forced to those
# roles when the email is sent.
# All the names are email addresses, not userids
# values are scalars, except for cc, which is a list
sub Send($;$) {
    my ($id, $recipients) = (@_);

    # This only works in a sub. Probably something to do with the
    # require abuse we do.
    GetVersionTable();

    # Make sure to clean up _all_ package vars here. Yuck...
    $nametoexclude = $recipients->{'changer'} || "";
    @{$force{'CClist'}} = (exists $recipients->{'cc'} && 
     scalar($recipients->{'cc'}) > 0) ? map(trim($_), 
     @{$recipients->{'cc'}}) : ();
    @{$force{'Owner'}} = $recipients->{'owner'} ? 
     (trim($recipients->{'owner'})) : ();
    @{$force{'QAcontact'}} = $recipients->{'qacontact'} ? 
     (trim($recipients->{'qacontact'})) : ();
    @{$force{'Reporter'}} = $recipients->{'reporter'} ? 
     (trim($recipients->{'reporter'})) : ();
    @{$force{'Voter'}} = ();

    %seen = ();
    @excludedAddresses = ();
    @sentlist = ();

    return ProcessOneBug($id);
}

sub ProcessOneBug($) {
    my ($id) = (@_);

    my @headerlist;
    my %values;
    my %defmailhead;
    my %fielddescription;

    my $msg = "";

    SendSQL("SELECT name, description, mailhead FROM fielddefs " .
            "ORDER BY sortkey");
    while (MoreSQLData()) {
        my ($field, $description, $mailhead) = (FetchSQLData());
        push(@headerlist, $field);
        $defmailhead{$field} = $mailhead;
        $fielddescription{$field} = $description;
    }
    SendSQL("SELECT " . join(',', @::log_columns) . ", lastdiffed, now() " .
            "FROM bugs WHERE bug_id = $id");
    my @row = FetchSQLData();
    foreach my $i (@::log_columns) {
        $values{$i} = shift(@row);
    }
    $values{product} = get_product_name($values{product_id});
    $values{component} = get_component_name($values{component_id});

    my ($start, $end) = (@row);
    # $start and $end are considered safe because users can't touch them
    trick_taint($start);
    trick_taint($end);

    my $ccSet = new Bugzilla::RelationSet();
    $ccSet->mergeFromDB("SELECT who FROM cc WHERE bug_id = $id");
    $values{'cc'} = $ccSet->toString();
    
    my @voterList;
    SendSQL("SELECT profiles.login_name FROM votes, profiles " .
            "WHERE votes.bug_id = $id AND profiles.userid = votes.who");
    while (MoreSQLData()) {
        push(@voterList, FetchOneColumn());
    }

    $values{'assigned_to'} = DBID_to_name($values{'assigned_to'});
    $values{'reporter'} = DBID_to_name($values{'reporter'});
    if ($values{'qa_contact'}) {
        $values{'qa_contact'} = DBID_to_name($values{'qa_contact'});
    }
    $values{'estimated_time'} = FormatTimeUnit($values{'estimated_time'});

    my @dependslist;
    SendSQL("SELECT dependson FROM dependencies WHERE 
             blocked = $id ORDER BY dependson");
    while (MoreSQLData()) {
        push(@dependslist, FetchOneColumn());
    }
    $values{'dependson'} = join(",", @dependslist);

    my @blockedlist;
    SendSQL("SELECT blocked FROM dependencies WHERE 
             dependson = $id ORDER BY blocked");
    while (MoreSQLData()) {
        push(@blockedlist, FetchOneColumn());
    }
    $values{'blocked'} = join(",", @blockedlist);

    my @diffs;


    SendSQL("SELECT profiles.login_name, fielddefs.description, " .
            "       bug_when, removed, added, attach_id, fielddefs.name " .
            "FROM bugs_activity, fielddefs, profiles " .
            "WHERE bug_id = $id " .
            "  AND fielddefs.fieldid = bugs_activity.fieldid " .
            "  AND profiles.userid = who " .
            "  AND bug_when > '$start' " .
            "  AND bug_when <= '$end' " .
            "ORDER BY bug_when"
            );

    while (MoreSQLData()) {
        my @row = FetchSQLData();
        push(@diffs, \@row);
    }

    my $difftext = "";
    my $diffheader = "";
    my @diffparts;
    my $lastwho = "";
    foreach my $ref (@diffs) {
        my ($who, $what, $when, $old, $new, $attachid, $fieldname) = (@$ref);
        my $diffpart = {};
        if ($who ne $lastwho) {
            $lastwho = $who;
            $diffheader = "\n$who" . Param('emailsuffix') . " changed:\n\n";
            $diffheader .= FormatTriple("What    ", "Removed", "Added");
            $diffheader .= ('-' x 76) . "\n";
        }
        $what =~ s/^(Attachment )?/Attachment #$attachid / if $attachid;
        if( $fieldname eq 'estimated_time' ||
            $fieldname eq 'remaining_time' ) {
            $old = FormatTimeUnit($old);
            $new = FormatTimeUnit($new);
        }
        $difftext = FormatTriple($what, $old, $new);
        $diffpart->{'header'} = $diffheader;
        $diffpart->{'fieldname'} = $fieldname;
        $diffpart->{'text'} = $difftext;
        push(@diffparts, $diffpart);
    }


    my $deptext = "";

    SendSQL("SELECT bugs_activity.bug_id, bugs.short_desc, fielddefs.name, " .
            "       removed, added " .
            "FROM bugs_activity, bugs, dependencies, fielddefs ".
            "WHERE bugs_activity.bug_id = dependencies.dependson " .
            "  AND bugs.bug_id = bugs_activity.bug_id ".
            "  AND dependencies.blocked = $id " .
            "  AND fielddefs.fieldid = bugs_activity.fieldid" .
            "  AND (fielddefs.name = 'bug_status' " .
            "    OR fielddefs.name = 'resolution') " .
            "  AND bug_when > '$start' " .
            "  AND bug_when <= '$end' " .
            "ORDER BY bug_when, bug_id");
    
    my $thisdiff = "";
    my $lastbug = "";
    my $interestingchange = 0;
    my $depbug = 0;
    my @depbugs;
    while (MoreSQLData()) {
        my ($summary, $what, $old, $new);
        ($depbug, $summary, $what, $old, $new) = (FetchSQLData());
        if ($depbug ne $lastbug) {
            if ($interestingchange) {
                $deptext .= $thisdiff;
            }
            $lastbug = $depbug;
            my $urlbase = Param("urlbase");
            $thisdiff =
              "\nBug $id depends on bug $depbug, which changed state.\n\n" . 
              "Bug $depbug Summary: $summary\n" . 
              "${urlbase}show_bug.cgi?id=$depbug\n\n"; 
            $thisdiff .= FormatTriple("What    ", "Old Value", "New Value");
            $thisdiff .= ('-' x 76) . "\n";
            $interestingchange = 0;
        }
        $thisdiff .= FormatTriple($fielddescription{$what}, $old, $new);
        if ($what eq 'bug_status' && IsOpenedState($old) ne IsOpenedState($new)) {
            $interestingchange = 1;
        }
        
        push(@depbugs, $depbug);
    }
    
    if ($interestingchange) {
        $deptext .= $thisdiff;
    }

    $deptext = trim($deptext);

    if ($deptext) {
        my $diffpart = {};
        $diffpart->{'text'} = "\n" . trim("\n\n" . $deptext);
        push(@diffparts, $diffpart);
    }


    my ($newcomments, $anyprivate) = GetLongDescriptionAsText($id, $start, $end);

    #
    # Start of email filtering code
    #
    my $count = 0;

    # Get a list of the reasons a user might receive email about the bug.
    my @currentEmailAttributes = 
      getEmailAttributes(\%values, \@diffs, $newcomments);
    
    my (@assigned_toList,@reporterList,@qa_contactList,@ccList) = ();

    #open(LOG, ">>/tmp/maillog");
    #print LOG "\nBug ID: $id   CurrentEmailAttributes:";
    #print LOG join(',', @currentEmailAttributes) . "\n";

    @excludedAddresses = (); # zero out global list 

    @assigned_toList = filterEmailGroup('Owner',
                                        \@currentEmailAttributes,
                                        $values{'assigned_to'});
    @reporterList = filterEmailGroup('Reporter', 
                                     \@currentEmailAttributes,
                                     $values{'reporter'});
    if (Param('useqacontact') && $values{'qa_contact'}) {
        @qa_contactList = filterEmailGroup('QAcontact',
                                           \@currentEmailAttributes,
                                           $values{'qa_contact'});
    } else { 
        @qa_contactList = (); 
    }

    @ccList = filterEmailGroup('CClist', \@currentEmailAttributes,
                               $values{'cc'});

    @voterList = filterEmailGroup('Voter', \@currentEmailAttributes,
                                  join(',',@voterList));

    my @emailList = (@assigned_toList, @reporterList, 
                     @qa_contactList, @ccList, @voterList);

    # only need one entry per person
    my @allEmail = ();
    my %AlreadySeen = ();
    my $checkperson = "";
    foreach my $person (@emailList) {
        # don't modify the original so it sends out with the right case
        # based on who came first.
        $checkperson = lc($person);
        if ( !($AlreadySeen{$checkperson}) ) {
            push(@allEmail,$person);
            $AlreadySeen{$checkperson}++;
        }
    }

    #print LOG "\nbug $id email sent: " . join(',', @allEmail) . "\n";
        
    @excludedAddresses = filterExcludeList(\@excludedAddresses,
                                           \@allEmail);

    # print LOG "excluded: " . join(',',@excludedAddresses) . "\n\n";

    foreach my $person ( @allEmail ) {
        my @reasons;

        $count++;

        push(@reasons, 'AssignedTo') if lsearch(\@assigned_toList, $person) != -1;
        push(@reasons, 'Reporter') if lsearch(\@reporterList, $person) != -1;
        push(@reasons, 'QAcontact') if lsearch(\@qa_contactList, $person) != -1;
        push(@reasons, 'CC') if lsearch(\@ccList, $person) != -1;
        push(@reasons, 'Voter') if lsearch(\@voterList, $person) != -1;

        if ( !defined(NewProcessOnePerson($person, $count, \@headerlist,
                                          \@reasons, \%values,
                                          \%defmailhead, 
                                          \%fielddescription, \@diffparts,
                                          $newcomments, 
                                          $anyprivate, $start, $id, 
                                          \@depbugs))) 
        {

            # if a value is not returned, this means that the person
            # was not sent mail.  add them to the excludedAddresses list.
            # it will be filtered later for dups.
            #
            push @excludedAddresses, $person;

        }
    }


    SendSQL("UPDATE bugs SET lastdiffed = '$end', delta_ts = delta_ts " .
            "WHERE bug_id = $id");

    # Filter the exclude list for dupes one last time
    @excludedAddresses = filterExcludeList(\@excludedAddresses,
                                           \@sentlist);

    return { sent => \@sentlist, excluded => \@excludedAddresses };
}

# When one person is in different fields on one bug, they may be
# excluded from email because of one relationship to the bug (eg
# they're the QA contact) but included because of another (eg they
# also reported the bug).  Inclusion takes precedence, so this
# function looks for and removes any users from the exclude list who
# are also on the include list.  Additionally, it removes duplicate
# entries from the exclude list.  
#
# Arguments are the exclude list and the include list; the cleaned up
# exclude list is returned.
#
sub filterExcludeList ($$) {

    if ($#_ != 1) {
        die ("filterExcludeList called with wrong number of args");
    }

    my ($refExcluded, $refAll) = @_;

    my @excludedAddrs = @$refExcluded;
    my @allEmail = @$refAll;
    my @tmpList = @excludedAddrs;
    my (@result,@uniqueResult) = ();
    my %alreadySeen;

    foreach my $excluded (@tmpList) {

        push (@result,$excluded);
        foreach my $included (@allEmail) {

            # match found, so we remove the entry
            if (lc($included) eq lc($excluded)) {
                pop(@result);
                last;
            }
        }
    }

    # only need one entry per person
    my $checkperson = "";

    foreach my $person (@result) {
        $checkperson = lc($person);
        if ( !($alreadySeen{$checkperson}) ) {
            push(@uniqueResult,$person);
            $alreadySeen{$checkperson}++;
        }
    }

    return @uniqueResult;
}

# if the Status was changed to Resolved or Verified
#       set the Resolved flag
#
# else if Severity, Status, Target Milestone OR Priority fields have any change
#       set the Status flag
#
# else if Keywords has changed
#       set the Keywords flag
#
# else if CC has changed
#       set the CC flag
#
# if the Comments field shows an attachment
#       set the Attachment flag
#
# else if Comments exist
#       set the Comments flag
#
# if no flags are set and there was some other field change
#       set the Status flag
#
sub getEmailAttributes (\%\@$) {
    
    my ($bug, $fieldDiffs, $commentField) = @_;
    my (@flags,@uniqueFlags,%alreadySeen) = ();
    
    # Add a flag if the status of the bug is "unconfirmed".
    if ($bug->{'bug_status'} eq $::unconfirmedstate) {
        push (@flags, 'Unconfirmed')
    };
    
    foreach my $ref (@$fieldDiffs) {
        my ($who, $fieldName, $when, $old, $new) = (@$ref);
        
        #print qq{field: $fieldName $new<br>};
        
        # the STATUS will be flagged for Severity, Status, Target Milestone and 
        # Priority changes
        #
        if ( $fieldName eq 'Status' && ($new eq 'RESOLVED' || $new eq 'VERIFIED')) {
            push (@flags, 'Resolved');
        }
        elsif ( $fieldName eq 'Severity' || $fieldName eq 'Status' ||
                $fieldName eq 'Priority' || $fieldName eq 'Target Milestone') {
            push (@flags, 'Status');
        } elsif ( $fieldName eq 'Keywords') {
            push (@flags, 'Keywords');
        } elsif ( $fieldName eq 'CC') {
            push (@flags, 'CC');
        }

        # These next few lines find out who has been added
        # to the Owner, QA, CC, etc. fields.  They do not affect
        # the @flags array at all, but are run here because they
        # affect filtering later and we're already in the loop.
        if ($fieldName eq 'AssignedTo') {
            push (@{$force{'Owner'}}, $new);
        } elsif ($fieldName eq 'QAcontact') {
           push (@{$force{'QAcontact'}}, $new);
        } elsif ($fieldName eq 'CC') {
            my @added = split (/[ ,]/, $new);
            push (@{$force{'CClist'}}, @added);
        }
    }
    
    if ( $commentField =~ /Created an attachment \(/ ) {
        push (@flags, 'Attachments');
    }
    elsif ( ($commentField ne '') && !(scalar(@flags) == 1 && $flags[0] eq 'Resolved')) {
        push (@flags, 'Comments');
    }
    
    # default fallthrough for any unflagged change is 'Other'
    if ( @flags == 0 && @$fieldDiffs != 0 ) {
        push (@flags, 'Other');
    }
    
    # only need one flag per attribute type
    foreach my $flag (@flags) {
        if ( !($alreadySeen{$flag}) ) {
            push(@uniqueFlags,$flag);
            $alreadySeen{$flag}++;
        }
    }
    #print "\nEmail Attributes: ", join(' ,',@uniqueFlags), "<br>\n";
    
    # catch-all default, just in case the above logic is faulty
    if ( @uniqueFlags == 0) {
        push (@uniqueFlags, 'Comments');
    }
    
    return @uniqueFlags;
}

sub filterEmailGroup ($$$) {
    # This function figures out who should receive email about the bug
    # based on the user's role with respect to the bug (assignee, reporter, 
    # etc.), the changes that occurred (new comments, attachment added, 
    # status changed, etc.) and the user's email preferences.
    
    # Returns a filtered list of those users who would receive email
    # about these changes, and adds the names of those users who would
    # not receive email about them to the global @excludedAddresses list.
    
    my ($role, $reasons, $users) = @_;
    
    # Make a list of users to filter.
    my @users = split( /,/ , $users );
    
    # Treat users who are in the process of being removed from this role
    # as if they still have it.
    push @users, @{$force{$role}};

    # If this installation supports user watching, add to the list those
    # users who are watching other users already on the list.  By doing
    # this here, we allow watchers to inherit the roles of the people 
    # they are watching while at the same time filtering email for watchers
    # based on their own preferences.
    if (Param("supportwatchers") && scalar(@users)) {
        # Convert the unfiltered list of users into an SQL-quoted, 
        # comma-separated string suitable for use in an SQL query.
        my $watched = join(",", map(SqlQuote($_), @users));
        SendSQL("SELECT watchers.login_name 
                   FROM watch, profiles AS watchers, profiles AS watched
                  WHERE watched.login_name IN ($watched)
                    AND watched.userid = watch.watched
                    AND watch.watcher = watchers.userid");
        push (@users, FetchOneColumn()) while MoreSQLData();
    }

    # Initialize the list of recipients.
    my @recipients = ();

    USER: foreach my $user (@users) {
        next unless $user;
        
        # Get the user's unique ID, and if the user is not registered
        # (no idea why unregistered users should even be on this list,
        # but the code that was here before I re-wrote it allows this),
        # then we do not have any preferences for them, so assume the
        # default preference is to receive all mail.
        my $userid = DBname_to_id($user);
        if (!$userid) {
            push(@recipients, $user);
            next;
        }
        
        # Get the user's email preferences from the database.
        SendSQL("SELECT emailflags FROM profiles WHERE userid = $userid");
        my $prefs = FetchOneColumn();
        
        # If the user's preferences are empty, it means the user has not set
        # their mail preferences after the installation upgraded from a
        # version of Bugzilla without email preferences to one with them. In
        # this case, assume they want to receive all mail.
        if (!defined($prefs) || $prefs !~ /email/) {
            push(@recipients, $user);
            next;
        }
        
        # Write the user's preferences into a Perl record indexed by 
        # preference name.  We pass the value "255" to the split function 
        # because otherwise split will trim trailing null fields, causing 
        # Perl to generate lots of warnings.  Any suitably large number 
        # would do.
        my %prefs = split(/~/, $prefs, 255);
        
        # If this user is the one who made the change in the first place,
        # and they prefer not to receive mail about their own changes,
        # filter them from the list.
        if (lc($user) eq lc($nametoexclude) && $prefs{'ExcludeSelf'} eq 'on') {
            push(@excludedAddresses, $user);
            next;
        }
        
        # If the user doesn't want to receive email about unconfirmed
        # bugs, that setting overrides their other preferences, including
        # the preference to receive email when they are added to or removed
        # from a role, so remove them from the list before checking their
        # other preferences.
        if (grep(/Unconfirmed/, @$reasons)
            && exists($prefs{"email${role}Unconfirmed"})
            && $prefs{"email${role}Unconfirmed"} eq '')
        {
            push(@excludedAddresses, $user);
            next;
        }
        
        # If the user was added to or removed from this role, and they
        # prefer to receive email when that happens, send them mail.
        # Note: This was originally written to send email when users
        # were removed from roles and was later enhanced for additions,
        # but for simplicity's sake the name "Removeme" was retained.
        if (grep($_ eq $user, @{$force{$role}})
            && $prefs{"email${role}Removeme"} eq 'on')
        {
            push (@recipients, $user);
            next;
        }
        
        # If the user prefers to be included in mail about this change,
        # or they haven't specified a preference for it (because they
        # haven't visited the email preferences page since the preference
        # was added, in which case we include them by default), add them
        # to the list of recipients.
        foreach my $reason (@$reasons) {
            my $pref = "email$role$reason";
            if (!exists($prefs{$pref}) || $prefs{$pref} eq 'on') {
                push(@recipients, $user);
                next USER;
            }
        }
    
        # At this point there's no way the user wants to receive email
        # about this change, so exclude them from the list of recipients.
        push(@excludedAddresses, $user);
    
    } # for each user on the unfiltered list

    return @recipients;
}

sub NewProcessOnePerson ($$$$$$$$$$$$$) {
    my ($person, $count, $hlRef, $reasonsRef, $valueRef, $dmhRef, $fdRef,  
        $diffRef, $newcomments, $anyprivate, $start, 
        $id, $depbugsRef) = @_;

    my %values = %$valueRef;
    my @headerlist = @$hlRef;
    my @reasons = @$reasonsRef;
    my %defmailhead = %$dmhRef;
    my %fielddescription = %$fdRef;
    my @diffparts = @$diffRef;
    my @depbugs = @$depbugsRef;
    
    if ($seen{$person}) {
      return;
    }

    if ($nomail{$person}) {
      return;
    }

    # This routine should really get passed a userid
    # This rederives groups as a side effect
    my $user = Bugzilla::User->new_from_login($person);
    if (!$user) { # person doesn't exist, probably changed email
      return;
    }
    my $userid = $user->id;

    $seen{$person} = 1;

    # if this person doesn't have permission to see info on this bug, 
    # return.
    #
    # XXX - This currently means that if a bug is suddenly given
    # more restrictive permissions, people without those permissions won't
    # see the action of restricting the bug itself; the bug will just 
    # quietly disappear from their radar.
    #
    return unless $user->can_see_bug($id);

    #  Drop any non-insiders if the comment is private
    return if (Param("insidergroup") && 
               ($anyprivate != 0) && 
               (!$user->groups->{Param("insidergroup")}));

    # We shouldn't send changedmail if this is a dependency mail, and any of 
    # the depending bugs are not visible to the user.
    foreach my $dep_id (@depbugs) {
        my $save_id = $dep_id;
        detaint_natural($dep_id) || warn("Unexpected Error: \@depbugs contains a non-numeric value: '$save_id'")
                                 && return;
        return unless $user->can_see_bug($dep_id);
    }

    my %mailhead = %defmailhead;
    
    my $head = "";
    
    foreach my $f (@headerlist) {
      if ($mailhead{$f}) {
        my $value = $values{$f};
        # If there isn't anything to show, don't include this header
        if (! $value) {
          next;
        }
        # Only send estimated_time if it is enabled and the user is in the group
        if ($f ne 'estimated_time' ||
            $user->groups->{Param('timetrackinggroup')}) {

            my $desc = $fielddescription{$f};
            $head .= FormatDouble($desc, $value);
        }
      }
    }

    # Build difftext (the actions) by verifying the user should see them
    my $difftext = "";
    my $diffheader = "";
    my $add_diff;

    foreach my $diff (@diffparts) {
        $add_diff = 0;
        
        if (exists($diff->{'fieldname'}) && 
         ($diff->{'fieldname'} eq 'estimated_time' ||
         $diff->{'fieldname'} eq 'remaining_time' ||
         $diff->{'fieldname'} eq 'work_time')) {
            if ($user->groups->{Param("timetrackinggroup")}) {
                $add_diff = 1;
            }
        } else {
            $add_diff = 1;
        }

        if ($add_diff) {
            if (exists($diff->{'header'}) && 
             ($diffheader ne $diff->{'header'})) {
                $diffheader = $diff->{'header'};
                $difftext .= $diffheader;
            }
            $difftext .= $diff->{'text'};
        }
    }
 
    if ($difftext eq "" && $newcomments eq "") {
      # Whoops, no differences!
      return;
    }
    
    my $reasonsbody = "------- You are receiving this mail because: -------\n";

    if (scalar(@reasons) == 0) {
        $reasonsbody .= "Whoops!  I have no idea!\n";
    } else {
        foreach my $reason (@reasons) {
            if ($reason eq 'AssignedTo') {
                $reasonsbody .= "You are the assignee for the bug, or are watching the assignee.\n";
            } elsif ($reason eq 'Reporter') {
                $reasonsbody .= "You reported the bug, or are watching the reporter.\n";
            } elsif ($reason eq 'QAcontact') {
                $reasonsbody .= "You are the QA contact for the bug, or are watching the QA contact.\n";
            } elsif ($reason eq 'CC') {
                $reasonsbody .= "You are on the CC list for the bug, or are watching someone who is.\n";
            } elsif ($reason eq 'Voter') {
                $reasonsbody .= "You are a voter for the bug, or are watching someone who is.\n";
            } else {
                $reasonsbody .= "Whoops!  There is an unknown reason!\n";
            }
        }
    }

    my $isnew = ($start !~ m/[1-9]/);
    
    my %substs;

    # If an attachment was created, then add an URL. (Note: the 'g'lobal
    # replace should work with comments with multiple attachments.)

    if ( $newcomments =~ /Created an attachment \(/ ) {

        my $showattachurlbase =
            Param('urlbase') . "attachment.cgi?id=";

        $newcomments =~ s/(Created an attachment \(id=([0-9]+)\))/$1\n --> \(${showattachurlbase}$2&action=view\)/g;
    }

    $person .= Param('emailsuffix');
# 09/13/2000 cyeh@bluemartini.com
# If a bug is changed, don't put the word "Changed" in the subject mail
# since if the bug didn't change, you wouldn't be getting mail
# in the first place! see http://bugzilla.mozilla.org/show_bug.cgi?id=29820 
# for details.
    $substs{"neworchanged"} = $isnew ? ' New: ' : '';
    $substs{"to"} = $person;
    $substs{"cc"} = '';
    $substs{"bugid"} = $id;
    if ($isnew) {
      $substs{"diffs"} = $head . "\n\n" . $newcomments;
    } else {
      $substs{"diffs"} = $difftext . "\n\n" . $newcomments;
    }
    $substs{"summary"} = $values{'short_desc'};
    $substs{"reasonsheader"} = join(" ", @reasons);
    $substs{"reasonsbody"} = $reasonsbody;
    $substs{"space"} = " ";
    
    my $template = Param("newchangedmail");
    
    my $msg = PerformSubsts($template, \%substs);

    my $sendmailparam = "-ODeliveryMode=deferred";
    if (Param("sendmailnow")) {
       $sendmailparam = "";
    }

    if ($enableSendMail == 1) {
        open(SENDMAIL, "|/usr/lib/sendmail $sendmailparam -t -i") ||
          die "Can't open sendmail";
    
        print SENDMAIL trim($msg) . "\n";
        close SENDMAIL;
    }
    push(@sentlist, $person);
    return 1;
}

1;
