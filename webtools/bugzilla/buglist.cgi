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
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Stephan Niemz  <st.n@gmx.net>
#                 Andreas Franke <afranke@mathweb.org>
#                 Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(.);

use vars qw($template $vars);

use Bugzilla;
use Bugzilla::Search;
use Bugzilla::Constants;

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

use vars qw($db_name
            @components
            @default_column_list
            $defaultqueryname
            @legal_keywords
            @legal_platform
            @legal_priority
            @legal_product
            @legal_severity
            @settable_resolution
            @target_milestone
            $unconfirmedstate
            $userid
            @versions);

my $cgi = Bugzilla->cgi;

if (length($::buffer) == 0) {
    print $cgi->header(-refresh=> '10; URL=query.cgi');
    ThrowUserError("buglist_parameters_required");
}

################################################################################
# Data and Security Validation
################################################################################

# Whether or not the user wants to change multiple bugs.
my $dotweak = $::FORM{'tweak'} ? 1 : 0;

# Log the user in
if ($dotweak) {
    Bugzilla->login(LOGIN_REQUIRED);
    UserInGroup("editbugs") || ThrowUserError("insufficient_privs_for_multi");
    GetVersionTable();
}
else {
    Bugzilla->login();
}

# Hack to support legacy applications that think the RDF ctype is at format=rdf.
if ($::FORM{'format'} && $::FORM{'format'} eq "rdf" && !$::FORM{'ctype'}) { 
    $::FORM{'ctype'} = "rdf";
    delete($::FORM{'format'});
}

# The js ctype presents a security risk; a malicious site could use it  
# to gather information about secure bugs. So, we only allow public bugs to be
# retrieved with this format.
#
# Note that if and when this call clears cookies or has other persistent 
# effects, we'll need to do this another way instead.
if ((exists $::FORM{'ctype'}) && ($::FORM{'ctype'} eq "js")) {
    Bugzilla->logout_request();
}

# Determine the format in which the user would like to receive the output.
# Uses the default format if the user did not specify an output format;
# otherwise validates the user's choice against the list of available formats.
my $format = GetFormat("list/list", $::FORM{'format'}, $::FORM{'ctype'});

# Use server push to display a "Please wait..." message for the user while
# executing their query if their browser supports it and they are viewing
# the bug list as HTML and they have not disabled it by adding &serverpush=0
# to the URL.
#
# Server push is a Netscape 3+ hack incompatible with MSIE, Lynx, and others. 
# Even Communicator 4.51 has bugs with it, especially during page reload.
# http://www.browsercaps.org used as source of compatible browsers.
#
my $serverpush =
  $format->{'extension'} eq "html"
    && exists $ENV{'HTTP_USER_AGENT'} 
      && $ENV{'HTTP_USER_AGENT'} =~ /Mozilla.[3-9]/ 
        && $ENV{'HTTP_USER_AGENT'} !~ /[Cc]ompatible/
          && $ENV{'HTTP_USER_AGENT'} !~ /WebKit/
            && !defined($::FORM{'serverpush'})
              || $::FORM{'serverpush'};

my $order = $::FORM{'order'} || "";
my $order_from_cookie = 0;  # True if $order set using $::COOKIE{'LASTORDER'}

# The params object to use for the actual query itself
my $params;

# If the user is retrieving the last bug list they looked at, hack the buffer
# storing the query string so that it looks like a query retrieving those bugs.
if ($::FORM{'regetlastlist'}) {
    $::COOKIE{'BUGLIST'} || ThrowUserError("missing_cookie");

    $order = "reuse last sort" unless $order;

    # set up the params for this new query
    $params = new Bugzilla::CGI({
                                 bug_id => [split(/:/, $::COOKIE{'BUGLIST'})],
                                 order => $order,
                                });
}

if ($::buffer =~ /&cmd-/) {
    my $url = "query.cgi?$::buffer#chart";
    print $cgi->redirect(-location => $url);
    # Generate and return the UI (HTML page) from the appropriate template.
    $vars->{'message'} = "buglist_adding_field";
    $vars->{'url'} = $url;
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# Figure out whether or not the user is doing a fulltext search.  If not,
# we'll remove the relevance column from the lists of columns to display
# and order by, since relevance only exists when doing a fulltext search.
my $fulltext = 0;
if ($::FORM{'content'}) { $fulltext = 1 }
my @charts = map(/^field(\d-\d-\d)$/ ? $1 : (), keys %::FORM);
foreach my $chart (@charts) {
    if ($::FORM{"field$chart"} eq 'content' && $::FORM{"value$chart"}) {
        $fulltext = 1;
        last;
    }
}

################################################################################
# Utilities
################################################################################

my @weekday= qw( Sun Mon Tue Wed Thu Fri Sat );
sub DiffDate {
    my ($datestr) = @_;
    my $date = str2time($datestr);
    my $age = time() - $date;
    my ($s,$m,$h,$d,$mo,$y,$wd)= localtime $date;
    if( $age < 18*60*60 ) {
        $date = sprintf "%02d:%02d:%02d", $h,$m,$s;
    } elsif( $age < 6*24*60*60 ) {
        $date = sprintf "%s %02d:%02d", $weekday[$wd],$h,$m;
    } else {
        $date = sprintf "%04d-%02d-%02d", 1900+$y,$mo+1,$d;
    }
    return $date;
}

sub iCalendarDateTime {
    my ($datestr) = @_;
    my $date = str2time($datestr);
    my ($s,$m,$h,$d,$mo,$y,$wd)= gmtime $date;
    $date = sprintf "%04d%02d%02dT%02d%02d%02dZ", 1900+$y,$mo+1,$d,$h,$m,$s;
    return $date;
}

sub LookupNamedQuery {
    my ($name) = @_;
    Bugzilla->login(LOGIN_REQUIRED);
    my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
    my $qname = SqlQuote($name);
    SendSQL("SELECT query FROM namedqueries WHERE userid = $userid AND name = $qname");
    my $result = FetchOneColumn();
    
    defined($result) || ThrowUserError("missing_query", {'queryname' => $name});
    $result
       || ThrowUserError("buglist_parameters_required", {'queryname' => $name});

    return $result;
}

sub LookupSeries {
    my ($series_id) = @_;
    detaint_natural($series_id) || ThrowCodeError("invalid_series_id");
    
    my $dbh = Bugzilla->dbh;
    my $result = $dbh->selectrow_array("SELECT query FROM series " .
                                       "WHERE series_id = $series_id");
    $result
           || ThrowCodeError("invalid_series_id", {'series_id' => $series_id});
    return $result;
}

sub GetQuip {

    my $quip;

    # COUNT is quick because it is cached for MySQL. We may want to revisit
    # this when we support other databases.

    SendSQL("SELECT COUNT(quip) FROM quips WHERE approved = 1");
    my $count = FetchOneColumn();
    my $random = int(rand($count));
    SendSQL("SELECT quip FROM quips WHERE approved = 1 LIMIT $random,1");

    if (MoreSQLData()) {
        ($quip) = FetchSQLData();
    }

    return $quip;
}

sub GetGroupsByUserId {
    my ($userid) = @_;

    return if !$userid;

    SendSQL("
        SELECT DISTINCT  groups.id, name, description, isactive
                   FROM  groups, user_group_map
                  WHERE  user_id = $userid AND isbless = 0
                    AND  user_group_map.group_id = groups.id
                    AND  isbuggroup = 1
               ORDER BY  description ");

    my @groups;

    while (MoreSQLData()) {
        my $group = {};
        ($group->{'id'}, $group->{'name'},
         $group->{'description'}, $group->{'isactive'}) = FetchSQLData();
        push(@groups, $group);
    }

    return \@groups;
}


################################################################################
# Command Execution
################################################################################

$::FORM{'cmdtype'} ||= "";
$::FORM{'remaction'} ||= "";

# Backwards-compatibility - the old interface had cmdtype="runnamed" to run
# a named command, and we can't break this because it's in bookmarks.
if ($::FORM{'cmdtype'} eq "runnamed") {  
    $::FORM{'cmdtype'} = "dorem"; 
    $::FORM{'remaction'} = "run";
}

# Now we're going to be running, so ensure that the params object is set up,
# using ||= so that we only do so if someone hasn't overridden this 
# earlier, for example by setting up a named query search.

# This will be modified, so make a copy.
$params ||= new Bugzilla::CGI($cgi);

# Generate a reasonable filename for the user agent to suggest to the user
# when the user saves the bug list.  Uses the name of the remembered query
# if available.  We have to do this now, even though we return HTTP headers 
# at the end, because the fact that there is a remembered query gets 
# forgotten in the process of retrieving it.
my @time = localtime(time());
my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
my $filename = "bugs-$date.$format->{extension}";
if ($::FORM{'cmdtype'} eq "dorem" && $::FORM{'remaction'} =~ /^run/) {
    $filename = "$::FORM{'namedcmd'}-$date.$format->{extension}";
    # Remove white-space from the filename so the user cannot tamper
    # with the HTTP headers.
    $filename =~ s/\s/_/g;
}

# Take appropriate action based on user's request.
if ($::FORM{'cmdtype'} eq "dorem") {  
    if ($::FORM{'remaction'} eq "run") {
        $::buffer = LookupNamedQuery($::FORM{"namedcmd"});
        $vars->{'searchname'} = $::FORM{'namedcmd'};
        $vars->{'searchtype'} = "saved";
        $params = new Bugzilla::CGI($::buffer);
        $order = $params->param('order') || $order;
    }
    elsif ($::FORM{'remaction'} eq "runseries") {
        $::buffer = LookupSeries($::FORM{"series_id"});
        $vars->{'searchname'} = $::FORM{'namedcmd'};
        $vars->{'searchtype'} = "series";
        $params = new Bugzilla::CGI($::buffer);
        $order = $params->param('order') || $order;
    }
    elsif ($::FORM{'remaction'} eq "forget") {
        Bugzilla->login(LOGIN_REQUIRED);
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
        my $qname = SqlQuote($::FORM{'namedcmd'});
        SendSQL("DELETE FROM namedqueries WHERE userid = $userid AND name = $qname");

        # Now reset the cached queries
        Bugzilla->user->flush_queries_cache();

        print $cgi->header();
        # Generate and return the UI (HTML page) from the appropriate template.
        $vars->{'message'} = "buglist_query_gone";
        $vars->{'namedcmd'} = $::FORM{'namedcmd'};
        $vars->{'url'} = "query.cgi";
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }
}
elsif (($::FORM{'cmdtype'} eq "doit") && $::FORM{'remtype'}) {
    if ($::FORM{'remtype'} eq "asdefault") {
        Bugzilla->login(LOGIN_REQUIRED);
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
        my $qname = SqlQuote($::defaultqueryname);
        my $qbuffer = SqlQuote($::buffer);

        SendSQL("LOCK TABLES namedqueries WRITE");

        SendSQL("SELECT userid FROM namedqueries WHERE userid = $userid " .
        "AND name = $qname");
        my $result = FetchOneColumn();
        if ($result) {
            SendSQL("UPDATE namedqueries SET query = $qbuffer " .
                    "WHERE userid = $userid AND name = $qname");
        } else {
            SendSQL("INSERT INTO namedqueries (userid, name, query, linkinfooter) VALUES " .
                    "($userid, $qname, $qbuffer, 0)");
        }

        SendSQL("UNLOCK TABLES");

        $vars->{'message'} = "buglist_new_default_query";
    }
    elsif ($::FORM{'remtype'} eq "asnamed") {
        Bugzilla->login(LOGIN_REQUIRED);
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});

        my $name = trim($::FORM{'newqueryname'});
        $name || ThrowUserError("query_name_missing");
        $name !~ /[<>&]/ || ThrowUserError("illegal_query_name");
        my $qname = SqlQuote($name);

        $::FORM{'newquery'} || ThrowUserError("buglist_parameters_required", 
                                              {'queryname' => $name});
        my $qbuffer = SqlQuote($::FORM{'newquery'});
        
        my $tofooter = 1;

        $vars->{'message'} = "buglist_new_named_query";

        # We want to display the correct message. Check if it existed before
        # we insert, because ->queries may fetch from the db anyway
        if (grep { $_->{name} eq $name } @{Bugzilla->user->queries()}) {
            $vars->{'message'} = "buglist_updated_named_query";
        }

        SendSQL("LOCK TABLES namedqueries WRITE");

        SendSQL("SELECT query FROM namedqueries WHERE userid = $userid AND name = $qname");
        if (FetchOneColumn()) {
            SendSQL("UPDATE  namedqueries
                        SET  query = $qbuffer , linkinfooter = $tofooter
                      WHERE  userid = $userid AND name = $qname");
        }
        else {
            SendSQL("INSERT INTO namedqueries (userid, name, query, linkinfooter)
                     VALUES ($userid, $qname, $qbuffer, $tofooter)");
        }

        SendSQL("UNLOCK TABLES");

        # Make sure to invalidate any cached query data, so that the footer is
        # correctly displayed
        Bugzilla->user->flush_queries_cache();

        $vars->{'queryname'} = $name;
        
        print $cgi->header();
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    }
}


################################################################################
# Column Definition
################################################################################

# Define the columns that can be selected in a query and/or displayed in a bug
# list.  Column records include the following fields:
#
# 1. ID: a unique identifier by which the column is referred in code;
#
# 2. Name: The name of the column in the database (may also be an expression
#          that returns the value of the column);
#
# 3. Title: The title of the column as displayed to users.
# 
# Note: There are a few hacks in the code that deviate from these definitions.
#       In particular, when the list is sorted by the "votes" field the word 
#       "DESC" is added to the end of the field to sort in descending order, 
#       and the redundant short_desc column is removed when the client
#       requests "all" columns.

my $columns = {};
sub DefineColumn {
    my ($id, $name, $title) = @_;
    $columns->{$id} = { 'name' => $name , 'title' => $title };
}

# Column:     ID                    Name                           Title
DefineColumn("bug_id"            , "bugs.bug_id"                , "ID"               );
DefineColumn("alias"             , "bugs.alias"                 , "Alias"           );
DefineColumn("opendate"          , "bugs.creation_ts"           , "Opened"           );
DefineColumn("changeddate"       , "bugs.delta_ts"              , "Changed"          );
DefineColumn("bug_severity"      , "bugs.bug_severity"          , "Severity"         );
DefineColumn("priority"          , "bugs.priority"              , "Priority"         );
DefineColumn("rep_platform"      , "bugs.rep_platform"          , "Hardware"         );
DefineColumn("assigned_to"       , "map_assigned_to.login_name" , "Assignee"         );
DefineColumn("assigned_to_realname", "map_assigned_to.realname" , "Assignee"         );
DefineColumn("reporter"          , "map_reporter.login_name"    , "Reporter"         );
DefineColumn("reporter_realname" , "map_reporter.realname"      , "Reporter"         );
DefineColumn("qa_contact"        , "map_qa_contact.login_name"  , "QA Contact"       );
DefineColumn("qa_contact_realname", "map_qa_contact.realname"   , "QA Contact"       );
DefineColumn("bug_status"        , "bugs.bug_status"            , "Status"           );
DefineColumn("resolution"        , "bugs.resolution"            , "Result"           );
DefineColumn("short_short_desc"  , "bugs.short_desc"            , "Summary"          );
DefineColumn("short_desc"        , "bugs.short_desc"            , "Summary"          );
DefineColumn("status_whiteboard" , "bugs.status_whiteboard"     , "Status Summary"   );
DefineColumn("component"         , "map_components.name"        , "Component"        );
DefineColumn("product"           , "map_products.name"          , "Product"          );
DefineColumn("version"           , "bugs.version"               , "Version"          );
DefineColumn("op_sys"            , "bugs.op_sys"                , "OS"               );
DefineColumn("target_milestone"  , "bugs.target_milestone"      , "Target Milestone" );
DefineColumn("votes"             , "bugs.votes"                 , "Votes"            );
DefineColumn("keywords"          , "bugs.keywords"              , "Keywords"         );
DefineColumn("estimated_time"    , "bugs.estimated_time"        , "Estimated Hours"  );
DefineColumn("remaining_time"    , "bugs.remaining_time"        , "Remaining Hours"  );
DefineColumn("actual_time"       , "(SUM(ldtime.work_time)*COUNT(DISTINCT ldtime.bug_when)/COUNT(bugs.bug_id)) AS actual_time", "Actual Hours");
DefineColumn("percentage_complete","(100*((SUM(ldtime.work_time)*COUNT(DISTINCT ldtime.bug_when)/COUNT(bugs.bug_id))/((SUM(ldtime.work_time)*COUNT(DISTINCT ldtime.bug_when)/COUNT(bugs.bug_id))+bugs.remaining_time))) AS percentage_complete", "% Complete"); 
DefineColumn("relevance"         , "relevance"                  , "Relevance"        );


################################################################################
# Display Column Determination
################################################################################

# Determine the columns that will be displayed in the bug list via the 
# columnlist CGI parameter, the user's preferences, or the default.
my @displaycolumns = ();
if (defined $params->param('columnlist')) {
    if ($params->param('columnlist') eq "all") {
        # If the value of the CGI parameter is "all", display all columns,
        # but remove the redundant "short_desc" column.
        @displaycolumns = grep($_ ne 'short_desc', keys(%$columns));
    }
    else {
        @displaycolumns = split(/[ ,]+/, $params->param('columnlist'));
    }
}
elsif (defined $::COOKIE{'COLUMNLIST'}) {
    # 2002-10-31 Rename column names (see bug 176461)
    my $columnlist = $::COOKIE{'COLUMNLIST'};
    $columnlist =~ s/\bowner\b/assigned_to/;
    $columnlist =~ s/\bowner_realname\b/assigned_to_realname/;
    $columnlist =~ s/\bplatform\b/rep_platform/;
    $columnlist =~ s/\bseverity\b/bug_severity/;
    $columnlist =~ s/\bstatus\b/bug_status/;
    $columnlist =~ s/\bsummaryfull\b/short_desc/;
    $columnlist =~ s/\bsummary\b/short_short_desc/;

    # Use the columns listed in the user's preferences.
    @displaycolumns = split(/ /, $columnlist);
}
else {
    # Use the default list of columns.
    @displaycolumns = @::default_column_list;
}

# Weed out columns that don't actually exist to prevent the user 
# from hacking their column list cookie to grab data to which they 
# should not have access.  Detaint the data along the way.
@displaycolumns = grep($columns->{$_} && trick_taint($_), @displaycolumns);

# Remove the "ID" column from the list because bug IDs are always displayed
# and are hard-coded into the display templates.
@displaycolumns = grep($_ ne 'bug_id', @displaycolumns);

# Add the votes column to the list of columns to be displayed
# in the bug list if the user is searching for bugs with a certain
# number of votes and the votes column is not already on the list.

# Some versions of perl will taint 'votes' if this is done as a single
# statement, because the votes param is tainted at this point
my $votes = $params->param('votes');
$votes ||= "";
if (trim($votes) && !grep($_ eq 'votes', @displaycolumns)) {
    push(@displaycolumns, 'votes');
}

# Remove the timetracking columns if they are not a part of the group
# (happens if a user had access to time tracking and it was revoked/disabled)
if (!UserInGroup(Param("timetrackinggroup"))) {
   @displaycolumns = grep($_ ne 'estimated_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'remaining_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'actual_time', @displaycolumns);
   @displaycolumns = grep($_ ne 'percentage_complete', @displaycolumns);
}

# Remove the relevance column if the user is not doing a fulltext search.
if (grep('relevance', @displaycolumns) && !$fulltext) {
    @displaycolumns = grep($_ ne 'relevance', @displaycolumns);
}


################################################################################
# Select Column Determination
################################################################################

# Generate the list of columns that will be selected in the SQL query.

# The bug ID is always selected because bug IDs are always displayed.
# Severity, priority, resolution and status are required for buglist
# CSS classes.
my @selectcolumns = ("bug_id", "bug_severity", "priority", "bug_status",
                     "resolution");

# remaining and actual_time are required for precentage_complete calculation:
if (lsearch(\@displaycolumns, "percentage_complete") >= 0) {
    push (@selectcolumns, "remaining_time");
    push (@selectcolumns, "actual_time");
}

# Display columns are selected because otherwise we could not display them.
push (@selectcolumns, @displaycolumns);

# If the user is editing multiple bugs, we also make sure to select the product
# and status because the values of those fields determine what options the user
# has for modifying the bugs.
if ($dotweak) {
    push(@selectcolumns, "product") if !grep($_ eq 'product', @selectcolumns);
    push(@selectcolumns, "bug_status") if !grep($_ eq 'bug_status', @selectcolumns);
}

if ($format->{'extension'} eq 'ics') {
    push(@selectcolumns, "opendate") if !grep($_ eq 'opendate', @selectcolumns);
}

################################################################################
# Query Generation
################################################################################

# Convert the list of columns being selected into a list of column names.
my @selectnames = map($columns->{$_}->{'name'}, @selectcolumns);

# Remove columns with no names, such as percentage_complete
#  (or a removed *_time column due to permissions)
@selectnames = grep($_ ne '', @selectnames);

################################################################################
# Sort Order Determination
################################################################################

# Add to the query some instructions for sorting the bug list.
if ($::COOKIE{'LASTORDER'} && (!$order || $order =~ /^reuse/i)) {
    $order = $::COOKIE{'LASTORDER'};
    $order_from_cookie = 1;
}

my $db_order = "";  # Modified version of $order for use with SQL query
if ($order) {
    # Convert the value of the "order" form field into a list of columns
    # by which to sort the results.
    ORDER: for ($order) {
        /^Bug Number$/ && do {
            $order = "bugs.bug_id";
            last ORDER;
        };
        /^Importance$/ && do {
            $order = "bugs.priority, bugs.bug_severity";
            last ORDER;
        };
        /^Assignee$/ && do {
            $order = "map_assigned_to.login_name, bugs.bug_status, bugs.priority, bugs.bug_id";
            last ORDER;
        };
        /^Last Changed$/ && do {
            $order = "bugs.delta_ts, bugs.bug_status, bugs.priority, map_assigned_to.login_name, bugs.bug_id";
            last ORDER;
        };
        do {
            my @order;
            my @columnnames = map($columns->{lc($_)}->{'name'}, keys(%$columns));
            # A custom list of columns.  Make sure each column is valid.
            foreach my $fragment (split(/,/, $order)) {
                $fragment = trim($fragment);
                # Accept an order fragment matching a column name, with
                # asc|desc optionally following (to specify the direction)
                if (grep($fragment =~ /^\Q$_\E(\s+(asc|desc))?$/, @columnnames)) {
                    next if $fragment =~ /\brelevance\b/ && !$fulltext;
                    push(@order, $fragment);
                }
                else {
                    my $vars = { fragment => $fragment };
                    if ($order_from_cookie) {
                        $cgi->send_cookie(-name => 'LASTORDER',
                                          -expires => 'Tue, 15-Sep-1998 21:49:00 GMT');
                        ThrowCodeError("invalid_column_name_cookie", $vars);
                    }
                    else {
                        ThrowCodeError("invalid_column_name_form", $vars);
                    }
                }
            }
            $order = join(",", @order);
            # Now that we have checked that all columns in the order are valid,
            # detaint the order string.
            trick_taint($order);
        };
    }
}
else {
    # DEFAULT
    $order = "bugs.bug_status, bugs.priority, map_assigned_to.login_name, bugs.bug_id";
}

foreach my $fragment (split(/,/, $order)) {
    $fragment = trim($fragment);
    if (!grep($fragment =~ /^\Q$_\E(\s+(asc|desc))?$/, @selectnames)) {
        # Add order columns to selectnames
        # The fragment has already been validated
        $fragment =~ s/\s+(asc|desc)$//;
        $fragment =~ tr/a-zA-Z\.0-9\-_//cd;
        push @selectnames, $fragment;
    }
}

$db_order = $order;  # Copy $order into $db_order for use with SQL query

# If we are sorting by votes, sort in descending order if no explicit
# sort order was given
$db_order =~ s/bugs.votes\s*(,|$)/bugs.votes desc$1/i;
                             
# the 'actual_time' field is defined as an aggregate function, but 
# for order we just need the column name 'actual_time'
my $aggregate_search = quotemeta($columns->{'actual_time'}->{'name'});
$db_order =~ s/$aggregate_search/actual_time/g;

# the 'percentage_complete' field is defined as an aggregate too
$aggregate_search = quotemeta($columns->{'percentage_complete'}->{'name'});
$db_order =~ s/$aggregate_search/percentage_complete/g;

# Generate the basic SQL query that will be used to generate the bug list.
my $search = new Bugzilla::Search('fields' => \@selectnames, 
                                  'params' => $params);
my $query = $search->getSQL();

# Extra special disgusting hack: if we are ordering by target_milestone,
# change it to order by the sortkey of the target_milestone first.
if ($db_order =~ /bugs.target_milestone/) {
    $db_order =~ s/bugs.target_milestone/ms_order.sortkey,ms_order.value/;
    $query =~ s/\sWHERE\s/ LEFT JOIN milestones ms_order ON ms_order.value = bugs.target_milestone AND ms_order.product_id = bugs.product_id WHERE /;
}

$query .= " ORDER BY $db_order " if ($order);

if ($::FORM{'limit'} && detaint_natural($::FORM{'limit'})) {
    $query .= " LIMIT $::FORM{'limit'}";
}
elsif ($fulltext) {
    $query .= " LIMIT 200";
}


################################################################################
# Query Execution
################################################################################

if ($::FORM{'debug'}) {
    $vars->{'debug'} = 1;
    $vars->{'query'} = $query;
}

# Time to use server push to display an interim message to the user until
# the query completes and we can display the bug list.
if ($serverpush) {
    print $cgi->multipart_init(-content_disposition => "inline; filename=$filename");

    print $cgi->multipart_start();

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("list/server-push.html.tmpl", $vars)
      || ThrowTemplateError($template->error());

    print $cgi->multipart_end();
}

# Connect to the shadow database if this installation is using one to improve
# query performance.
Bugzilla->switch_to_shadow_db();

# Normally, we ignore SIGTERM and SIGPIPE (see globals.pl) but we need to
# respond to them here to prevent someone DOSing us by reloading a query
# a large number of times.
$::SIG{TERM} = 'DEFAULT';
$::SIG{PIPE} = 'DEFAULT';

# Execute the query.
SendSQL($query);


################################################################################
# Results Retrieval
################################################################################

# Retrieve the query results one row at a time and write the data into a list
# of Perl records.

my $bugowners = {};
my $bugproducts = {};
my $bugstatuses = {};
my @bugidlist;

my @bugs; # the list of records

while (my @row = FetchSQLData()) {
    my $bug = {}; # a record

    # Slurp the row of data into the record.
    # The second from last column in the record is the number of groups
    # to which the bug is restricted.
    foreach my $column (@selectcolumns) {
        $bug->{$column} = shift @row;
    }

    # Process certain values further (i.e. date format conversion).
    if ($bug->{'changeddate'}) {
        $bug->{'changeddate'} =~ 
            s/^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})$/$1-$2-$3 $4:$5:$6/;
        if ($format->{'extension'} eq 'ics') {
            $bug->{'changeddate'} = iCalendarDateTime($bug->{'changeddate'});
        }
        else {
            $bug->{'changeddate'} = DiffDate($bug->{'changeddate'});
        }
    }

    if ($bug->{'opendate'}) {
        if ($format->{'extension'} eq 'ics') {
            $bug->{'opendate'} = iCalendarDateTime($bug->{'opendate'});
        }
        else {
            $bug->{'opendate'} = DiffDate($bug->{'opendate'});
        }
    }

    # Record the owner, product, and status in the big hashes of those things.
    $bugowners->{$bug->{'assigned_to'}} = 1 if $bug->{'assigned_to'};
    $bugproducts->{$bug->{'product'}} = 1 if $bug->{'product'};
    $bugstatuses->{$bug->{'bug_status'}} = 1 if $bug->{'bug_status'};

    $bug->{isingroups} = 0;

    # Add the record to the list.
    push(@bugs, $bug);

    # Add id to list for checking for bug privacy later
    push(@bugidlist, $bug->{'bug_id'});
}

# Check for bug privacy and set $bug->{isingroups} = 1 if private 
# to 1 or more groups
my %privatebugs;
if (@bugidlist) {
    SendSQL("SELECT DISTINCT bugs.bug_id FROM bugs, bug_group_map " .
            "WHERE bugs.bug_id = bug_group_map.bug_id " .
            "AND bugs.bug_id IN (" . join(',',@bugidlist) . ")");
    while (MoreSQLData()) {
        my ($bug_id) = FetchSQLData();
        $privatebugs{$bug_id} = 1;
    }
    foreach my $bug (@bugs) {
        if ($privatebugs{$bug->{'bug_id'}}) {
            $bug->{isingroups} = 1;
        }
    }
}

################################################################################
# Template Variable Definition
################################################################################

# Define the variables and functions that will be passed to the UI template.

$vars->{'bugs'} = \@bugs;
$vars->{'buglist'} = join(',', @bugidlist);
$vars->{'columns'} = $columns;
$vars->{'displaycolumns'} = \@displaycolumns;

my @openstates = OpenStates();
$vars->{'openstates'} = \@openstates;
$vars->{'closedstates'} = ['CLOSED', 'VERIFIED', 'RESOLVED'];

# The list of query fields in URL query string format, used when creating
# URLs to the same query results page with different parameters (such as
# a different sort order or when taking some action on the set of query
# results).  To get this string, we start with the raw URL query string
# buffer that was created when we initially parsed the URL on script startup,
# then we remove all non-query fields from it, f.e. the sort order (order)
# and command type (cmdtype) fields.
$vars->{'urlquerypart'} = $::buffer;
$vars->{'urlquerypart'} =~ s/(order|cmdtype)=[^&]*&?//g;
$vars->{'order'} = $order;

# The user's login account name (i.e. email address).
my $login = $::COOKIE{'Bugzilla_login'};

$vars->{'caneditbugs'} = UserInGroup('editbugs');

# Whether or not this user is authorized to move bugs to another installation.
$vars->{'ismover'} = 1
  if Param('move-enabled')
    && defined($login)
      && Param('movers') =~ /^(\Q$login\E[,\s])|([,\s]\Q$login\E[,\s]+)/;

my @bugowners = keys %$bugowners;
if (scalar(@bugowners) > 1 && UserInGroup('editbugs')) {
    my $suffix = Param('emailsuffix');
    map(s/$/$suffix/, @bugowners) if $suffix;
    my $bugowners = join(",", @bugowners);
    $vars->{'bugowners'} = $bugowners;
}

# Whether or not to split the column titles across two rows to make
# the list more compact.
$vars->{'splitheader'} = $::COOKIE{'SPLITHEADER'} ? 1 : 0;

$vars->{'quip'} = GetQuip();
$vars->{'currenttime'} = time();
if ($format->{'extension'} eq 'ics') {
    $vars->{'currenttime'} = iCalendarDateTime(scalar gmtime $vars->{'currenttime'});
}

# The following variables are used when the user is making changes to multiple bugs.
if ($dotweak) {
    $vars->{'dotweak'} = 1;
    $vars->{'use_keywords'} = 1 if @::legal_keywords;

    my @enterable_products = GetEnterableProducts();
    $vars->{'products'} = \@enterable_products;
    $vars->{'platforms'} = \@::legal_platform;
    $vars->{'priorities'} = \@::legal_priority;
    $vars->{'severities'} = \@::legal_severity;
    $vars->{'resolutions'} = \@::settable_resolution;

    $vars->{'unconfirmedstate'} = $::unconfirmedstate;

    $vars->{'bugstatuses'} = [ keys %$bugstatuses ];

    # The groups to which the user belongs.
    $vars->{'groups'} = GetGroupsByUserId($::userid);

    # If all bugs being changed are in the same product, the user can change
    # their version and component, so generate a list of products, a list of
    # versions for the product (if there is only one product on the list of
    # products), and a list of components for the product.
    $vars->{'bugproducts'} = [ keys %$bugproducts ];
    if (scalar(@{$vars->{'bugproducts'}}) == 1) {
        my $product = $vars->{'bugproducts'}->[0];
        $vars->{'versions'} = $::versions{$product};
        $vars->{'components'} = $::components{$product};
        $vars->{'targetmilestones'} = $::target_milestone{$product} if Param('usetargetmilestone');
    }
}


################################################################################
# HTTP Header Generation
################################################################################

# Generate HTTP headers

my $contenttype;
my $disp = "inline";

if ($format->{'extension'} eq "html") {
    my $cookiepath = Param("cookiepath");

    if ($order) {
        $cgi->send_cookie(-name => 'LASTORDER',
                          -value => $order,
                          -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');
    }
    my $bugids = join(":", @bugidlist);
    # See also Bug 111999
    if (length($bugids) < 4000) {
        $cgi->send_cookie(-name => 'BUGLIST',
                          -value => $bugids,
                          -expires => 'Fri, 01-Jan-2038 00:00:00 GMT');
    }
    else {
        $cgi->send_cookie(-name => 'BUGLIST',
                          -expires => 'Tue, 15-Sep-1998 21:49:00 GMT');
        $vars->{'toolong'} = 1;
    }

    $contenttype = "text/html";
}
else {
    $contenttype = $format->{'ctype'};
}

if ($format->{'extension'} eq "csv") {
    # We set CSV files to be downloaded, as they are designed for importing
    # into other programs.
    $disp = "attachment";
}

if ($serverpush) {
    print $cgi->multipart_start(-type=>$contenttype);
} else {
    # Suggest a name for the bug list if the user wants to save it as a file.
    # If we are doing server push, then we did this already in the HTTP headers
    # that started the server push, so we don't have to do it again here.
    print $cgi->header(-type => $contenttype,
                       -content_disposition => "$disp; filename=$filename");
}


################################################################################
# Content Generation
################################################################################

# Generate and return the UI (HTML page) from the appropriate template.
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());


################################################################################
# Script Conclusion
################################################################################

print $cgi->multipart_final() if $serverpush;
