#!/usr/bonsaitools/bin/perl -wT
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
use diagnostics;
use strict;

use lib qw(.);

use vars qw( $template $vars );

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.
sub sillyness {
    my $zz;
    $zz = $::db_name;
    $zz = @::components;
    $zz = @::default_column_list;
    $zz = $::defaultqueryname;
    $zz = @::dontchange;
    $zz = @::legal_keywords;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_product;
    $zz = @::legal_severity;
    $zz = @::settable_resolution;
    $zz = @::target_milestone;
    $zz = $::unconfirmedstate;
    $zz = $::userid;
    $zz = @::versions;
};

ConnectToDatabase();

################################################################################
# Data and Security Validation
################################################################################

# Determine the format in which the user would like to receive the output.
# Uses the default format if the user did not specify an output format;
# otherwise validates the user's choice against the list of available formats.
my $format = ValidateOutputFormat($::FORM{'format'}, "list");

# Whether or not the user wants to change multiple bugs.
my $dotweak = $::FORM{'tweak'} ? 1 : 0;

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
  exists $ENV{'HTTP_USER_AGENT'} 
    && $ENV{'HTTP_USER_AGENT'} =~ /Mozilla.[3-9]/ 
      && $ENV{'HTTP_USER_AGENT'} !~ /[Cc]ompatible/
        && $format->{'extension'} eq "html"
          && !defined($::FORM{'serverpush'})
            || $::FORM{'serverpush'};

my $order = $::FORM{'order'} || "";
my $order_from_cookie = 0;  # True if $order set using $::COOKIE{'LASTORDER'}

# If the user is retrieving the last bug list they looked at, hack the buffer
# storing the query string so that it looks like a query retrieving those bugs.
if ($::FORM{'regetlastlist'}) {
    if (!$::COOKIE{'BUGLIST'}) {
        DisplayError(qq|Sorry, I seem to have lost the cookie that recorded
                        the results of your last query.  You will have to start
                        over at the <a href="query.cgi">query page</a>.|);
        exit;
    }
    $::FORM{'bug_id'} = join(",", split(/:/, $::COOKIE{'BUGLIST'}));
    $order = "reuse last sort" unless $order;
    $::buffer = "bug_id=$::FORM{'bug_id'}&order=" . url_quote($order);
}

if ($::buffer =~ /&cmd-/) {
    my $url = "query.cgi?$::buffer#chart";
    print "Refresh: 0; URL=$url\n";
    print "Content-Type: text/html\n\n";
    # Generate and return the UI (HTML page) from the appropriate template.
    $vars->{'title'} = "Adding field to query page...";
    $vars->{'url'} = $url;
    $vars->{'link'} = "Click here if the page does not redisplay automatically.";
    $template->process("global/message.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
    exit;
}

# Generate a reasonable filename for the user agent to suggest to the user
# when the user saves the bug list.  Uses the name of the remembered query
# if available.  We have to do this now, even though we return HTTP headers 
# at the end, because the fact that there is a remembered query gets 
# forgotten in the process of retrieving it.
my @time = localtime(time());
my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
my $filename = "bugs-$date.$format->{extension}";
$::FORM{'cmdtype'} ||= "";
if ($::FORM{'cmdtype'} eq 'runnamed') {
    $filename = "$::FORM{'namedcmd'}-$date.$format->{extension}";
    # Remove white-space from the filename so the user cannot tamper
    # with the HTTP headers.
    $filename =~ s/\s//;
}

if ($dotweak) {
    confirm_login();
    if (!UserInGroup("editbugs")) {
        DisplayError("Sorry, you do not have sufficient privileges to edit
                      multiple bugs.");
        exit;
    }
    GetVersionTable();
}
else {
    quietly_check_login();
}


################################################################################
# Utilities
################################################################################

sub SqlifyDate {
    my ($str) = @_;
    $str = "" if !defined $str;
    my $date = str2time($str);
    if (!defined($date)) {
        my $htmlstr = html_quote($str);
        DisplayError("The string <tt>$htmlstr</tt> is not a legal date.");
        exit;
    }
    return time2str("%Y-%m-%d %H:%M:%S", $date);
}

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

sub GetByWordList {
    my ($field, $strs) = (@_);
    my @list;

    foreach my $w (split(/[\s,]+/, $strs)) {
        my $word = $w;
        if ($word ne "") {
            $word =~ tr/A-Z/a-z/;
            $word = SqlQuote(quotemeta($word));
            $word =~ s/^'//;
            $word =~ s/'$//;
            $word = '(^|[^a-z0-9])' . $word . '($|[^a-z0-9])';
            push(@list, "lower($field) regexp '$word'");
        }
    }

    return \@list;
}

#
# support for "any/all/nowordssubstr" comparison type ("words as substrings")
#
sub GetByWordListSubstr {
    my ($field, $strs) = (@_);
    my @list;

    foreach my $word (split(/[\s,]+/, $strs)) {
        if ($word ne "") {
            push(@list, "INSTR(LOWER($field), " . lc(SqlQuote($word)) . ")");
        }
    }

    return \@list;
}

sub LookupNamedQuery {
    my ($name) = @_;
    confirm_login();
    my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
    my $qname = SqlQuote($name);
    SendSQL("SELECT query FROM namedqueries WHERE userid = $userid AND name = $qname");
    my $result = FetchOneColumn();
    if (!$result) {
        my $qname = html_quote($name);
        DisplayError("The query named <em>$qname</em> seems to no longer exist.");
        exit;
    }
    return $result;
}

sub GetQuip {
    return if !Param('usequip');

    my $quip;

    # This is stupid.  We really really need to move the quip list into the DB!
    if (open(COMMENTS, "<data/comments")) {
        my @cdata;
        push(@cdata, $_) while <COMMENTS>;
        close COMMENTS;
        $quip = $cdata[int(rand($#cdata + 1))];
    }
    $quip ||= "Bugzilla would like to put a random quip here, but nobody has entered any.";

    return $quip;
}

sub GetGroupsByGroupSet {
    my ($groupset) = @_;

    return if !$groupset;

    SendSQL("
        SELECT  bit, name, description, isactive
          FROM  groups
         WHERE  (bit & $groupset) != 0
           AND  isbuggroup != 0
      ORDER BY  description ");

    my @groups;

    while (MoreSQLData()) {
        my $group = {};
        ($group->{'bit'}, $group->{'name'},
         $group->{'description'}, $group->{'isactive'}) = FetchSQLData();
        push(@groups, $group);
    }

    return \@groups;
}



################################################################################
# Query Generation
################################################################################

sub GenerateSQL {
    my $debug = 0;
    my ($fieldsref, $urlstr) = (@_);
    my @fields;
    my @supptables;
    my @wherepart;
    @fields = @$fieldsref if $fieldsref;
    my %F;
    my %M;
    ParseUrlString($urlstr, \%F, \%M);
    my @specialchart;
    my @andlist;

    # First, deal with all the old hard-coded non-chart-based poop.

    unshift(@supptables,
            ("profiles map_assigned_to",
             "profiles map_reporter",
             "LEFT JOIN profiles map_qa_contact ON bugs.qa_contact = map_qa_contact.userid"));
    unshift(@wherepart,
            ("bugs.assigned_to = map_assigned_to.userid",
             "bugs.reporter = map_reporter.userid"));

    my $minvotes;
    if (defined $F{'votes'}) {
        my $c = trim($F{'votes'});
        if ($c ne "") {
            if ($c !~ /^[0-9]*$/) {
                my $htmlc = html_quote($c);
                DisplayError("The <em>At least ___ votes</em> field must be
                              a simple number.  You entered <kbd>$htmlc</kbd>,
                              which doesn't cut it.");
                exit;
            }
            push(@specialchart, ["votes", "greaterthan", $c - 1]);
        }
    }

    if ($M{'bug_id'}) {
        my $type = "anyexact";
        if ($F{'bugidtype'} && $F{'bugidtype'} eq 'exclude') {
            $type = "nowords";
        }
        push(@specialchart, ["bug_id", $type, join(',', @{$M{'bug_id'}})]);
    }

# This is evil.  We should never allow a user to directly append SQL to
# any query without a huge amount of validation.  Even then, it would
# be a bad idea.  Beware that uncommenting this will allow someone to
# peak at virtually anything they want in the bugs database.
#    if (defined $F{'sql'}) {
#        die "Invalid sql: $F{'sql'}" if $F{'sql'} =~ /;/;
#        push(@wherepart, "( $F{'sql'} )");
#    }

    my @legal_fields = ("product", "version", "rep_platform", "op_sys",
                        "bug_status", "resolution", "priority", "bug_severity",
                        "assigned_to", "reporter", "component",
                        "target_milestone", "groupset");

    foreach my $field (keys %F) {
        if (lsearch(\@legal_fields, $field) != -1) {
            push(@specialchart, [$field, "anyexact",
                                 join(',', @{$M{$field}})]);
        }
    }

    if ($F{'keywords'}) {
        my $t = $F{'keywords_type'};
        if (!$t || $t eq "or") {
            $t = "anywords";
        }
        push(@specialchart, ["keywords", $t, $F{'keywords'}]);
    }

    foreach my $id ("1", "2") {
        if (!defined ($F{"email$id"})) {
            next;
        }
        my $email = trim($F{"email$id"});
        if ($email eq "") {
            next;
        }
        my $type = $F{"emailtype$id"};
        if ($type eq "exact") {
            $type = "anyexact";
            foreach my $name (split(',', $email)) {
                $name = trim($name);
                if ($name) {
                    DBNameToIdAndCheck($name);
                }
            }
        }

        my @clist;
        foreach my $field ("assigned_to", "reporter", "cc", "qa_contact") {
            if ($F{"email$field$id"}) {
                push(@clist, $field, $type, $email);
            }
        }
        if ($F{"emaillongdesc$id"}) {
            my $table = "longdescs_";
            push(@supptables, "longdescs $table");
            push(@wherepart, "$table.bug_id = bugs.bug_id");
            my $ptable = "longdescnames_";
            push(@supptables, "profiles $ptable");
            push(@wherepart, "$table.who = $ptable.userid");
            push(@clist, "$ptable.login_name", $type, $email);
        }
        if (@clist) {
            push(@specialchart, \@clist);
        } else {
            my $htmlemail = html_quote($email);
            DisplayError("You must specify one or more fields in which
                          to search for <tt>$htmlemail</tt>.");
            exit;
        }
    }


    if (defined $F{'changedin'}) {
        my $c = trim($F{'changedin'});
        if ($c ne "") {
            if ($c !~ /^[0-9]*$/) {
                my $htmlc = html_quote($c);
                DisplayError("The <em>changed in last ___ days</em> field
                              must be a simple number.  You entered
                              <kbd>$htmlc</kbd>, which doesn't cut it.");
                exit;
            }
            push(@specialchart, ["changedin",
                                 "lessthan", $c + 1]);
        }
    }

    my $ref = $M{'chfield'};

    if (defined $ref) {
        my $which = lsearch($ref, "[Bug creation]");
        if ($which >= 0) {
            splice(@$ref, $which, 1);
            push(@specialchart, ["creation_ts", "greaterthan",
                                 SqlifyDate($F{'chfieldfrom'})]);
            my $to = $F{'chfieldto'};
            if (defined $to) {
                $to = trim($to);
                if ($to ne "" && $to !~ /^now$/i) {
                    push(@specialchart, ["creation_ts", "lessthan",
                                         SqlifyDate($to)]);
                }
            }
        }
    }



    if (defined $ref && 0 < @$ref) {
        push(@supptables, "bugs_activity actcheck");

        my @list;
        foreach my $f (@$ref) {
            push(@list, "\nactcheck.fieldid = " . GetFieldID($f));
        }
        push(@wherepart, "actcheck.bug_id = bugs.bug_id");
        push(@wherepart, "(" . join(' OR ', @list) . ")");
        push(@wherepart, "actcheck.bug_when >= " .
             SqlQuote(SqlifyDate($F{'chfieldfrom'})));
        my $to = $F{'chfieldto'};
        if (defined $to) {
            $to = trim($to);
            if ($to ne "" && $to !~ /^now$/i) {
                push(@wherepart, "actcheck.bug_when <= " .
                     SqlQuote(SqlifyDate($to)));
            }
        }
        my $value = $F{'chfieldvalue'};
        if (defined $value) {
            $value = trim($value);
            if ($value ne "") {
                push(@wherepart, "actcheck.added = " .
                     SqlQuote($value))
            }
        }
    }


    foreach my $f ("short_desc", "long_desc", "bug_file_loc",
                   "status_whiteboard") {
        if (defined $F{$f}) {
            my $s = trim($F{$f});
            if ($s ne "") {
                my $n = $f;
                my $q = SqlQuote($s);
                my $type = $F{$f . "_type"};
                push(@specialchart, [$f, $type, $s]);
            }
        }
    }

    my $chartid;
    # $statusid is used by the code that queries for attachment statuses.
    my $statusid = 0;
    my $f;
    my $ff;
    my $t;
    my $q;
    my $v;
    my $term;
    my %funcsbykey;
    my @funcdefs =
        (
         "^(assigned_to|reporter)," => sub {
             push(@supptables, "profiles map_$f");
             push(@wherepart, "bugs.$f = map_$f.userid");
             $f = "map_$f.login_name";
         },
         "^qa_contact," => sub {
             push(@supptables,
                  "LEFT JOIN profiles map_qa_contact ON bugs.qa_contact = map_qa_contact.userid");
             $f = "map_$f.login_name";
         },

         "^cc," => sub {
            push(@supptables, "LEFT JOIN cc cc_$chartid ON bugs.bug_id = cc_$chartid.bug_id");

            push(@supptables, "LEFT JOIN profiles map_cc_$chartid ON cc_$chartid.who = map_cc_$chartid.userid");
            $f = "map_cc_$chartid.login_name";
         },

         "^long_?desc,changedby" => sub {
             my $table = "longdescs_$chartid";
             push(@supptables, "longdescs $table");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             my $id = DBNameToIdAndCheck($v);
             $term = "$table.who = $id";
         },
         "^long_?desc,changedbefore" => sub {
             my $table = "longdescs_$chartid";
             push(@supptables, "longdescs $table");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             $term = "$table.bug_when < " . SqlQuote(SqlifyDate($v));
         },
         "^long_?desc,changedafter" => sub {
             my $table = "longdescs_$chartid";
             push(@supptables, "longdescs $table");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             $term = "$table.bug_when > " . SqlQuote(SqlifyDate($v));
         },
         "^long_?desc," => sub {
             my $table = "longdescs_$chartid";
             push(@supptables, "longdescs $table");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             $f = "$table.thetext";
         },
         "^attachments\..*," => sub {
             my $table = "attachments_$chartid";
             push(@supptables, "attachments $table");
             push(@wherepart, "bugs.bug_id = $table.bug_id");
             $f =~ m/^attachments\.(.*)$/;
             my $field = $1;
             if ($t eq "changedby") {
                 $v = DBNameToIdAndCheck($v);
                 $q = SqlQuote($v);
                 $field = "submitter_id";
                 $t = "equals";
             } elsif ($t eq "changedbefore") {
                 $v = SqlifyDate($v);
                 $q = SqlQuote($v);
                 $field = "creation_ts";
                 $t = "lessthan";
             } elsif ($t eq "changedafter") {
                 $v = SqlifyDate($v);
                 $q = SqlQuote($v);
                 $field = "creation_ts";
                 $t = "greaterthan";
             }
             if ($field eq "ispatch" && $v ne "0" && $v ne "1") {
                 DisplayError("The only legal values for the <em>Attachment is
                               patch</em> field are 0 and 1.");
                 exit;
             }
             if ($field eq "isobsolete" && $v ne "0" && $v ne "1") {
                 DisplayError("The only legal values for the <em>Attachment is
                               obsolete</em> field are 0 and 1.");
                 exit;
             }
             $f = "$table.$field";
         },
         "^attachstatusdefs.name," => sub {
             # The below has Fun with the names for attachment statuses. This
             # isn't needed for changed* queries, so exclude those - the
             # generic stuff will cope
             return if ($t =~ m/^changed/);

             # Searching for "status != 'bar'" wants us to look for an
             # attachment without the 'bar' status, not for an attachment with
             # a status not equal to 'bar' (Which would pick up an attachment
             # with more than one status). We do this by LEFT JOINS, after
             # grabbing the matching attachment status ids.
             # Note that this still won't find bugs with no attachments, since
             # that isn't really what people would expect.

             # First, get the attachment status ids, using the other funcs
             # to match the WHERE term.
             # Note that we need to reverse the negated bits for this to work
             # This somewhat abuses the definitions of the various terms -
             # eg, does 'contains all' mean that the status has to contain all
             # those words, or that all those words must be exact matches to
             # statuses, which must all be on a single attachment, or should
             # the match on the status descriptions be a contains match, too?

             my $inverted = 0;
             if ($t =~ m/not(.*)/) {
                 $t = $1;
                 $inverted = 1;
             }

             $ref = $funcsbykey{",$t"};
             &$ref;
             SendSQL("SELECT id FROM attachstatusdefs WHERE $term");

             my @as_ids;
             while (MoreSQLData()) {
                 push @as_ids, FetchOneColumn();
             }

             # When searching for multiple statuses within a single boolean chart,
             # we want to match each status record separately.  In other words,
             # "status = 'foo' AND status = 'bar'" should match attachments with
             # one status record equal to "foo" and another one equal to "bar",
             # not attachments where the same status record equals both "foo" and
             # "bar" (which is nonsensical).  In order to do this we must add an
             # additional counter to the end of the "attachstatuses" table
             # reference.
             ++$statusid;

             my $attachtable = "attachments_$chartid";
             my $statustable = "attachstatuses_${chartid}_$statusid";

             push(@supptables, "attachments $attachtable");
             my $join = "LEFT JOIN attachstatuses $statustable ON ".
               "($attachtable.attach_id = $statustable.attach_id AND " .
                "$statustable.statusid IN (" . join(",", @as_ids) . "))";
             push(@supptables, $join);
             push(@wherepart, "bugs.bug_id = $attachtable.bug_id");
             if ($inverted) {
                 $term = "$statustable.statusid IS NULL";
             } else {
                 $term = "$statustable.statusid IS NOT NULL";
             }
         },
         "^changedin," => sub {
             $f = "(to_days(now()) - to_days(bugs.delta_ts))";
         },

         "^keywords," => sub {
             GetVersionTable();
             my @list;
             my $table = "keywords_$chartid";
             foreach my $value (split(/[\s,]+/, $v)) {
                 if ($value eq '') {
                     next;
                 }
                 my $id = GetKeywordIdFromName($value);
                 if ($id) {
                     push(@list, "$table.keywordid = $id");
                 }
                 else {
                     my $htmlv = html_quote($v);
                     DisplayError(qq|There is no keyword named <code>$htmlv</code>.
                                     To search for keywords, consult the
                                    <a href="describekeywords.cgi">list of legal keywords</a>.|);
                     exit;
                 }
             }
             my $haveawordterm;
             if (@list) {
                 $haveawordterm = "(" . join(' OR ', @list) . ")";
                 if ($t eq "anywords") {
                     $term = $haveawordterm;
                 } elsif ($t eq "allwords") {
                     $ref = $funcsbykey{",$t"};
                     &$ref;
                     if ($term && $haveawordterm) {
                         $term = "(($term) AND $haveawordterm)";
                     }
                 }
             }
             if ($term) {
                 push(@supptables, "keywords $table");
                 push(@wherepart, "$table.bug_id = bugs.bug_id");
             }
         },

         "^dependson," => sub {
                my $table = "dependson_" . $chartid;
                push(@supptables, "dependencies $table");
                $ff = "$table.$f";
                $ref = $funcsbykey{",$t"};
                &$ref;
                push(@wherepart, "$table.blocked = bugs.bug_id");
         },

         "^blocked," => sub {
                my $table = "blocked_" . $chartid;
                push(@supptables, "dependencies $table");
                $ff = "$table.$f";
                $ref = $funcsbykey{",$t"};
                &$ref;
                push(@wherepart, "$table.dependson = bugs.bug_id");
         },


         ",equals" => sub {
             $term = "$ff = $q";
         },
         ",notequals" => sub {
             $term = "$ff != $q";
         },
         ",casesubstring" => sub {
             $term = "INSTR($ff, $q)";
         },
         ",(substring|substr)" => sub {
             $term = "INSTR(LOWER($ff), " . lc($q) . ")";
         },
         ",notsubstring" => sub {
             $term = "INSTR(LOWER($ff), " . lc($q) . ") = 0";
         },
         ",regexp" => sub {
             $term = "LOWER($ff) REGEXP $q";
         },
         ",notregexp" => sub {
             $term = "LOWER($ff) NOT REGEXP $q";
         },
         ",lessthan" => sub {
             $term = "$ff < $q";
         },
         ",greaterthan" => sub {
             $term = "$ff > $q";
         },
         ",anyexact" => sub {
             my @list;
             foreach my $w (split(/,/, $v)) {
                 if ($w eq "---" && $f !~ /milestone/) {
                     $w = "";
                 }
                 push(@list, "$ff = " . SqlQuote($w));
             }
             $term = join(" OR ", @list);
         },
         ",anywordssubstr" => sub {
             $term = join(" OR ", @{GetByWordListSubstr($ff, $v)});
         },
         ",allwordssubstr" => sub {
             $term = join(" AND ", @{GetByWordListSubstr($ff, $v)});
         },
         ",nowordssubstr" => sub {
             my @list = @{GetByWordListSubstr($ff, $v)};
             if (@list) {
                 $term = "NOT (" . join(" OR ", @list) . ")";
             }
         },
         ",anywords" => sub {
             $term = join(" OR ", @{GetByWordList($ff, $v)});
         },
         ",allwords" => sub {
             $term = join(" AND ", @{GetByWordList($ff, $v)});
         },
         ",nowords" => sub {
             my @list = @{GetByWordList($ff, $v)};
             if (@list) {
                 $term = "NOT (" . join(" OR ", @list) . ")";
             }
         },
         ",changedbefore" => sub {
             my $table = "act_$chartid";
             my $ftable = "fielddefs_$chartid";
             push(@supptables, "bugs_activity $table");
             push(@supptables, "fielddefs $ftable");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             push(@wherepart, "$table.fieldid = $ftable.fieldid");
             $term = "($ftable.name = '$f' AND $table.bug_when < $q)";
         },
         ",changedafter" => sub {
             my $table = "act_$chartid";
             my $ftable = "fielddefs_$chartid";
             push(@supptables, "bugs_activity $table");
             push(@supptables, "fielddefs $ftable");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             push(@wherepart, "$table.fieldid = $ftable.fieldid");
             $term = "($ftable.name = '$f' AND $table.bug_when > $q)";
         },
         ",changedfrom" => sub {
             my $table = "act_$chartid";
             my $ftable = "fielddefs_$chartid";
             push(@supptables, "bugs_activity $table");
             push(@supptables, "fielddefs $ftable");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             push(@wherepart, "$table.fieldid = $ftable.fieldid");
             $term = "($ftable.name = '$f' AND $table.removed = $q)";
         },
         ",changedto" => sub {
             my $table = "act_$chartid";
             my $ftable = "fielddefs_$chartid";
             push(@supptables, "bugs_activity $table");
             push(@supptables, "fielddefs $ftable");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             push(@wherepart, "$table.fieldid = $ftable.fieldid");
             $term = "($ftable.name = '$f' AND $table.added = $q)";
         },
         ",changedby" => sub {
             my $table = "act_$chartid";
             my $ftable = "fielddefs_$chartid";
             push(@supptables, "bugs_activity $table");
             push(@supptables, "fielddefs $ftable");
             push(@wherepart, "$table.bug_id = bugs.bug_id");
             push(@wherepart, "$table.fieldid = $ftable.fieldid");
             my $id = DBNameToIdAndCheck($v);
             $term = "($ftable.name = '$f' AND $table.who = $id)";
         },
         );
    my @funcnames;
    while (@funcdefs) {
        my $key = shift(@funcdefs);
        my $value = shift(@funcdefs);
        if ($key =~ /^[^,]*$/) {
            die "All defs in %funcs must have a comma in their name: $key";
        }
        if (exists $funcsbykey{$key}) {
            die "Duplicate key in %funcs: $key";
        }
        $funcsbykey{$key} = $value;
        push(@funcnames, $key);
    }

    # first we delete any sign of "Chart #-1" from the HTML form hash
    # since we want to guarantee the user didn't hide something here
    my @badcharts = grep /^(field|type|value)-1-/, (keys %F);
    foreach my $field (@badcharts) {
        delete $F{$field};
    }

    # now we take our special chart and stuff it into the form hash
    my $chart = -1;
    my $row = 0;
    foreach my $ref (@specialchart) {
        my $col = 0;
        while (@$ref) {
            $F{"field$chart-$row-$col"} = shift(@$ref);
            $F{"type$chart-$row-$col"} = shift(@$ref);
            $F{"value$chart-$row-$col"} = shift(@$ref);
            if ($debug) {
                print qq{<P>$F{"field$chart-$row-$col"} | $F{"type$chart-$row-$col"} | $F{"value$chart-$row-$col"}*\n};
            }
            $col++;

        }
        $row++;
    }


# A boolean chart is a way of representing the terms in a logical
# expression.  Bugzilla builds SQL queries depending on how you enter
# terms into the boolean chart. Boolean charts are represented in
# urls as tree-tuples of (chart id, row, column). The query form
# (query.cgi) may contain an arbitrary number of boolean charts where
# each chart represents a clause in a SQL query.
#
# The query form starts out with one boolean chart containing one
# row and one column.  Extra rows can be created by pressing the
# AND button at the bottom of the chart.  Extra columns are created
# by pressing the OR button at the right end of the chart. Extra
# charts are created by pressing "Add another boolean chart".
#
# Each chart consists of an artibrary number of rows and columns.
# The terms within a row are ORed together. The expressions represented
# by each row are ANDed together. The expressions represented by each
# chart are ANDed together.
#
#        ----------------------
#        | col2 | col2 | col3 |
# --------------|------|------|
# | row1 |  a1  |  a2  |      |
# |------|------|------|------|  => ((a1 OR a2) AND (b1 OR b2 OR b3) AND (c1))
# | row2 |  b1  |  b2  |  b3  |
# |------|------|------|------|
# | row3 |  c1  |      |      |
# -----------------------------
#
#        --------
#        | col2 |
# --------------|
# | row1 |  d1  | => (d1)
# ---------------
#
# Together, these two charts represent a SQL expression like this
# SELECT blah FROM blah WHERE ( (a1 OR a2)AND(b1 OR b2 OR b3)AND(c1)) AND (d1)
#
# The terms within a single row of a boolean chart are all constraints
# on a single piece of data.  If you're looking for a bug that has two
# different people cc'd on it, then you need to use two boolean charts.
# This will find bugs with one CC mathing 'foo@blah.org' and and another
# CC matching 'bar@blah.org'.
#
# --------------------------------------------------------------
# CC    | equal to
# foo@blah.org
# --------------------------------------------------------------
# CC    | equal to
# bar@blah.org
#
# If you try to do this query by pressing the AND button in the
# original boolean chart then what you'll get is an expression that
# looks for a single CC where the login name is both "foo@blah.org",
# and "bar@blah.org". This is impossible.
#
# --------------------------------------------------------------
# CC    | equal to
# foo@blah.org
# AND
# CC    | equal to
# bar@blah.org
# --------------------------------------------------------------

# $chartid is the number of the current chart whose SQL we're contructing
# $row is the current row of the current chart

# names for table aliases are constructed using $chartid and $row
#   SELECT blah  FROM $table "$table_$chartid_$row" WHERE ....

# $f  = field of table in bug db (e.g. bug_id, reporter, etc)
# $ff = qualified field name (field name prefixed by table)
#       e.g. bugs_activity.bug_id
# $t  = type of query. e.g. "equal to", "changed after", case sensitive substr"
# $v  = value - value the user typed in to the form
# $q  = sanitized version of user input (SqlQuote($v))
# @supptables = Tables and/or table aliases used in query
# %suppseen   = A hash used to store all the tables in supptables to weed
#               out duplicates.
# $suppstring = String which is pasted into query containing all table names

    # get a list of field names to verify the user-submitted chart fields against
    my %chartfields;
    SendSQL("SELECT name FROM fielddefs");
    while (MoreSQLData()) {
        my ($name) = FetchSQLData();
        $chartfields{$name} = 1;
    }

    $row = 0;
    for ($chart=-1 ;
         $chart < 0 || exists $F{"field$chart-0-0"} ;
         $chart++) {
        $chartid = $chart >= 0 ? $chart : "";
        for ($row = 0 ;
             exists $F{"field$chart-$row-0"} ;
             $row++) {
            my @orlist;
            for (my $col = 0 ;
                 exists $F{"field$chart-$row-$col"} ;
                 $col++) {
                $f = $F{"field$chart-$row-$col"} || "noop";
                $t = $F{"type$chart-$row-$col"} || "noop";
                $v = $F{"value$chart-$row-$col"};
                $v = "" if !defined $v;
                $v = trim($v);
                if ($f eq "noop" || $t eq "noop" || $v eq "") {
                    next;
                }
                # chart -1 is generated by other code above, not from the user-
                # submitted form, so we'll blindly accept any values in chart -1
                if ((!$chartfields{$f}) && ($chart != -1)) {
                    my $errstr = "Can't use " . html_quote($f) . " as a field name.  " .
                        "If you think you're getting this in error, please copy the " .
                        "entire URL out of the address bar at the top of your browser " .
                        "window and email it to <109679\@bugzilla.org>";
                    die "Internal error: $errstr" if $chart < 0;
                    return Error($errstr);
                }

                # This is either from the internal chart (in which case we
                # already know about it), or it was in %chartfields, so it is
                # a valid field name, which means that its ok.
                trick_taint($f);
                $q = SqlQuote($v);
                my $func;
                $term = undef;
                foreach my $key (@funcnames) {
                    if ("$f,$t" =~ m/$key/) {
                        my $ref = $funcsbykey{$key};
                        if ($debug) {
                            print "<P>$key ($f , $t ) => ";
                        }
                        $ff = $f;
                        if ($f !~ /\./) {
                            $ff = "bugs.$f";
                        }
                        &$ref;
                        if ($debug) {
                            print "$f , $t , $term";
                        }
                        if ($term) {
                            last;
                        }
                    }
                }
                if ($term) {
                    push(@orlist, $term);
                }
                else {
                    my $errstr =
                      qq|Cannot seem to handle <code>$F{"field$chart-$row-$col"}</code>
                         and <code>$F{"type$chart-$row-$col"}</code> together|;
                    $chart < 0 ? die "Internal error: $errstr"
                               : DisplayError($errstr) && exit;
                }
            }
            if (@orlist) {
                push(@andlist, "(" . join(" OR ", @orlist) . ")");
            }
        }
    }
    my %suppseen = ("bugs" => 1);
    my $suppstring = "bugs";
    foreach my $str (@supptables) {
        if (!$suppseen{$str}) {
            if ($str !~ /^(LEFT|INNER) JOIN/i) {
                $suppstring .= ",";
            }
            $suppstring .= " $str";
            $suppseen{$str} = 1;
        }
    }
    my $query =  ("SELECT DISTINCT " . join(', ', @fields) .
                  " FROM $suppstring" .
                  " WHERE " . join(' AND ', (@wherepart, @andlist)));

    $query = SelectVisible($query, $::userid, $::usergroupset);

    if ($debug) {
        print "<P><CODE>" . value_quote($query) . "</CODE><P>\n";
        exit;
    }
    return $query;
}



################################################################################
# Command Execution
################################################################################

# Figure out if the user wanted to do anything besides just running the query
# they defined on the query page, and take appropriate action.
CMD: for ($::FORM{'cmdtype'}) {
    /^runnamed$/ && do {
        $::buffer = LookupNamedQuery($::FORM{"namedcmd"});
        $vars->{'title'} = "Bug List: $::FORM{'namedcmd'}";
        ProcessFormFields($::buffer);
        last CMD;
    };

    /^editnamed$/ && do {
        my $url = "query.cgi?" . LookupNamedQuery($::FORM{"namedcmd"});
        print "Refresh: 0; URL=$url\n";
        print "Content-Type: text/html\n\n";
        # Generate and return the UI (HTML page) from the appropriate template.
        $vars->{'title'} = "Loading your query named $::FORM{'namedcmd'}";
        $vars->{'url'} = $url;
        $vars->{'link'} = "Click here if the page does not redisplay automatically.";
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    };

    /^forgetnamed$/ && do {
        confirm_login();
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
        my $qname = SqlQuote($::FORM{'namedcmd'});
        SendSQL("DELETE FROM namedqueries WHERE userid = $userid AND name = $qname");
        print "Content-Type: text/html\n\n";
        # Generate and return the UI (HTML page) from the appropriate template.
        $vars->{'title'} = "Query is gone";
        $vars->{'message'} = "OK, the <b>$::FORM{'namedcmd'}</b> query is gone.";
        $vars->{'url'} = "query.cgi";
        $vars->{'link'} = "Go back to the query page.";
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    };

    /^asdefault$/ && do {
        confirm_login();
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
        my $qname = SqlQuote($::defaultqueryname);
        my $qbuffer = SqlQuote($::buffer);
        SendSQL("REPLACE INTO namedqueries (userid, name, query)
                 VALUES ($userid, $qname, $qbuffer)");
        print "Content-Type: text/html\n\n";
        # Generate and return the UI (HTML page) from the appropriate template.
        $vars->{'title'} = "OK, default is set";
        $vars->{'message'} = "OK, you now have a new default query.  You may
                              also bookmark the result of any individual query.";
        $vars->{'url'} = "query.cgi";
        $vars->{'link'} = "Go back to the query page, using the new default.";
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    };

    /^asnamed$/ && do {
        confirm_login();
        my $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});

        my $name = trim($::FORM{'newqueryname'});
        $name
          || DisplayError("You must enter a name for your query.")
            && exit;
        $name =~ /[<>&]/
          && DisplayError("The name of your query cannot contain any
                           of the following characters: &lt;, &gt;, &amp;.")
            && exit;
        my $qname = SqlQuote($name);

        $::buffer =~ s/[\&\?]cmdtype=[a-z]+//;
        my $qbuffer = SqlQuote($::buffer);

        my $tofooter= $::FORM{'tofooter'} ? 1 : 0;

        SendSQL("SELECT query FROM namedqueries WHERE userid = $userid AND name = $qname");
        if (FetchOneColumn()) {
            SendSQL("UPDATE  namedqueries
                        SET  query = $qbuffer , linkinfooter = $tofooter
                      WHERE  userid = $userid AND name = $qname");
        }
        else {
            SendSQL("REPLACE INTO namedqueries (userid, name, query, linkinfooter)
                     VALUES ($userid, $qname, $qbuffer, $tofooter)");
        }
        
        my $new_in_footer = $tofooter;
        
        # Don't add it to the list if they are reusing an existing query name.
        foreach my $query (@{$vars->{'user'}{'queries'}}) {
            if ($query->{'name'} eq $name) {
                $new_in_footer = 0;
            }
        }        
        
        print "Content-Type: text/html\n\n";
        # Generate and return the UI (HTML page) from the appropriate template.        
        if ($new_in_footer) {
            push(@{$vars->{'user'}{'queries'}}, {name => $name});
        }
        
        $vars->{'title'} = "OK, query saved.";
        $vars->{'message'} = "OK, you have a new query named <code>$name</code>";
        $vars->{'url'} = "query.cgi";
        $vars->{'link'} = "Go back to the query page.";
        $template->process("global/message.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;
    };
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
#       and the redundant summaryfull column is removed when the client
#       requests "all" columns.

my $columns = {};
sub DefineColumn {
    my ($id, $name, $title) = @_;
    $columns->{$id} = { 'name' => $name , 'title' => $title };
}

# Column:     ID                    Name                           Title
DefineColumn("id"                , "bugs.bug_id"                , "ID"               );
DefineColumn("groupset"          , "bugs.groupset"              , "Groupset"         );
DefineColumn("opendate"          , "bugs.creation_ts"           , "Opened"           );
DefineColumn("changeddate"       , "bugs.delta_ts"              , "Changed"          );
DefineColumn("severity"          , "bugs.bug_severity"          , "Severity"         );
DefineColumn("priority"          , "bugs.priority"              , "Priority"         );
DefineColumn("platform"          , "bugs.rep_platform"          , "Platform"         );
DefineColumn("owner"             , "map_assigned_to.login_name" , "Owner"            );
DefineColumn("reporter"          , "map_reporter.login_name"    , "Reporter"         );
DefineColumn("qa_contact"        , "map_qa_contact.login_name"  , "QA Contact"       );
DefineColumn("status"            , "bugs.bug_status"            , "State"            );
DefineColumn("resolution"        , "bugs.resolution"            , "Result"           );
DefineColumn("summary"           , "bugs.short_desc"            , "Summary"          );
DefineColumn("summaryfull"       , "bugs.short_desc"            , "Summary"          );
DefineColumn("status_whiteboard" , "bugs.status_whiteboard"     , "Status Summary"   );
DefineColumn("component"         , "bugs.component"             , "Component"        );
DefineColumn("product"           , "bugs.product"               , "Product"          );
DefineColumn("version"           , "bugs.version"               , "Version"          );
DefineColumn("os"                , "bugs.op_sys"                , "OS"               );
DefineColumn("target_milestone"  , "bugs.target_milestone"      , "Target Milestone" );
DefineColumn("votes"             , "bugs.votes"                 , "Votes"            );
DefineColumn("keywords"          , "bugs.keywords"              , "Keywords"         );


################################################################################
# Display Column Determination
################################################################################

# Determine the columns that will be displayed in the bug list via the 
# columnlist CGI parameter, the user's preferences, or the default.
my @displaycolumns = ();
if (defined $::FORM{'columnlist'}) {
    if ($::FORM{'columnlist'} eq "all") {
        # If the value of the CGI parameter is "all", display all columns,
        # but remove the redundant "summaryfull" column.
        @displaycolumns = grep($_ ne 'summaryfull', keys(%$columns));
    }
    else {
        @displaycolumns = split(/[ ,]+/, $::FORM{'columnlist'});
    }
}
elsif (defined $::COOKIE{'COLUMNLIST'}) {
    # Use the columns listed in the user's preferences.
    @displaycolumns = split(/ /, $::COOKIE{'COLUMNLIST'});
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
@displaycolumns = grep($_ ne 'id', @displaycolumns);

# IMPORTANT! Never allow the groupset column to be displayed!
@displaycolumns = grep($_ ne 'groupset', @displaycolumns);

# Add the votes column to the list of columns to be displayed
# in the bug list if the user is searching for bugs with a certain
# number of votes and the votes column is not already on the list.

# Some versions of perl will taint 'votes' if this is done as a single
# statement, because $::FORM{'votes'} is tainted at this point
$::FORM{'votes'} ||= "";
if (trim($::FORM{'votes'}) && !grep($_ eq 'votes', @displaycolumns)) {
    push(@displaycolumns, 'votes');
}


################################################################################
# Select Column Determination
################################################################################

# Generate the list of columns that will be selected in the SQL query.

# The bug ID and groupset are always selected because bug IDs are always
# displayed and we need the groupset to determine whether or not the bug
# is visible to the user.
my @selectcolumns = ("id", "groupset");

# Display columns are selected because otherwise we could not display them.
push (@selectcolumns, @displaycolumns);

# If the user is editing multiple bugs, we also make sure to select the product
# and status because the values of those fields determine what options the user
# has for modifying the bugs.
if ($dotweak) {
    push(@selectcolumns, "product") if !grep($_ eq 'product', @selectcolumns);
    push(@selectcolumns, "status") if !grep($_ eq 'status', @selectcolumns);
}


################################################################################
# Query Generation
################################################################################

# Convert the list of columns being selected into a list of column names.
my @selectnames = map($columns->{$_}->{'name'}, @selectcolumns);

# Generate the basic SQL query that will be used to generate the bug list.
my $query = GenerateSQL(\@selectnames, $::buffer);


################################################################################
# Sort Order Determination
################################################################################

# Add to the query some instructions for sorting the bug list.
if ($::COOKIE{'LASTORDER'} && !$order || $order =~ /^reuse/i) {
    $order = url_decode($::COOKIE{'LASTORDER'});
    $order_from_cookie = 1;
}

if ($order) {
    my $db_order;  # Modified version of $order for use with SQL query

    # Convert the value of the "order" form field into a list of columns
    # by which to sort the results.
    ORDER: for ($order) {
        /\./ && do {
            # A custom list of columns.  Make sure each column is valid.
            foreach my $fragment (split(/[,\s]+/, $order)) {
                next if $fragment =~ /^asc|desc$/i;
                my @columnnames = map($columns->{lc($_)}->{'name'}, keys(%$columns));
                if (!grep($_ eq $fragment, @columnnames)) {
                    my $qfragment = html_quote($fragment);
                    my $error = "The custom sort order you specified in your "
                              . "form submission contains an invalid column "
                              . "name <em>$qfragment</em>.";
                    if ($order_from_cookie) {
                        my $cookiepath = Param("cookiepath");
                        print "Set-Cookie: LASTORDER= ; path=$cookiepath; expires=Sun, 30-Jun-80 00:00:00 GMT\n";
                        $error =~ s/form submission/cookie/;
                        $error .= "  The cookie has been cleared.";
                    }
                    DisplayError($error);
                    exit;
                }
            }
            # Now that we have checked that all columns in the order are valid,
            # detaint the order string.
            trick_taint($order);
            last ORDER;
        };
        /Number/ && do {
            $order = "bugs.bug_id";
            last ORDER;
        };
        /Import/ && do {
            $order = "bugs.priority, bugs.bug_severity";
            last ORDER;
        };
        /Assign/ && do {
            $order = "map_assigned_to.login_name, bugs.bug_status, priority, bugs.bug_id";
            last ORDER;
        };
        /Changed/ && do {
            $order = "bugs.delta_ts, bugs.bug_status, bugs.priority, map_assigned_to.login_name, bugs.bug_id";
            last ORDER;
        };
        # DEFAULT
        $order = "bugs.bug_status, bugs.priority, map_assigned_to.login_name, bugs.bug_id";
    }

    $db_order = $order;  # Copy $order into $db_order for use with SQL query

    # Extra special disgusting hack: if we are ordering by target_milestone,
    # change it to order by the sortkey of the target_milestone first.
    if ($db_order =~ /bugs.target_milestone/) {
        $db_order =~ s/bugs.target_milestone/ms_order.sortkey,ms_order.value/;
        $query =~ s/\sWHERE\s/ LEFT JOIN milestones ms_order ON ms_order.value = bugs.target_milestone AND ms_order.product = bugs.product WHERE /;
    }

    # If we are sorting by votes, sort in descending order.
    if ($db_order =~ /bugs.votes\s+(asc|desc){0}/i) {
        $db_order =~ s/bugs.votes/bugs.votes desc/i;
    }

    $query .= " ORDER BY $db_order ";
}


################################################################################
# Query Execution
################################################################################

# Time to use server push to display an interim message to the user until
# the query completes and we can display the bug list.
if ($serverpush) {
    # Generate HTTP headers.
    print "Content-Disposition: inline; filename=$filename\n";
    print "Content-Type: multipart/x-mixed-replace;boundary=thisrandomstring\n\n";
    print "--thisrandomstring\n";
    print "Content-Type: text/html\n\n";

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("list/server-push.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

# Connect to the shadow database if this installation is using one to improve
# query performance.
ReconnectToShadowDatabase();

# Tell MySQL to store temporary tables on the hard drive instead of memory
# to avoid "table out of space" errors on MySQL versions less than 3.23.2.
SendSQL("SET OPTION SQL_BIG_TABLES=1") if Param('expectbigqueries');

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

my @bugs; # the list of records

while (my @row = FetchSQLData()) {
    my $bug = {}; # a record

    # Slurp the row of data into the record.
    foreach my $column (@selectcolumns) {
        $bug->{$column} = shift @row;
    }

    # Process certain values further (i.e. date format conversion).
    if ($bug->{'changeddate'}) {
        $bug->{'changeddate'} =~ 
          s/^(\d{4})(\d{2})(\d{2})(\d{2})(\d{2})(\d{2})$/$1-$2-$3 $4:$5:$6/;
        $bug->{'changeddate'} = DiffDate($bug->{'changeddate'});
    }
    ($bug->{'opendate'} = DiffDate($bug->{'opendate'})) if $bug->{'opendate'};

    # Record the owner, product, and status in the big hashes of those things.
    $bugowners->{$bug->{'owner'}} = 1 if $bug->{'owner'};
    $bugproducts->{$bug->{'product'}} = 1 if $bug->{'product'};
    $bugstatuses->{$bug->{'status'}} = 1 if $bug->{'status'};

    # Add the record to the list.
    push(@bugs, $bug);
}

# Switch back from the shadow database to the regular database so PutFooter()
# can determine the current user even if the "logincookies" table is corrupted
# in the shadow database.
SendSQL("USE $::db_name");


################################################################################
# Template Variable Definition
################################################################################

# Define the variables and functions that will be passed to the UI template.

$vars->{'bugs'} = \@bugs;
$vars->{'buglist'} = join(',', map($_->{id}, @bugs));
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
$vars->{'urlquerypart'} =~ s/[&?](order|cmdtype)=[^&]*//g;
$vars->{'order'} = $order;

# The user's login account name (i.e. email address).
my $login = $::COOKIE{'Bugzilla_login'};

$vars->{'caneditbugs'} = UserInGroup('editbugs');
$vars->{'usebuggroups'} = UserInGroup('usebuggroups');

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

if ($::FORM{'debug'}) {
    $vars->{'debug'} = 1;
    $vars->{'query'} = $query;
}

# Whether or not to split the column titles across two rows to make
# the list more compact.
$vars->{'splitheader'} = $::COOKIE{'SPLITHEADER'} ? 1 : 0;

$vars->{'quip'} = GetQuip() if Param('usequip');
$vars->{'currenttime'} = time2str("%a %b %e %T %Z %Y", time());

# The following variables are used when the user is making changes to multiple bugs.
if ($dotweak) {
    $vars->{'dotweak'} = 1;
    $vars->{'use_keywords'} = 1 if @::legal_keywords;

    $vars->{'products'} = \@::legal_product;
    $vars->{'platforms'} = \@::legal_platform;
    $vars->{'priorities'} = \@::legal_priority;
    $vars->{'severities'} = \@::legal_severity;
    $vars->{'resolutions'} = \@::settable_resolution;

    # The value that represents "don't change the value of this field".
    $vars->{'dontchange'} = $::dontchange;

    $vars->{'unconfirmedstate'} = $::unconfirmedstate;

    $vars->{'bugstatuses'} = [ keys %$bugstatuses ];

    # The groups to which the user belongs.
    $vars->{'groups'} = GetGroupsByGroupSet($::usergroupset) if $::usergroupset ne '0';

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

# If we are doing server push, output a separator string.
print "\n--thisrandomstring\n" if $serverpush;
    
# Generate HTTP headers

# Suggest a name for the bug list if the user wants to save it as a file.
# If we are doing server push, then we did this already in the HTTP headers
# that started the server push, so we don't have to do it again here.
print "Content-Disposition: inline; filename=$filename\n" unless $serverpush;

if ($format->{'extension'} eq "html") {
    my $cookiepath = Param("cookiepath");
    print "Content-Type: text/html\n";

    if ($order) {
        my $qorder = url_quote($order);
        print "Set-Cookie: LASTORDER=$qorder ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    }
    my $bugids = join(":", map( $_->{'id'}, @bugs));
    # See also Bug 111999
    if (length($bugids) < 4000) {
        print "Set-Cookie: BUGLIST=$bugids ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
    }
    else {
        print "Set-Cookie: BUGLIST= ; path=$cookiepath; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
        $vars->{'toolong'} = 1;
    }
}
else {
    print "Content-Type: $format->{'contenttype'}\n";
}

print "\n"; # end HTTP headers


################################################################################
# Content Generation
################################################################################

# Generate and return the UI (HTML page) from the appropriate template.
$template->process("list/$format->{'template'}", $vars)
  || ThrowTemplateError($template->error());


################################################################################
# Script Conclusion
################################################################################

print "\n--thisrandomstring--\n" if $serverpush;
