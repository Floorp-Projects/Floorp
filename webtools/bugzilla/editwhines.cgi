#!/usr/bin/perl -wT
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
# Contributor(s): Erik Stambaugh <erik@dasbistro.com>
#

################################################################################
# Script Initialization
################################################################################

use strict;

use lib ".";
require "CGI.pl";
require "globals.pl";

use vars qw( $vars );

use Bugzilla::Constants;
use Bugzilla::User;
# require the user to have logged in
Bugzilla->login(LOGIN_REQUIRED);

###############################################################################
# Main Body Execution
###############################################################################

my $cgi      = Bugzilla->cgi;
my $template = Bugzilla->template;
my $dbh      = Bugzilla->dbh;

my $user     = Bugzilla->user;
my $userid   = $user->id;

my $sth; # database statement handle

# $events is a hash ref, keyed by event id, that stores the active user's
# events.  It starts off with:
#  'subject' - the subject line for the email message
#  'body'    - the text to be sent at the top of the message
#
# Eventually, it winds up with:
#  'queries'  - array ref containing hashes of:
#       'name'          - the name of the saved query
#       'title'         - The title line for the search results table
#       'sort'          - Numeric sort ID
#       'id'            - row ID for the query entry
#       'onemailperbug' - whether a single message must be sent for each
#                         result.
#  'schedule' - array ref containing hashes of:
#       'day' - Day or range of days this schedule will be run
#       'time' - time or interval to run
#       'mailto' - person who will receive the results
#       'id' - row ID for the schedule
my $events = get_events($userid);

# First see if this user may use whines
ThrowUserError('whine_access_denied') unless (UserInGroup('bz_canusewhines'));

# May this user send mail to other users?
my $can_mail_others = UserInGroup('bz_canusewhineatothers');

# If the form was submitted, we need to look for what needs to be added or
# removed, then what was altered.

if ($cgi->param('update')) {
    if ($cgi->param("add_event")) {
        # we create a new event
        $sth = $dbh->prepare("INSERT INTO whine_events " .
                             "(owner_userid) " .
                             "VALUES (?)");
        $sth->execute($userid);
    }
    else {
        for my $eventid (keys %{$events}) {
            # delete an entire event
            if ($cgi->param("remove_event_$eventid")) {
                # We need to make sure these belong to the same user,
                # otherwise we could simply delete whatever matched that ID.
                #
                # schedules
                $sth = $dbh->prepare("SELECT whine_schedules.id " .
                                     "FROM whine_schedules " .
                                     "LEFT JOIN whine_events " .
                                     "ON whine_events.id = " .
                                     "whine_schedules.eventid " .
                                     "WHERE whine_events.id = ? " .
                                     "AND whine_events.owner_userid = ?");
                $sth->execute($eventid, $userid);
                my @ids = @{$sth->fetchall_arrayref};
                $sth = $dbh->prepare("DELETE FROM whine_schedules "
                    . "WHERE id=?");
                for (@ids) {
                    my $delete_id = $_->[0];
                    $sth->execute($delete_id);
                }

                # queries
                $sth = $dbh->prepare("SELECT whine_queries.id " .
                                     "FROM whine_queries " .
                                     "LEFT JOIN whine_events " .
                                     "ON whine_events.id = " .
                                     "whine_queries.eventid " .
                                     "WHERE whine_events.id = ? " .
                                     "AND whine_events.owner_userid = ?");
                $sth->execute($eventid, $userid);
                @ids = @{$sth->fetchall_arrayref};
                $sth = $dbh->prepare("DELETE FROM whine_queries " .
                                     "WHERE id=?");
                for (@ids) {
                    my $delete_id = $_->[0];
                    $sth->execute($delete_id);
                }

                # events
                $sth = $dbh->prepare("DELETE FROM whine_events " .
                                     "WHERE id=? AND owner_userid=?");
                $sth->execute($eventid, $userid);
            }
            else {
                # check the subject and body for changes
                my $subject = ($cgi->param("event_${eventid}_subject") or '');
                my $body    = ($cgi->param("event_${eventid}_body")    or '');

                trick_taint($subject) if $subject;
                trick_taint($body)    if $body;

                if ( ($subject ne $events->{$eventid}->{'subject'})
                  || ($body    ne $events->{$eventid}->{'body'}) ) {

                    $sth = $dbh->prepare("UPDATE whine_events " .
                                         "SET subject=?, body=? " .
                                         "WHERE id=?");
                    $sth->execute($subject, $body, $eventid);
                }

                # add a schedule
                if ($cgi->param("add_schedule_$eventid")) {
                    # the schedule table must be locked before altering
                    $sth = $dbh->prepare("INSERT INTO whine_schedules " .
                                         "(eventid, mailto_userid, " .
                                         "run_day, run_time) " .
                                         "VALUES (?, ?, 'Sun', 2)");
                    $sth->execute($eventid, $userid);
                }
                # add a query
                elsif ($cgi->param("add_query_$eventid")) {
                    $sth = $dbh->prepare("INSERT INTO whine_queries "
                        . "(eventid) "
                        . "VALUES (?)");
                    $sth->execute($eventid);
                }
            }

            # now check all of the schedules and queries to see if they need
            # to be altered or deleted

            # Check schedules for changes
            $sth = $dbh->prepare("SELECT id " .
                                 "FROM whine_schedules " .
                                 "WHERE eventid=?");
            $sth->execute($eventid);
            my @scheduleids = ();
            for (@{$sth->fetchall_arrayref}) {
                push @scheduleids, $_->[0];
            };

            # we need to double-check all of the user IDs in mailto to make
            # sure they exist
            my $arglist = {};   # args for match_field
            for my $sid (@scheduleids) {
                $arglist->{"mailto_$sid"} = {
                    'type' => 'single',
                };
            }
            if (scalar %{$arglist}) {
                &Bugzilla::User::match_field($arglist);
            }

            for my $sid (@scheduleids) {
                if ($cgi->param("remove_schedule_$sid")) {
                    # having the owner id in here is a security failsafe
                    $sth = $dbh->prepare("SELECT whine_schedules.id " .
                                         "FROM whine_schedules " .
                                         "LEFT JOIN whine_events " .
                                         "ON whine_events.id = " .
                                         "whine_schedules.eventid " .
                                         "WHERE whine_events.owner_userid=? " .
                                         "AND whine_schedules.id =?");
                    $sth->execute($userid, $sid);

                    my @ids = @{$sth->fetchall_arrayref};
                    for (@ids) {
                        $sth = $dbh->prepare("DELETE FROM whine_schedules " .
                                             "WHERE id=?");
                        $sth->execute($_->[0]);
                    }
                }
                else {
                    my $o_day    = $cgi->param("orig_day_$sid");
                    my $day      = $cgi->param("day_$sid");
                    my $o_time   = $cgi->param("orig_time_$sid");
                    my $time     = $cgi->param("time_$sid");
                    my $o_mailto = $cgi->param("orig_mailto_$sid");
                    my $mailto   = $cgi->param("mailto_$sid");

                    $o_day    = '' unless length($o_day);
                    $o_time   = '' unless length($o_time);
                    $o_mailto = '' unless length($o_mailto);
                    $day      = '' unless length($day);
                    $time     = '' unless length($time);
                    $mailto   = '' unless length($mailto);

                    my $mail_uid = $userid;

                    # get a userid for the mailto address
                    if ($can_mail_others and $mailto) {
                        trick_taint($mailto);
                        $mail_uid = DBname_to_id($mailto);
                    }

                    if ( ($o_day  ne $day) ||
                         ($o_time ne $time) ){

                        trick_taint($day) if length($day);
                        trick_taint($time) if length($time);

                        # the schedule table must be locked
                        $sth = $dbh->prepare("UPDATE whine_schedules " .
                                             "SET run_day=?, run_time=?, " .
                                             "mailto_userid=?, " .
                                             "run_next=NULL " .
                                             "WHERE id=?");
                        $sth->execute($day, $time, $mail_uid, $sid);
                    }
                }
            }

            # Check queries for changes
            $sth = $dbh->prepare("SELECT id " .
                                 "FROM whine_queries " .
                                 "WHERE eventid=?");
            $sth->execute($eventid);
            my @queries = ();
            for (@{$sth->fetchall_arrayref}) {
                push @queries, $_->[0];
            };

            for my $qid (@queries) {
                if ($cgi->param("remove_query_$qid")) {

                    $sth = $dbh->prepare("SELECT whine_queries.id " .
                                         "FROM whine_queries " .
                                         "LEFT JOIN whine_events " .
                                         "ON whine_events.id = " .
                                         "whine_queries.eventid " .
                                         "WHERE whine_events.owner_userid=? " .
                                         "AND whine_queries.id =?");
                    $sth->execute($userid, $qid);

                    for (@{$sth->fetchall_arrayref}) {
                        $sth = $dbh->prepare("DELETE FROM whine_queries " .
                                             "WHERE id=?");
                        $sth->execute($_->[0]);
                    }
                }
                else {
                    my $o_sort      = $cgi->param("orig_query_sort_$qid");
                    my $sort        = $cgi->param("query_sort_$qid");
                    my $o_queryname = $cgi->param("orig_query_name_$qid");
                    my $queryname   = $cgi->param("query_name_$qid");
                    my $o_title     = $cgi->param("orig_query_title_$qid");
                    my $title       = $cgi->param("query_title_$qid");
                    my $o_onemailperbug =
                            $cgi->param("orig_query_onemailperbug_$qid");
                    my $onemailperbug   =
                            $cgi->param("query_onemailperbug_$qid");

                    $o_sort          = '' unless length($o_sort);
                    $o_queryname     = '' unless length($o_queryname);
                    $o_title         = '' unless length($o_title);
                    $o_onemailperbug = '' unless length($o_onemailperbug);
                    $sort            = '' unless length($sort);
                    $queryname       = '' unless length($queryname);
                    $title           = '' unless length($title);
                    $onemailperbug   = '' unless length($onemailperbug);

                    if ($onemailperbug eq 'on') {
                        $onemailperbug = 1;
                    }
                    elsif ($onemailperbug eq 'off') {
                        $onemailperbug = 0;
                    }

                    if ( ($o_sort ne $sort) ||
                         ($o_queryname ne $queryname) ||
                         ($o_onemailperbug xor $onemailperbug) ||
                         ($o_title ne $title) ){

                        detaint_natural($sort)      if length $sort;
                        trick_taint($queryname)     if length $queryname;
                        trick_taint($title)         if length $title;
                        trick_taint($onemailperbug) if length $onemailperbug;

                        $sth = $dbh->prepare("UPDATE whine_queries " .
                                             "SET sortkey=?, " .
                                             "query_name=?, " .
                                             "title=?, " .
                                             "onemailperbug=? " .
                                             "WHERE id=?");
                        $sth->execute($sort, $queryname, $title,
                                      $onemailperbug, $qid);
                    }
                }
            }
        }
    }
}

$vars->{'mail_others'} = $can_mail_others;

# Return the appropriate HTTP response headers.
print $cgi->header();

# Get events again, to cover any updates that were made
$events = get_events($userid);

# Here is the data layout as sent to the template:
#
#   events
#       event_id #
#           schedule
#               day
#               time
#               mailto
#           queries
#               name
#               title
#               sort
#
# build the whine list by event id
for my $event_id (keys %{$events}) {

    $events->{$event_id}->{'schedule'} = [];
    $events->{$event_id}->{'queries'} = [];

    # schedules
    $sth = $dbh->prepare("SELECT run_day, run_time, profiles.login_name, id " .
                         "FROM whine_schedules " .
                         "LEFT JOIN profiles " .
                         "ON whine_schedules.mailto_userid = " .
                         "profiles.userid " .
                         "WHERE eventid=?");
    $sth->execute($event_id);
    for my $row (@{$sth->fetchall_arrayref}) {
        my $this_schedule = {
            'day'    => $row->[0],
            'time'   => $row->[1],
            'mailto' => $row->[2],
            'id'     => $row->[3],
        };
        push @{$events->{$event_id}->{'schedule'}}, $this_schedule;
    }

    # queries
    $sth = $dbh->prepare("SELECT query_name, title, sortkey, id, " .
                         "onemailperbug " .
                         "FROM whine_queries " .
                         "WHERE eventid=? " .
                         "ORDER BY sortkey");
    $sth->execute($event_id);
    for my $row (@{$sth->fetchall_arrayref}) {
        my $this_query = {
            'name'          => $row->[0],
            'title'         => $row->[1],
            'sort'          => $row->[2],
            'id'            => $row->[3],
            'onemailperbug' => $row->[4],
        };
        push @{$events->{$event_id}->{'queries'}}, $this_query;
    }
}

$vars->{'events'} = $events;

# get the available queries
$sth = $dbh->prepare("SELECT name FROM namedqueries WHERE userid=?");
$sth->execute($userid);

$vars->{'available_queries'} = [];
while (my $query = $sth->fetch) {
    push @{$vars->{'available_queries'}}, $query->[0];
}

$template->process("whine/schedule.html.tmpl", $vars)
  || ThrowTemplateError($template->error());

# get_events takes a userid and returns a hash, keyed by event ID, containing
# the subject and body of each event that user owns
sub get_events {
    my $userid = shift;
    my $events = {};

    my $sth = $dbh->prepare("SELECT DISTINCT id, subject, body " .
                            "FROM whine_events " .
                            "WHERE owner_userid=?");
    $sth->execute($userid);
    for (@{$sth->fetchall_arrayref}) {
        $events->{$_->[0]} = {
            'subject' => $_->[1],
            'body'    => $_->[2],
        }
    }
    return $events;
}

