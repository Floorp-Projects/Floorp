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
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>

package Bugzilla::Install::DB;

# NOTE: This package may "use" any modules that it likes,
# localconfig is available, and params are up to date. 

use strict;

use Bugzilla::Bug qw(is_open_state);
use Bugzilla::Constants;
use Bugzilla::Field;
use Bugzilla::Util;
use Bugzilla::Series;

use Date::Parse;
use Date::Format;
use IO::File;

use base qw(Exporter);
our @EXPORT = qw(
    indicate_progress
);

sub indicate_progress {
    my ($params) = @_;
    my $current = $params->{current};
    my $total   = $params->{total};
    my $every   = $params->{every} || 1;

    print "." if !($current % $every);
    if ($current % ($every * 50) == 0) {
        print "$current/$total (" . int($current * 100 / $total) . "%)\n";
    }
}


# Small changes can be put directly into this function.
# However, larger changes (more than three or four lines) should
# go into their own private subroutine, and you should call that
# subroutine from this function. That keeps this function readable.
#
# This function runs in historical order--from upgrades that older
# installations need, to upgrades that newer installations need.
# The order of items inside this function should only be changed if 
# absolutely necessary.
#
# The subroutines should have long, descriptive names, so that you
# can easily see what is being done, just by reading this function.
#
# This function is mostly self-documenting. If you're curious about
# what each of the added/removed columns does, you should see the schema 
# docs at:
# http://www.ravenbrook.com/project/p4dti/tool/cgi/bugzilla-schema/
#
# When you add a change, you should only add a comment if you want
# to describe why the change was made. You don't need to describe
# the purpose of a column.
#
sub update_table_definitions {
    my $dbh = Bugzilla->dbh;
    _update_pre_checksetup_bugzillas();

    $dbh->bz_add_column('attachments', 'submitter_id',
                        {TYPE => 'INT3', NOTNULL => 1}, 0); 

    $dbh->bz_rename_column('bugs_activity', 'when', 'bug_when');

    _add_bug_vote_cache();
    _update_product_name_definition();
    _add_bug_keyword_cache();

    $dbh->bz_add_column('profiles', 'disabledtext',
                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

    _populate_longdescs();
    _update_bugs_activity_field_to_fieldid();

    if (!$dbh->bz_column_info('bugs', 'lastdiffed')) {
        $dbh->bz_add_column('bugs', 'lastdiffed', {TYPE =>'DATETIME'});
        $dbh->do('UPDATE bugs SET lastdiffed = NOW()');
    }

    _add_unique_login_name_index_to_profiles();

    $dbh->bz_add_column('profiles', 'mybugslink', 
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    _update_component_user_fields_to_ids();

    $dbh->bz_add_column('bugs', 'everconfirmed',
                        {TYPE => 'BOOLEAN', NOTNULL => 1}, 1);

    $dbh->bz_add_column('products', 'maxvotesperbug',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '10000'});
    $dbh->bz_add_column('products', 'votestoconfirm',
                        {TYPE => 'INT2', NOTNULL => 1}, 0);

    _populate_milestones_table();

    # 2000-03-22 Changed the default value for target_milestone to be "---"
    # (which is still not quite correct, but much better than what it was
    # doing), and made the size of the value field in the milestones table match
    # the size of the target_milestone field in the bugs table.
    $dbh->bz_alter_column('bugs', 'target_milestone',
        {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
    $dbh->bz_alter_column('milestones', 'value',
                          {TYPE => 'varchar(20)', NOTNULL => 1});

    _add_products_defaultmilestone();

    # 2000-03-24 Added unique indexes into the cc and keyword tables.  This
    # prevents certain database inconsistencies, and, moreover, is required for
    # new generalized list code to work.
    if (!$dbh->bz_index_info('cc', 'cc_bug_id_idx')->{TYPE}) {
        $dbh->bz_drop_index('cc', 'cc_bug_id_idx');
        $dbh->bz_add_index('cc', 'cc_bug_id_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(bug_id who)]});
    }
    if (!$dbh->bz_index_info('keywords', 'keywords_bug_id_idx')->{TYPE}) {
        $dbh->bz_drop_index('keywords', 'keywords_bug_id_idx');
        $dbh->bz_add_index('keywords', 'keywords_bug_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(bug_id keywordid)]});
    }

    _copy_from_comments_to_longdescs();
    _populate_duplicates_table();

    if (!$dbh->bz_column_info('email_setting', 'user_id')) {
        $dbh->bz_add_column('profiles', 'emailflags', {TYPE => 'MEDIUMTEXT'});
    }

    $dbh->bz_add_column('groups', 'isactive',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    $dbh->bz_add_column('attachments', 'isobsolete',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    $dbh->bz_drop_column("profiles", "emailnotification");
    $dbh->bz_drop_column("profiles", "newemailtech");

    # 2003-11-19; chicks@chicks.net; bug 225973: fix field size to accommodate
    # wider algorithms such as Blowfish. Note that this needs to be run
    # before recrypting passwords in the following block.
    $dbh->bz_alter_column('profiles', 'cryptpassword',
                          {TYPE => 'varchar(128)'});

    _recrypt_plaintext_passwords();

    # 2001-06-06 justdave@syndicomm.com:
    # There was no index on the 'who' column in the long descriptions table.
    # This caused queries by who posted comments to take a LONG time.
    #   http://bugzilla.mozilla.org/show_bug.cgi?id=57350
    $dbh->bz_add_index('longdescs', 'longdescs_who_idx', [qw(who)]);

    # 2001-06-15 kiko@async.com.br - Change bug:version size to avoid
    # truncates re http://bugzilla.mozilla.org/show_bug.cgi?id=9352
    $dbh->bz_alter_column('bugs', 'version',
                          {TYPE => 'varchar(64)', NOTNULL => 1});

    _update_bugs_activity_to_only_record_changes();

    $dbh->bz_alter_column("profiles", "disabledtext",
                          {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');

    $dbh->bz_add_column("bugs", "reporter_accessible",
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});
    $dbh->bz_add_column("bugs", "cclist_accessible",
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    $dbh->bz_add_column("bugs_activity", "attach_id", {TYPE => 'INT3'});

    _delete_logincookies_cryptpassword_and_handle_invalid_cookies();

    # qacontact/assignee should always be able to see bugs: bug 97471
    $dbh->bz_drop_column("bugs", "qacontact_accessible");
    $dbh->bz_drop_column("bugs", "assignee_accessible");

    # 2002-02-20 jeff.hedlund@matrixsi.com - bug 24789 time tracking
    $dbh->bz_add_column("longdescs", "work_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "estimated_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "remaining_time",
                        {TYPE => 'decimal(5,2)', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_add_column("bugs", "deadline", {TYPE => 'DATETIME'});

    _use_ip_instead_of_hostname_in_logincookies();

    $dbh->bz_add_column('longdescs', 'isprivate',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});
    $dbh->bz_add_column('attachments', 'isprivate',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

    $dbh->bz_add_column("bugs", "alias", {TYPE => "varchar(20)"});
    $dbh->bz_add_index('bugs', 'bugs_alias_idx',
                       {TYPE => 'UNIQUE', FIELDS => [qw(alias)]});

    _move_quips_into_db();

    $dbh->bz_drop_column("namedqueries", "watchfordiffs");

    _use_ids_for_products_and_components();
    _convert_groups_system_from_groupset();
    _convert_attachment_statuses_to_flags();
    _remove_spaces_and_commas_from_flagtypes();
    _setup_usebuggroups_backward_compatibility();
    _remove_user_series_map();
    _copy_old_charts_into_database();

    Bugzilla::Field::create_or_update(
        {name => "owner_idle_time", desc => "Time Since Assignee Touched"});

    _add_user_group_map_grant_type();
    _add_group_group_map_grant_type();

    $dbh->bz_add_column("profiles", "extern_id", {TYPE => 'varchar(64)'});

    $dbh->bz_add_column('flagtypes', 'grant_group_id', {TYPE => 'INT3'});
    $dbh->bz_add_column('flagtypes', 'request_group_id', {TYPE => 'INT3'});

    # mailto is no longer just userids
    $dbh->bz_rename_column('whine_schedules', 'mailto_userid', 'mailto');
    $dbh->bz_add_column('whine_schedules', 'mailto_type',
        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '0'});

    _add_longdescs_already_wrapped();

    # Moved enum types to separate tables so we need change the old enum 
    # types to standard varchars in the bugs table.
    $dbh->bz_alter_column('bugs', 'bug_status',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    # 2005-03-23 Tomas.Kopal@altap.cz - add default value to resolution,
    # bug 286695
    $dbh->bz_alter_column('bugs', 'resolution',
        {TYPE => 'varchar(64)', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'priority',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_alter_column('bugs', 'bug_severity',
                          {TYPE => 'varchar(64)', NOTNULL => 1});
    $dbh->bz_alter_column('bugs', 'rep_platform',
                          {TYPE => 'varchar(64)', NOTNULL => 1}, '');
    $dbh->bz_alter_column('bugs', 'op_sys',
                          {TYPE => 'varchar(64)', NOTNULL => 1});

    # When migrating quips from the '$datadir/comments' file to the DB,
    # the user ID should be NULL instead of 0 (which is an invalid user ID).
    if ($dbh->bz_column_info('quips', 'userid')->{NOTNULL}) {
        $dbh->bz_alter_column('quips', 'userid', {TYPE => 'INT3'});
        print "Changing owner to NULL for quips where the owner is",
              " unknown...\n";
        $dbh->do('UPDATE quips SET userid = NULL WHERE userid = 0');
    }

    $dbh->bz_add_index('bugs', 'bugs_short_desc_idx',
                       {TYPE => 'FULLTEXT', FIELDS => [qw(short_desc)]});

    # Right now, we only create the "thetext" index on MySQL.
    if ($dbh->isa('Bugzilla::DB::Mysql')) {
        $dbh->bz_add_index('longdescs', 'longdescs_thetext_idx',
                           {TYPE => 'FULLTEXT', FIELDS => [qw(thetext)]});
    }

    _convert_attachments_filename_from_mediumtext();

    $dbh->bz_add_column('quips', 'approved',
                        {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'TRUE'});

    # 2002-12-20 Bug 180870 - remove manual shadowdb replication code
    $dbh->bz_drop_table("shadowlog");

    _rename_votes_count_and_force_group_refresh();

    # 2004/02/15 - Summaries shouldn't be null - see bug 220232
    if (!exists $dbh->bz_column_info('bugs', 'short_desc')->{NOTNULL}) {
        $dbh->bz_alter_column('bugs', 'short_desc',
                              {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');
    }

    $dbh->bz_add_column('products', 'classification_id',
                        {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '1'});

    _fix_group_with_empty_name();

    $dbh->bz_add_index('bugs_activity', 'bugs_activity_who_idx', [qw(who)]);

    # Add defaults for some fields that should have them but didn't.
    $dbh->bz_alter_column('bugs', 'status_whiteboard',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'keywords',
        {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_alter_column('bugs', 'votes',
                          {TYPE => 'INT3', NOTNULL => 1, DEFAULT => '0'});

    $dbh->bz_alter_column('bugs', 'lastdiffed', {TYPE => 'DATETIME'});

    # 2005-03-09 qa_contact should be NULL instead of 0, bug 285534
    if ($dbh->bz_column_info('bugs', 'qa_contact')->{NOTNULL}) {
        $dbh->bz_alter_column('bugs', 'qa_contact', {TYPE => 'INT3'});
        $dbh->do("UPDATE bugs SET qa_contact = NULL WHERE qa_contact = 0");
    }

    # 2005-03-27 initialqacontact should be NULL instead of 0, bug 287483
    if ($dbh->bz_column_info('components', 'initialqacontact')->{NOTNULL}) {
        $dbh->bz_alter_column('components', 'initialqacontact', 
                              {TYPE => 'INT3'});
        $dbh->do("UPDATE components SET initialqacontact = NULL " .
                  "WHERE initialqacontact = 0");
    }

    _migrate_email_prefs_to_new_table();
    _initialize_dependency_tree_changes_email_pref();
    _change_all_mysql_booleans_to_tinyint();


}

# Subroutines should be ordered in the order that they are called.
# Thus, newer subroutines should be at the bottom.

sub _update_pre_checksetup_bugzillas {
    my $dbh = Bugzilla->dbh;
    # really old fields that were added before checksetup.pl existed
    # but aren't in very old bugzilla's (like 2.1)
    # Steve Stock (sstock@iconnect-inc.com)

    $dbh->bz_add_column('bugs', 'target_milestone',
        {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
    $dbh->bz_add_column('bugs', 'qa_contact', {TYPE => 'INT3'});
    $dbh->bz_add_column('bugs', 'status_whiteboard',
                       {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});
    $dbh->bz_add_column('products', 'disallownew',
                        {TYPE => 'BOOLEAN', NOTNULL => 1}, 0);
    $dbh->bz_add_column('products', 'milestoneurl',
                        {TYPE => 'TINYTEXT', NOTNULL => 1}, '');
    $dbh->bz_add_column('components', 'initialqacontact',
                        {TYPE => 'TINYTEXT'});
    $dbh->bz_add_column('components', 'description',
                        {TYPE => 'MEDIUMTEXT', NOTNULL => 1}, '');
}

sub _add_bug_vote_cache {
    my $dbh = Bugzilla->dbh;
    # 1999-10-11 Restructured voting database to add a cached value in each 
    # bug recording how many total votes that bug has.  While I'm at it, 
    # I removed the unused "area" field from the bugs database.  It is 
    # distressing to realize that the bugs table has reached the maximum 
    # number of indices allowed by MySQL (16), which may make future 
    # enhancements awkward.
    # (P.S. All is not lost; it appears that the latest betas of MySQL 
    # support a new table format which will allow 32 indices.)

    $dbh->bz_drop_column('bugs', 'area');
    if (!$dbh->bz_column_info('bugs', 'votes')) {
        $dbh->bz_add_column('bugs', 'votes', {TYPE => 'INT3', NOTNULL => 1,
                                              DEFAULT => 0});
        $dbh->bz_add_index('bugs', 'bugs_votes_idx', [qw(votes)]);
    }
    $dbh->bz_add_column('products', 'votesperuser',
                        {TYPE => 'INT2', NOTNULL => 1}, 0);
}

sub _update_product_name_definition {
    my $dbh = Bugzilla->dbh;
    # The product name used to be very different in various tables.
    #
    # It was   varchar(16)   in bugs
    #          tinytext      in components
    #          tinytext      in products
    #          tinytext      in versions
    #
    # tinytext is equivalent to varchar(255), which is quite huge, so I change
    # them all to varchar(64).

    # Only do this if these fields still exist - they're removed in
    # a later change
    if ($dbh->bz_column_info('products', 'product')) {
        $dbh->bz_alter_column('bugs',       'product',
                             {TYPE => 'varchar(64)', NOTNULL => 1});
        $dbh->bz_alter_column('components', 'program', {TYPE => 'varchar(64)'});
        $dbh->bz_alter_column('products',   'product', {TYPE => 'varchar(64)'});
        $dbh->bz_alter_column('versions',   'program',
                              {TYPE => 'varchar(64)', NOTNULL => 1});
    }
}

sub _add_bug_keyword_cache {
    my $dbh = Bugzilla->dbh;
    # 2000-01-16 Added a "keywords" field to the bugs table, which
    # contains a string copy of the entries of the keywords table for this
    # bug.  This is so that I can easily sort and display a keywords
    # column in bug lists.

    if (!$dbh->bz_column_info('bugs', 'keywords')) {
        $dbh->bz_add_column('bugs', 'keywords',
            {TYPE => 'MEDIUMTEXT', NOTNULL => 1, DEFAULT => "''"});

        my @kwords;
        print "Making sure 'keywords' field of table 'bugs' is empty...\n";
        $dbh->do("UPDATE bugs SET keywords = '' WHERE keywords != ''");
        print "Repopulating 'keywords' field of table 'bugs'...\n";
        my $sth = $dbh->prepare("SELECT keywords.bug_id, keyworddefs.name " .
                                  "FROM keywords, keyworddefs " .
                                 "WHERE keyworddefs.id = keywords.keywordid " .
                              "ORDER BY keywords.bug_id, keyworddefs.name");
        $sth->execute;
        my @list;
        my $bugid = 0;
        my @row;
        while (1) {
            my ($b, $k) = ($sth->fetchrow_array());
            if (!defined $b || $b ne $bugid) {
                if (@list) {
                    $dbh->do("UPDATE bugs SET keywords = " .
                             $dbh->quote(join(', ', @list)) .
                             " WHERE bug_id = $bugid");
                }
                last if !$b;
                $bugid = $b;
                @list = ();
            }
            push(@list, $k);
        }
    }
}

# A helper for the function below.
sub _write_one_longdesc {
    my ($id, $who, $when, $buffer) = (@_);
    my $dbh = Bugzilla->dbh;
    $buffer = trim($buffer);
    return if !$buffer;
    $dbh->do("INSERT INTO longdescs (bug_id, who, bug_when, thetext)
                   VALUES (?,?,?,?)", undef, $id, $who, 
             time2str("%Y/%m/%d %H:%M:%S", $when), $buffer);
}

sub _populate_longdescs {
    my $dbh = Bugzilla->dbh;
    # 2000-01-20 Added a new "longdescs" table, which is supposed to have 
    # all the long descriptions in it, replacing the old long_desc field 
    # in the bugs table. The below hideous code populates this new table 
    # with things from the old field, with ugly parsing and heuristics.

    if ($dbh->bz_column_info('bugs', 'long_desc')) {
        my ($total) = $dbh->selectrow_array("SELECT COUNT(*) FROM bugs");

        print "Populating new long_desc table. This is slow. There are",
              " $total\nbugs to process; a line of dots will be printed",
              " for each 50.\n\n";
        local $| = 1;

        $dbh->bz_lock_tables('bugs write', 'longdescs write', 'profiles write',
                             'bz_schema WRITE');

        $dbh->do('DELETE FROM longdescs');

        my $sth = $dbh->prepare("SELECT bug_id, creation_ts, reporter,
                                        long_desc FROM bugs ORDER BY bug_id");
        $sth->execute();
        my $count = 0;
        while (my ($id, $createtime, $reporterid, $desc) = 
                   $sth->fetchrow_array()) 
        {
            $count++;
            indicate_progress({ total => $total, current => $count });
            $desc =~ s/\r//g;
            my $who = $reporterid;
            my $when = str2time($createtime);
            my $buffer = "";
            foreach my $line (split(/\n/, $desc)) {
                $line =~ s/\s+$//g; # Trim trailing whitespace.
                if ($line =~ /^------- Additional Comments From ([^\s]+)\s+(\d.+\d)\s+-------$/) 
                {
                    my $name = $1;
                    my $date = str2time($2);
                    # Oy, what a hack.  The creation time is accurate to the
                    # second. But the long text only contains things accurate
                    # to the And so, if someone makes a comment within a 
                    # minute of the original bug creation, then the comment can
                    # come *before* the bug creation.  So, we add 59 seconds to
                    # the time of all comments, so that they are always 
                    # considered to have happened at the *end* of the given
                    # minute, not the beginning.
                    $date += 59;
                    if ($date >= $when) {
                        _write_one_longdesc($id, $who, $when, $buffer);
                        $buffer = "";
                        $when = $date;
                        my $s2 = $dbh->prepare("SELECT userid FROM profiles " .
                                                "WHERE login_name = ?");
                        $s2->execute($name);
                        ($who) = ($s2->fetchrow_array());

                        if (!$who) {
                            # This username doesn't exist.  Maybe someone
                            # renamed him or something.  Invent a new profile
                            # entry disabled, just to represent him.
                            $dbh->do("INSERT INTO profiles (login_name, 
                                      cryptpassword, disabledtext) 
                                      VALUES (?,?,?)", undef, $name, '*',
                                      "Account created only to maintain"
                                      . " database integrity");
                            $who = $dbh->bz_last_key('profiles', 'userid');
                        }
                        next;
                    }
                }
                $buffer .= $line . "\n";
            }
            _write_one_longdesc($id, $who, $when, $buffer);
        } # while loop

        print "\n\n";
        $dbh->bz_drop_column('bugs', 'long_desc');
        $dbh->bz_unlock_tables();
    } # main if
}

sub _update_bugs_activity_field_to_fieldid {
    my $dbh = Bugzilla->dbh;

    # 2000-01-18 Added a new table fielddefs that records information about the
    # different fields we keep an activity log on.  The bugs_activity table
    # now has a pointer into that table instead of recording the name directly.
    if ($dbh->bz_column_info('bugs_activity', 'field')) {
        $dbh->bz_add_column('bugs_activity', 'fieldid',
                            {TYPE => 'INT3', NOTNULL => 1}, 0);

        $dbh->bz_add_index('bugs_activity', 'bugs_activity_fieldid_idx',
                           [qw(fieldid)]);
        print "Populating new bugs_activity.fieldid field...\n";

        $dbh->bz_lock_tables('bugs_activity WRITE', 'fielddefs WRITE');


        my $ids = $dbh->selectall_arrayref(
            'SELECT DISTINCT fielddefs.id, bugs_activity.field
               FROM bugs_activity LEFT JOIN fielddefs 
                    ON bugs_activity.field = fielddefs.name', {Slice=>{}});

        foreach my $item (@$ids) {
            my $id    = $item->{id};
            my $field = $item->{field};
            # If the id is NULL
            if (!$id) {
                $dbh->do("INSERT INTO fielddefs (name, description) VALUES " .
                         "(?, ?)", undef, $field, $field);
                $id = $dbh->bz_last_key('fielddefs', 'id');
            }
            $dbh->do("UPDATE bugs_activity SET fieldid = ? WHERE field = ?",
                     undef, $id, $field);
        }
        $dbh->bz_unlock_tables();

        $dbh->bz_drop_column('bugs_activity', 'field');
    }
}

sub _add_unique_login_name_index_to_profiles {
    my $dbh = Bugzilla->dbh;

    # 2000-01-22 The "login_name" field in the "profiles" table was not
    # declared to be unique.  Sure enough, somehow, I got 22 duplicated entries
    # in my database.  This code detects that, cleans up the duplicates, and
    # then tweaks the table to declare the field to be unique.  What a pain.
    if (!$dbh->bz_index_info('profiles', 'profiles_login_name_idx') ||
        !$dbh->bz_index_info('profiles', 'profiles_login_name_idx')->{TYPE}) {
        print "Searching for duplicate entries in the profiles table...\n";
        while (1) {
            # This code is weird in that it loops around and keeps doing this
            # select again.  That's because I'm paranoid about deleting entries
            # out from under us in the profiles table.  Things get weird if
            # there are *three* or more entries for the same user...
            my $sth = $dbh->prepare("SELECT p1.userid, p2.userid, p1.login_name
                                       FROM profiles AS p1, profiles AS p2
                                      WHERE p1.userid < p2.userid
                                            AND p1.login_name = p2.login_name
                                   ORDER BY p1.login_name");
            $sth->execute();
            my ($u1, $u2, $n) = ($sth->fetchrow_array);
            last if !$u1;

            print "Both $u1 & $u2 are ids for $n!  Merging $u2 into $u1...\n";
            foreach my $i (["bugs", "reporter"],
                           ["bugs", "assigned_to"],
                           ["bugs", "qa_contact"],
                           ["attachments", "submitter_id"],
                           ["bugs_activity", "who"],
                           ["cc", "who"],
                           ["votes", "who"],
                           ["longdescs", "who"]) {
                my ($table, $field) = (@$i);
                print "   Updating $table.$field...\n";
                $dbh->do("UPDATE $table SET $field = $u1 " .
                          "WHERE $field = $u2");
            }
            $dbh->do("DELETE FROM profiles WHERE userid = $u2");
        }
        print "OK, changing index type to prevent duplicates in the",
              " future...\n";

        $dbh->bz_drop_index('profiles', 'profiles_login_name_idx');
        $dbh->bz_add_index('profiles', 'profiles_login_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(login_name)]});
    }
}

sub _update_component_user_fields_to_ids {
    my $dbh = Bugzilla->dbh;

    # components.initialowner
    my $comp_init_owner = $dbh->bz_column_info('components', 'initialowner');
    if ($comp_init_owner && $comp_init_owner->{TYPE} eq 'TINYTEXT') {
        my $sth = $dbh->prepare("SELECT program, value, initialowner
                                   FROM components");
        $sth->execute();
        while (my ($program, $value, $initialowner) = $sth->fetchrow_array()) {
            my ($id) = $dbh->selectrow_array(
                "SELECT userid FROM profiles WHERE login_name = ?",
                undef, $initialowner);

            unless (defined $id) {
                print "Warning: You have an invalid default assignee",
                      " '$initialowner'\n in component '$value' of program",
                      " '$program'!\n";
                $id = 0;
            }

            $dbh->do("UPDATE components SET initialowner = ?
                       WHERE program = ? AND value = ?", undef,
                     $id, $program, $value);
        }
        $dbh->bz_alter_column('components','initialowner',{TYPE => 'INT3'});
    }

    # components.initialqacontact
    my $comp_init_qa = $dbh->bz_column_info('components', 'initialqacontact');
    if ($comp_init_qa && $comp_init_qa->{TYPE} eq 'TINYTEXT') {
        my $sth = $dbh->prepare("SELECT program, value, initialqacontact
                                   FROM components");
        $sth->execute();
        while (my ($program, $value, $initialqacontact) = 
                   $sth->fetchrow_array()) 
        {
            my ($id) = $dbh->selectrow_array(
                "SELECT userid FROM profiles WHERE login_name = ?",
                undef, $initialqacontact);

            unless (defined $id) {
                if ($initialqacontact) {
                    print "Warning: You have an invalid default QA contact",
                          " $initialqacontact' in program '$program',",
                          " component '$value'!\n";
                }
                $id = 0;
            }

            $dbh->do("UPDATE components SET initialqacontact = ?
                       WHERE program = ? AND value = ?", undef,
                     $id, $program, $value);
        }

        $dbh->bz_alter_column('components','initialqacontact',{TYPE => 'INT3'});
    }
}

sub _populate_milestones_table {
    my $dbh = Bugzilla->dbh;
    # 2000-03-21 Adding a table for target milestones to
    # database - matthew@zeroknowledge.com
    # If the milestones table is empty, and we're still back in a Bugzilla
    # that has a bugs.product field, that means that we just created
    # the milestones table and it needs to be populated.
    my $milestones_exist = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM milestones");
    if (!$milestones_exist && $dbh->bz_column_info('bugs', 'product')) {
        print "Replacing blank milestones...\n";

        $dbh->do("UPDATE bugs
                     SET target_milestone = '---'
                   WHERE target_milestone = ' '");

        # If we are upgrading from 2.8 or earlier, we will have *created*
        # the milestones table with a product_id field, but Bugzilla expects
        # it to have a "product" field. So we change the field backward so
        # other code can run. The change will be reversed later in checksetup.
        if ($dbh->bz_column_info('milestones', 'product_id')) {
            # Dropping the column leaves us with a milestones_product_id_idx
            # index that is only on the "value" column. We need to drop the
            # whole index so that it can be correctly re-created later.
            $dbh->bz_drop_index('milestones', 'milestones_product_id_idx');
            $dbh->bz_drop_column('milestones', 'product_id');
            $dbh->bz_add_column('milestones', 'product',
                                {TYPE => 'varchar(64)', NOTNULL => 1}, '');
        }

        # Populate the milestone table with all existing values in the database
        my $sth = $dbh->prepare("SELECT DISTINCT target_milestone, product 
                                   FROM bugs");
        $sth->execute();

        print "Populating milestones table...\n";

        while (my ($value, $product) = $sth->fetchrow_array()) {
            # check if the value already exists
            my $sortkey = substr($value, 1);
            if ($sortkey !~ /^\d+$/) {
                $sortkey = 0;
            } else {
                $sortkey *= 10;
            }
            my $ms_exists = $dbh->selectrow_array(
                "SELECT value FROM milestones
                  WHERE value = ? AND product = ?", undef, $value, $product);

            if (!$ms_exists) {
                $dbh->do("INSERT INTO milestones(value, product, sortkey) 
                          VALUES (?,?,?)", undef, $value, $product, $sortkey);
            }
        }
    }
}

sub _add_products_defaultmilestone {
    my $dbh = Bugzilla->dbh;

    # 2000-03-23 Added a defaultmilestone field to the products table, so that
    # we know which milestone to initially assign bugs to.
    if (!$dbh->bz_column_info('products', 'defaultmilestone')) {
        $dbh->bz_add_column('products', 'defaultmilestone',
                 {TYPE => 'varchar(20)', NOTNULL => 1, DEFAULT => "'---'"});
        my $sth = $dbh->prepare(
            "SELECT product, defaultmilestone FROM products");
        $sth->execute();
        while (my ($product, $default_ms) = $sth->fetchrow_array()) {
            my $exists = $dbh->selectrow_array(
                "SELECT value FROM milestones
                  WHERE value = ? AND product = ?", 
                undef, $default_ms, $product);
            if (!$exists) {
                $dbh->do("INSERT INTO milestones(value, product) " .
                         "VALUES (?, ?)", undef, $default_ms, $product);
            }
        }
    }
}

sub _copy_from_comments_to_longdescs {
    my $dbh = Bugzilla->dbh;
    # 2000-11-27 For Bugzilla 2.5 and later. Copy data from 'comments' to
    # 'longdescs' - the new name of the comments table.
    if ($dbh->bz_table_info('comments')) {
        my $quoted_when = $dbh->quote_identifier('when');
        $dbh->do("INSERT INTO longdescs (bug_when, bug_id, who, thetext)
                  SELECT $quoted_when, bug_id, who, comment
                    FROM comments");
        $dbh->bz_drop_table("comments");
    }
}

sub _populate_duplicates_table {
    my $dbh = Bugzilla->dbh;
    # 2000-07-15 Added duplicates table so Bugzilla tracks duplicates in a
    # better way than it used to. This code searches the comments to populate
    # the table initially. It's executed if the table is empty; if it's 
    # empty because there are no dupes (as opposed to having just created 
    # the table) it won't have any effect anyway, so it doesn't matter.
    my ($dups_exist) = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM duplicates");
    # We also check against a schema change that happened later.
    if (!$dups_exist && !$dbh->bz_column_info('groups', 'isactive')) {
        # populate table
        print "Populating duplicates table from comments...\n";

        my $sth = $dbh->prepare(
             "SELECT longdescs.bug_id, thetext
                FROM longdescs LEFT JOIN bugs 
                     ON longdescs.bug_id = bugs.bug_id
               WHERE (" . $dbh->sql_regexp("thetext", 
                          "'[.*.]{3} This bug has been marked as a duplicate"
                          . " of [[:digit:]]+ [.*.]{3}'") 
                  . ")
                     AND resolution = 'DUPLICATE'
            ORDER BY longdescs.bug_when");
        $sth->execute();

        my (%dupes, $key);
        # Because of the way hashes work, this loop removes all but the 
        # last dupe resolution found for a given bug.
        while (my ($dupe, $dupe_of) = $sth->fetchrow_array()) {
            $dupes{$dupe} = $dupe_of;
        }

        foreach $key (keys(%dupes)){
            $dupes{$key} =~ /^.*\*\*\* This bug has been marked as a duplicate of (\d+) \*\*\*$/ms;
            $dupes{$key} = $1;
            $dbh->do("INSERT INTO duplicates VALUES(?, ?)", undef,
                     $dupes{$key},  $key);
            #        BugItsADupeOf  Dupe
        }
    }
}

sub _recrypt_plaintext_passwords {
    my $dbh = Bugzilla->dbh;
    # 2001-06-12; myk@mozilla.org; bugs 74032, 77473:
    # Recrypt passwords using Perl &crypt instead of the mysql equivalent
    # and delete plaintext passwords from the database.
    if ($dbh->bz_column_info('profiles', 'password')) {

        print <<ENDTEXT;
Your current installation of Bugzilla stores passwords in plaintext
in the database and uses mysql's encrypt function instead of Perl's
crypt function to crypt passwords.  Passwords are now going to be
re-crypted with the Perl function, and plaintext passwords will be
deleted from the database.  This could take a while if your
installation has many users.
ENDTEXT

        # Re-crypt everyone's password.
        my $sth = $dbh->prepare("SELECT userid, password FROM profiles");
        $sth->execute();

        my $total = $sth->rows;
        my $i = 1;

        print "Fixing passwords...\n";
        while (my ($userid, $password) = $sth->fetchrow_array()) {
            my $cryptpassword = $dbh->quote(bz_crypt($password));
            $dbh->do("UPDATE profiles " .
                        "SET cryptpassword = $cryptpassword " .
                      "WHERE userid = $userid");
            indicate_progress({ total => $total, current => $i, every => 10 });
        }
        print "\n";

        # Drop the plaintext password field.
        $dbh->bz_drop_column('profiles', 'password');
    }
}

sub _update_bugs_activity_to_only_record_changes {
    my $dbh = Bugzilla->dbh;
    # 2001-07-20 jake@bugzilla.org - Change bugs_activity to only record changes
    #  http://bugzilla.mozilla.org/show_bug.cgi?id=55161
    if ($dbh->bz_column_info('bugs_activity', 'oldvalue')) {
        $dbh->bz_add_column("bugs_activity", "removed", {TYPE => "TINYTEXT"});
        $dbh->bz_add_column("bugs_activity", "added", {TYPE => "TINYTEXT"});

        # Need to get field id's for the fields that have multiple values
        my @multi;
        foreach my $f ("cc", "dependson", "blocked", "keywords") {
            my $sth = $dbh->prepare("SELECT id " .
                                    "FROM fielddefs " .
                                    "WHERE name = '$f'");
            $sth->execute();
            my ($fid) = $sth->fetchrow_array();
            push (@multi, $fid);
        }

        # Now we need to process the bugs_activity table and reformat the data
        print "Fixing activity log...\n";
        my $sth = $dbh->prepare("SELECT bug_id, who, bug_when, fieldid,
                                oldvalue, newvalue FROM bugs_activity");
        $sth->execute;
        my $i = 0;
        my $total = $sth->rows;
        while (my ($bug_id, $who, $bug_when, $fieldid, $oldvalue, $newvalue) 
                   = $sth->fetchrow_array()) 
        {
            $i++;
            indicate_progress({ total => $total, current => $i, every => 10 });
            # Make sure (old|new)value isn't null (to suppress warnings)
            $oldvalue ||= "";
            $newvalue ||= "";
            my ($added, $removed) = "";
            if (grep ($_ eq $fieldid, @multi)) {
                $oldvalue =~ s/[\s,]+/ /g;
                $newvalue =~ s/[\s,]+/ /g;
                my @old = split(" ", $oldvalue);
                my @new = split(" ", $newvalue);
                my (@add, @remove) = ();
                # Find values that were "added"
                foreach my $value(@new) {
                    if (! grep ($_ eq $value, @old)) {
                        push (@add, $value);
                    }
                }
                # Find values that were removed
                foreach my $value(@old) {
                    if (! grep ($_ eq $value, @new)) {
                        push (@remove, $value);
                    }
                }
                $added = join (", ", @add);
                $removed = join (", ", @remove);
                # If we can't determine what changed, put a ? in both fields
                unless ($added || $removed) {
                    $added = "?";
                    $removed = "?";
                }
                # If the original field (old|new)value was full, then this
                # could be incomplete data.
                if (length($oldvalue) == 255 || length($newvalue) == 255) {
                    $added = "? $added";
                    $removed = "? $removed";
                }
            } else {
                $removed = $oldvalue;
                $added = $newvalue;
            }
            $added = $dbh->quote($added);
            $removed = $dbh->quote($removed);
            $dbh->do("UPDATE bugs_activity 
                         SET removed = $removed, added = $added
                       WHERE bug_id = $bug_id AND who = $who
                             AND bug_when = '$bug_when' 
                             AND fieldid = $fieldid");
        }
        print "\n";
        $dbh->bz_drop_column("bugs_activity", "oldvalue");
        $dbh->bz_drop_column("bugs_activity", "newvalue");
    }
}

sub _delete_logincookies_cryptpassword_and_handle_invalid_cookies {
    my $dbh = Bugzilla->dbh;
    # 2002-02-04 bbaetz@student.usyd.edu.au bug 95732
    # Remove logincookies.cryptpassword, and delete entries which become
    # invalid
    if ($dbh->bz_column_info("logincookies", "cryptpassword")) {
        # We need to delete any cookies which are invalid before dropping the
        # column
        print "Removing invalid login cookies...\n";

        # mysql doesn't support DELETE with multi-table queries, so we have
        # to iterate
        my $sth = $dbh->prepare("SELECT cookie FROM logincookies, profiles " .
                                "WHERE logincookies.cryptpassword != " .
                                "profiles.cryptpassword AND " .
                                "logincookies.userid = profiles.userid");
        $sth->execute();
        while (my ($cookie) = $sth->fetchrow_array()) {
            $dbh->do("DELETE FROM logincookies WHERE cookie = $cookie");
        }

        $dbh->bz_drop_column("logincookies", "cryptpassword");
    }
}

sub _use_ip_instead_of_hostname_in_logincookies {
    my $dbh = Bugzilla->dbh;

    # 2002-03-15 bbaetz@student.usyd.edu.au - bug 129466
    # 2002-05-13 preed@sigkill.com - bug 129446 patch backported to the
    #  BUGZILLA-2_14_1-BRANCH as a security blocker for the 2.14.2 release
    #
    # Use the ip, not the hostname, in the logincookies table
    if ($dbh->bz_column_info("logincookies", "hostname")) {
        # We've changed what we match against, so all entries are now invalid
        $dbh->do("DELETE FROM logincookies");

        # Now update the logincookies schema
        $dbh->bz_drop_column("logincookies", "hostname");
        $dbh->bz_add_column("logincookies", "ipaddr",
                            {TYPE => 'varchar(40)', NOTNULL => 1}, '');
    }
}

sub _move_quips_into_db {
    my $dbh = Bugzilla->dbh;
    my $datadir = bz_locations->{'datadir'};
    # 2002-07-15 davef@tetsubo.com - bug 67950
    # Move quips to the db.
    if (-e "$datadir/comments") {
        print "Populating quips table from $datadir/comments...\n";
        my $comments = new IO::File("$datadir/comments", 'r')
            || die "$datadir/comments: $!";
        $comments->untaint;
        while (<$comments>) {
            chomp;
            $dbh->do("INSERT INTO quips (quip) VALUES (?)", undef, $_);
        }

        print <<EOT;

Quips are now stored in the database, rather than in an external file.
The quips previously stored in $datadir/comments have been copied into
the database, and that file has been renamed to $datadir/comments.bak
You may delete the renamed file once you have confirmed that all your
quips were moved successfully.

EOT
        $comments->close;
        rename("$datadir/comments", "$datadir/comments.bak")
            || warn "Failed to rename: $!";
    }
}

sub _use_ids_for_products_and_components {
    my $dbh = Bugzilla->dbh;
    # 2002-08-12 jake@bugzilla.org/bbaetz@student.usyd.edu.au - bug 43600
    # Use integer IDs for products and components.
    if ($dbh->bz_column_info("products", "product")) {
        print "Updating database to use product IDs.\n";

        # First, we need to remove possible NULL entries
        # NULLs may exist, but won't have been used, since all the uses of them
        # are in NOT NULL fields in other tables
        $dbh->do("DELETE FROM products WHERE product IS NULL");
        $dbh->do("DELETE FROM components WHERE value IS NULL");

        $dbh->bz_add_column("products", "id",
            {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
        $dbh->bz_add_column("components", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("versions", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("milestones", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);
        $dbh->bz_add_column("bugs", "product_id",
            {TYPE => 'INT2', NOTNULL => 1}, 0);

        # The attachstatusdefs table was added in version 2.15, but 
        # removed again in early 2.17.  If it exists now, we still need 
        # to perform this change with product_id because the code later on
        # which converts the attachment statuses to flags depends on it.
        # But we need to avoid this if the user is upgrading from 2.14
        # or earlier (because it won't be there to convert).
        if ($dbh->bz_table_info("attachstatusdefs")) {
            $dbh->bz_add_column("attachstatusdefs", "product_id",
                {TYPE => 'INT2', NOTNULL => 1}, 0);
        }

        my %products;
        my $sth = $dbh->prepare("SELECT id, product FROM products");
        $sth->execute;
        while (my ($product_id, $product) = $sth->fetchrow_array()) {
            if (exists $products{$product}) {
                print "Ignoring duplicate product $product\n";
                $dbh->do("DELETE FROM products WHERE id = $product_id");
                next;
            }
            $products{$product} = 1;
            $dbh->do("UPDATE components SET product_id = $product_id " .
                     "WHERE program = " . $dbh->quote($product));
            $dbh->do("UPDATE versions SET product_id = $product_id " .
                     "WHERE program = " . $dbh->quote($product));
            $dbh->do("UPDATE milestones SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product));
            $dbh->do("UPDATE bugs SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product));
            $dbh->do("UPDATE attachstatusdefs SET product_id = $product_id " .
                     "WHERE product = " . $dbh->quote($product))
                if $dbh->bz_table_info("attachstatusdefs");
            }

            print "Updating the database to use component IDs.\n";
        $dbh->bz_add_column("components", "id",
            {TYPE => 'SMALLSERIAL', NOTNULL => 1, PRIMARYKEY => 1});
        $dbh->bz_add_column("bugs", "component_id",
                            {TYPE => 'INT2', NOTNULL => 1}, 0);

        my %components;
        $sth = $dbh->prepare("SELECT id, value, product_id FROM components");
        $sth->execute;
        while (my ($component_id, $component, $product_id) 
            = $sth->fetchrow_array()) 
        {
            if (exists $components{$component}) {
                if (exists $components{$component}{$product_id}) {
                    print "Ignoring duplicate component $component for",
                          " product $product_id\n";
                    $dbh->do("DELETE FROM components WHERE id = $component_id");
                    next;
                }
            } else {
                $components{$component} = {};
            }
            $components{$component}{$product_id} = 1;
            $dbh->do("UPDATE bugs SET component_id = $component_id " .
                      "WHERE component = " . $dbh->quote($component) .
                       " AND product_id = $product_id");
        }
        print "Fixing Indexes and Uniqueness.\n";
        $dbh->bz_drop_index('milestones', 'milestones_product_idx');

        $dbh->bz_add_index('milestones', 'milestones_product_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(product_id value)]});

        $dbh->bz_drop_index('bugs', 'bugs_product_idx');
        $dbh->bz_add_index('bugs', 'bugs_product_id_idx', [qw(product_id)]);
        $dbh->bz_drop_index('bugs', 'bugs_component_idx');
        $dbh->bz_add_index('bugs', 'bugs_component_id_idx', [qw(component_id)]);

        print "Removing, renaming, and retyping old product and",
              " component fields.\n";
        $dbh->bz_drop_column("components", "program");
        $dbh->bz_drop_column("versions", "program");
        $dbh->bz_drop_column("milestones", "product");
        $dbh->bz_drop_column("bugs", "product");
        $dbh->bz_drop_column("bugs", "component");
        $dbh->bz_drop_column("attachstatusdefs", "product")
            if $dbh->bz_table_info("attachstatusdefs");
        $dbh->bz_rename_column("products", "product", "name");
        $dbh->bz_alter_column("products", "name",
                              {TYPE => 'varchar(64)', NOTNULL => 1});
        $dbh->bz_rename_column("components", "value", "name");
        $dbh->bz_alter_column("components", "name",
                              {TYPE => 'varchar(64)', NOTNULL => 1});

        print "Adding indexes for products and components tables.\n";
        $dbh->bz_add_index('products', 'products_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(name)]});
        $dbh->bz_add_index('components', 'components_product_id_idx',
            {TYPE => 'UNIQUE', FIELDS => [qw(product_id name)]});
        $dbh->bz_add_index('components', 'components_name_idx', [qw(name)]);
    }
}

# Helper for the below function.
#
# _list_bits(arg) returns a list of UNKNOWN<n> if the group
# has been deleted for all bits set in arg. When the activity
# records are converted from groupset numbers to lists of
# group names, _list_bits is used to fill in a list of references
# to groupset bits for groups that no longer exist.
sub _list_bits {
    my ($num) = @_;
    my $dbh = Bugzilla->dbh;
    my @res;
    my $curr = 1;
    while (1) {
        # Convert a big integer to a list of bits
        my $sth = $dbh->prepare("SELECT ($num & ~$curr) > 0,
                                        ($num & $curr),
                                        ($num & ~$curr),
                                        $curr << 1");
        $sth->execute;
        my ($more, $thisbit, $remain, $nval) = $sth->fetchrow_array;
        push @res,"UNKNOWN<$curr>" if ($thisbit);
        $curr = $nval;
        $num = $remain;
        last if !$more;
    }
    return @res;
}

sub _convert_groups_system_from_groupset {
    my $dbh = Bugzilla->dbh;
    # 2002-09-22 - bugreport@peshkin.net - bug 157756
    #
    # If the whole groups system is new, but the installation isn't,
    # convert all the old groupset groups, etc...
    #
    # This requires:
    # 1) define groups ids in group table
    # 2) populate user_group_map with grants from old groupsets 
    #    and blessgroupsets
    # 3) populate bug_group_map with data converted from old bug groupsets
    # 4) convert activity logs to use group names instead of numbers
    # 5) identify the admin from the old all-ones groupset

    # The groups system needs to be converted if groupset exists
    if ($dbh->bz_column_info("profiles", "groupset")) {
        $dbh->bz_add_column('groups', 'last_changed',
            {TYPE => 'DATETIME', NOTNULL => 1}, '0000-00-00 00:00:00');

        # Some mysql versions will promote any unique key to primary key
        # so all unique keys are removed first and then added back in
        $dbh->bz_drop_index('groups', 'groups_bit_idx');
        $dbh->bz_drop_index('groups', 'groups_name_idx');
        if ($dbh->primary_key(undef, undef, 'groups')) {
            $dbh->do("ALTER TABLE groups DROP PRIMARY KEY");
        }

        $dbh->bz_add_column('groups', 'id',
            {TYPE => 'MEDIUMSERIAL', NOTNULL => 1, PRIMARYKEY => 1});

        $dbh->bz_add_index('groups', 'groups_name_idx',
                           {TYPE => 'UNIQUE', FIELDS => [qw(name)]});
        $dbh->bz_add_column('profiles', 'refreshed_when',
            {TYPE => 'DATETIME', NOTNULL => 1}, '0000-00-00 00:00:00');

        # Convert all existing groupset records to map entries before removing
        # groupset fields or removing "bit" from groups.
        my $sth = $dbh->prepare("SELECT bit, id FROM groups WHERE bit > 0");
        $sth->execute();
        while (my ($bit, $gid) = $sth->fetchrow_array) {
            # Create user_group_map membership grants for old groupsets.
            # Get each user with the old groupset bit set
            my $sth2 = $dbh->prepare("SELECT userid FROM profiles
                                       WHERE (groupset & $bit) != 0");
            $sth2->execute();
            while (my ($uid) = $sth2->fetchrow_array) {
                # Check to see if the user is already a member of the group
                # and, if not, insert a new record.
                my $query = "SELECT user_id FROM user_group_map
                              WHERE group_id = $gid AND user_id = $uid
                                    AND isbless = 0";
                my $sth3 = $dbh->prepare($query);
                $sth3->execute();
                if ( !$sth3->fetchrow_array() ) {
                    $dbh->do("INSERT INTO user_group_map
                              (user_id, group_id, isbless, grant_type)
                              VALUES ($uid, $gid, 0, " . GRANT_DIRECT . ")");
                }
            }
            # Create user can bless group grants for old groupsets, but only
            # if we're upgrading from a Bugzilla that had blessing.
            if($dbh->bz_column_info('profiles', 'blessgroupset')) {
                # Get each user with the old blessgroupset bit set
                $sth2 = $dbh->prepare("SELECT userid FROM profiles
                                        WHERE (blessgroupset & $bit) != 0");
                $sth2->execute();
                while (my ($uid) = $sth2->fetchrow_array) {
                    $dbh->do("INSERT INTO user_group_map
                              (user_id, group_id, isbless, grant_type)
                              VALUES ($uid, $gid, 1, " . GRANT_DIRECT . ")");
                }
            }
            # Create bug_group_map records for old groupsets.
            # Get each bug with the old group bit set.
            $sth2 = $dbh->prepare("SELECT bug_id FROM bugs
                                    WHERE (groupset & $bit) != 0");
            $sth2->execute();
            while (my ($bug_id) = $sth2->fetchrow_array) {
               # Insert the bug, group pair into the bug_group_map.
                $dbh->do("INSERT INTO bug_group_map (bug_id, group_id)
                          VALUES ($bug_id, $gid)");
            }
        }
        # Replace old activity log groupset records with lists of names 
        # of groups. Start by defining the bug_group field and getting its id.
        Bugzilla::Field::create_or_update(
            {name => "bug_group", desc => "Group"});
        $sth = $dbh->prepare("SELECT id FROM fielddefs
                               WHERE name = " . $dbh->quote('bug_group'));
        $sth->execute();
        my ($bgfid) = $sth->fetchrow_array;
        # Get the field id for the old groupset field
        $sth = $dbh->prepare("SELECT id FROM fielddefs
                               WHERE name = " . $dbh->quote('groupset'));
        $sth->execute();
        my ($gsid) = $sth->fetchrow_array;
        # Get all bugs_activity records from groupset changes
        if ($gsid) {
            $sth = $dbh->prepare("SELECT bug_id, bug_when, who, added, removed
                                    FROM bugs_activity WHERE fieldid = $gsid");
            $sth->execute();
            while (my ($bug_id, $bug_when, $who, $added, $removed) = 
                       $sth->fetchrow_array) 
            {
                $added ||= 0;
                $removed ||= 0;
                # Get names of groups added.
                my $sth2 = $dbh->prepare("SELECT name FROM groups
                                           WHERE (bit & $added) != 0
                                                 AND (bit & $removed) = 0");
                $sth2->execute();
                my @logadd;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logadd, $n;
                }
                # Get names of groups removed.
                $sth2 = $dbh->prepare("SELECT name FROM groups
                                        WHERE (bit & $removed) != 0
                                              AND (bit & $added) = 0");
                $sth2->execute();
                my @logrem;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logrem, $n;
                }
                # Get list of group bits added that correspond to 
                # missing groups.
                $sth2 = $dbh->prepare("SELECT ($added & ~BIT_OR(bit)) 
                                         FROM groups");
                $sth2->execute();
                my ($miss) = $sth2->fetchrow_array;
                if ($miss) {
                    push @logadd, _list_bits($miss);
                    print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id",
                          " CONTAINS DELETED GROUPS\n";
                }
                # Get list of group bits deleted that correspond to 
                # missing groups.
                $sth2 = $dbh->prepare("SELECT ($removed & ~BIT_OR(bit)) 
                                         FROM groups");
                $sth2->execute();
                ($miss) = $sth2->fetchrow_array;
                if ($miss) {
                    push @logrem, _list_bits($miss);
                    print "\nWARNING - GROUPSET ACTIVITY ON BUG $bug_id",
                          " CONTAINS DELETED GROUPS\n";
                }
                my $logr = "";
                my $loga = "";
                $logr = join(", ", @logrem) . '?' if @logrem;
                $loga = join(", ", @logadd) . '?' if @logadd;
                # Replace to old activity record with the converted data.
                $dbh->do("UPDATE bugs_activity SET fieldid = $bgfid, added = " .
                          $dbh->quote($loga) . ", removed = " .
                          $dbh->quote($logr) .
                         " WHERE bug_id = $bug_id AND bug_when = " .
                          $dbh->quote($bug_when) .
                         " AND who = $who AND fieldid = $gsid");
            }
            # Replace groupset changes with group name changes in
            # profiles_activity. Get profiles_activity records for groupset.
            $sth = $dbh->prepare(
                 "SELECT userid, profiles_when, who, newvalue, oldvalue " .
                   "FROM profiles_activity " .
                  "WHERE fieldid = $gsid");
            $sth->execute();
            while (my ($uid, $uwhen, $uwho, $added, $removed) = 
                       $sth->fetchrow_array) 
            {
                $added ||= 0;
                $removed ||= 0;
                # Get names of groups added.
                my $sth2 = $dbh->prepare("SELECT name FROM groups
                                           WHERE (bit & $added) != 0
                                                 AND (bit & $removed) = 0");
                $sth2->execute();
                my @logadd;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logadd, $n;
                }
                # Get names of groups removed.
                $sth2 = $dbh->prepare("SELECT name FROM groups
                                        WHERE (bit & $removed) != 0
                                              AND (bit & $added) = 0");
                $sth2->execute();
                my @logrem;
                while (my ($n) = $sth2->fetchrow_array) {
                    push @logrem, $n;
                }
                my $ladd = "";
                my $lrem = "";
                $ladd = join(", ", @logadd) . '?' if @logadd;
                $lrem = join(", ", @logrem) . '?' if @logrem;
                # Replace profiles_activity record for groupset change
                # with group list.
                $dbh->do("UPDATE profiles_activity " .
                            "SET fieldid = $bgfid, newvalue = " .
                          $dbh->quote($ladd) . ", oldvalue = " .
                          $dbh->quote($lrem) .
                         " WHERE userid = $uid AND profiles_when = " .
                          $dbh->quote($uwhen) .
                               " AND who = $uwho AND fieldid = $gsid");
            }
        }

        # Identify admin group.
        my ($admin_gid) = $dbh->selectrow_array(
            "SELECT id FROM groups WHERE name = 'admin'");
        if (!$admin_gid) {
            $dbh->do(q{INSERT INTO groups (name, description)
                                   VALUES ('admin', 'Administrators')});
            $admin_gid = $dbh->bz_last_key('groups', 'id');
        }
        # Find current admins
        my @admins;
        # Don't lose admins from DBs where Bug 157704 applies
        $sth = $dbh->prepare(
               "SELECT userid, (groupset & 65536), login_name " .
                 "FROM profiles " .
                "WHERE (groupset | 65536) = 9223372036854775807");
        $sth->execute();
        while ( my ($userid, $iscomplete, $login_name) 
                    = $sth->fetchrow_array() ) 
        {
            # existing administrators are made members of group "admin"
            print "\nWARNING - $login_name IS AN ADMIN IN SPITE OF BUG",
                  " 157704\n\n" if (!$iscomplete);
            push(@admins, $userid) unless grep($_ eq $userid, @admins);
        }
        # Now make all those users admins directly. They were already
        # added to every other group, above, because of their groupset.
        foreach my $admin_id (@admins) {
            $dbh->do("INSERT INTO user_group_map
                                  (user_id, group_id, isbless, grant_type)
                           VALUES (?, ?, ?, ?)", 
                      undef, $admin_id, $admin_gid, $_, GRANT_DIRECT)
                foreach (0, 1);
        }

        $dbh->bz_drop_column('profiles','groupset');
        $dbh->bz_drop_column('profiles','blessgroupset');
        $dbh->bz_drop_column('bugs','groupset');
        $dbh->bz_drop_column('groups','bit');
        $dbh->do("DELETE FROM fielddefs WHERE name = " 
            . $dbh->quote('groupset'));
    }
}

sub _convert_attachment_statuses_to_flags {
    my $dbh = Bugzilla->dbh;

    # September 2002 myk@mozilla.org bug 98801
    # Convert the attachment statuses tables into flags tables.
    if ($dbh->bz_table_info("attachstatuses") 
        && $dbh->bz_table_info("attachstatusdefs")) 
    {
        print "Converting attachment statuses to flags...\n";

        # Get IDs for the old attachment status and new flag fields.
        my ($old_field_id) = $dbh->selectrow_array(
            "SELECT id FROM fielddefs WHERE name='attachstatusdefs.name'")
            || 0;
        my ($new_field_id) = $dbh->selectrow_array(
            "SELECT id FROM fielddefs WHERE name = 'flagtypes.name'");

        # Convert attachment status definitions to flag types.  If more than one
        # status has the same name and description, it is merged into a single
        # status with multiple inclusion records.     

        my $sth = $dbh->prepare(
            "SELECT id, name, description, sortkey, product_id  
               FROM attachstatusdefs");

        # status definition IDs indexed by name/description
        my $def_ids = {};

        # merged IDs and the IDs they were merged into.  The key is the old ID,
        # the value is the new one.  This allows us to give statuses the right
        # ID when we convert them over to flags.  This map includes IDs that
        # weren't merged (in this case the old and new IDs are the same), since
        # it makes the code simpler.
        my $def_id_map = {};

        $sth->execute();
        while (my ($id, $name, $desc, $sortkey, $prod_id) = 
                   $sth->fetchrow_array()) 
        {
            my $key = $name . $desc;
            if (!$def_ids->{$key}) {
                $def_ids->{$key} = $id;
                my $quoted_name = $dbh->quote($name);
                my $quoted_desc = $dbh->quote($desc);
                $dbh->do("INSERT INTO flagtypes (id, name, description, 
                                                 sortkey, target_type) 
                          VALUES ($id, $quoted_name, $quoted_desc, 
                                  $sortkey,'a')");
            }
            $def_id_map->{$id} = $def_ids->{$key};
            $dbh->do("INSERT INTO flaginclusions (type_id, product_id)
                      VALUES ($def_id_map->{$id}, $prod_id)");
        }

        # Note: even though we've converted status definitions, we still
        # can't drop the table because we need it to convert the statuses 
        # themselves.

        # Convert attachment statuses to flags.  To do this we select 
        # the statuses from the status table and then, for each one, 
        # figure out who set it and when they set it from the bugs 
        # activity table.
        my $id = 0;
        $sth = $dbh->prepare(
            "SELECT attachstatuses.attach_id, attachstatusdefs.id,
                    attachstatusdefs.name, attachments.bug_id
               FROM attachstatuses, attachstatusdefs, attachments
              WHERE attachstatuses.statusid = attachstatusdefs.id
                    AND attachstatuses.attach_id = attachments.attach_id");

        # a query to determine when the attachment status was set and who set it
        my $sth2 = $dbh->prepare("SELECT added, who, bug_when
                                    FROM bugs_activity
                                   WHERE bug_id = ? AND attach_id = ?
                                         AND fieldid = $old_field_id
                                ORDER BY bug_when DESC");

        $sth->execute();
        while (my ($attach_id, $def_id, $status, $bug_id) = 
                   $sth->fetchrow_array()) 
        {
            ++$id;

            # Determine when the attachment status was set and who set it.
            # We should always be able to find out this info from the bug
            # activity, but we fall back to default values just in case.
            $sth2->execute($bug_id, $attach_id);
            my ($added, $who, $when);
            while (($added, $who, $when) = $sth2->fetchrow_array()) {
                last if $added =~ /(^|[, ]+)\Q$status\E([, ]+|$)/;
            }
            $who = $dbh->quote($who); # "NULL" by default if $who is undefined
            $when = $when ? $dbh->quote($when) : "NOW()";


            $dbh->do("INSERT INTO flags (id, type_id, status, bug_id, 
                      attach_id, creation_date, modification_date, 
                      requestee_id, setter_id)
                      VALUES ($id, $def_id_map->{$def_id}, '+', $bug_id,
                              $attach_id, $when, $when, NULL, $who)");
        }

        # Now that we've converted both tables we can drop them.
        $dbh->bz_drop_table("attachstatuses");
        $dbh->bz_drop_table("attachstatusdefs");

        # Convert activity records for attachment statuses into records 
        # for flags.
        $sth = $dbh->prepare("SELECT attach_id, who, bug_when, added, 
                                     removed 
                                FROM bugs_activity 
                               WHERE fieldid = $old_field_id");
        $sth->execute();
        while (my ($attach_id, $who, $when, $old_added, $old_removed) =
                   $sth->fetchrow_array())
        {
            my @additions = split(/[, ]+/, $old_added);
            @additions = map("$_+", @additions);
            my $new_added = $dbh->quote(join(", ", @additions));

            my @removals = split(/[, ]+/, $old_removed);
            @removals = map("$_+", @removals);
            my $new_removed = $dbh->quote(join(", ", @removals));

            $old_added = $dbh->quote($old_added);
            $old_removed = $dbh->quote($old_removed);
            $who = $dbh->quote($who);
            $when = $dbh->quote($when);

            $dbh->do("UPDATE bugs_activity SET fieldid = $new_field_id, " .
                     "added = $new_added, removed = $new_removed " .
                     "WHERE attach_id = $attach_id AND who = $who " .
                     "AND bug_when = $when AND fieldid = $old_field_id " .
                     "AND added = $old_added AND removed = $old_removed");
        }

        # Remove the attachment status field from the field definitions.
        $dbh->do("DELETE FROM fielddefs WHERE name='attachstatusdefs.name'");

        print "done.\n";
    }
}

sub _remove_spaces_and_commas_from_flagtypes {
    my $dbh = Bugzilla->dbh;
    # Get all names and IDs, to find broken ones and to
    # check for collisions when renaming.
    my $sth = $dbh->prepare("SELECT name, id FROM flagtypes");
    $sth->execute();

    my %flagtypes;
    my @badflagnames; 
    # find broken flagtype names, and populate a hash table
    # to check for collisions.
    while (my ($name, $id) = $sth->fetchrow_array()) {
        $flagtypes{$name} = $id;
        if ($name =~ /[ ,]/) {
            push(@badflagnames, $name);
        }
    }
    if (@badflagnames) {
        print "Removing spaces and commas from flag names...\n";
        my ($flagname, $tryflagname);
        my $sth = $dbh->prepare("UPDATE flagtypes SET name = ? WHERE id = ?");
        foreach $flagname (@badflagnames) {
            print "  Bad flag type name \"$flagname\" ...\n";
            # find a new name for this flagtype.
            ($tryflagname = $flagname) =~ tr/ ,/__/;
            # avoid collisions with existing flagtype names.
            while (defined($flagtypes{$tryflagname})) {
                print "  ... can't rename as \"$tryflagname\" ...\n";
                $tryflagname .= "'";
                if (length($tryflagname) > 50) {
                    my $lastchanceflagname = (substr $tryflagname, 0, 47) . '...';
                    if (defined($flagtypes{$lastchanceflagname})) {
                        print "  ... last attempt as \"$lastchanceflagname\" still failed.'\n",
                              "Rename the flag by hand and run checksetup.pl again.\n";
                        die("Bad flag type name $flagname");
                    }
                    $tryflagname = $lastchanceflagname;
                }
            }
            $sth->execute($tryflagname, $flagtypes{$flagname});
            print "  renamed flag type \"$flagname\" as \"$tryflagname\"\n";
            $flagtypes{$tryflagname} = $flagtypes{$flagname};
            delete $flagtypes{$flagname};
        }
        print "... done.\n";
    }
}

sub _setup_usebuggroups_backward_compatibility {
    my $dbh = Bugzilla->dbh;
    # 2002-11-24 - bugreport@peshkin.net - bug 147275
    #
    # If group_control_map is empty, backward-compatibility
    # usebuggroups-equivalent records should be created.
    my $entry = Bugzilla->params->{'useentrygroupdefault'};
    my ($maps_exist) = $dbh->selectrow_array(
        "SELECT DISTINCT 1 FROM group_control_map");
    if (!$maps_exist) {
        # Initially populate group_control_map.
        # First, get all the existing products and their groups.
        my $sth = $dbh->prepare("SELECT groups.id, products.id, groups.name,
                                        products.name 
                                  FROM groups, products
                                 WHERE isbuggroup != 0");
        $sth->execute();
        while (my ($groupid, $productid, $groupname, $productname)
                = $sth->fetchrow_array()) 
        {
            if ($groupname eq $productname) {
                # Product and group have same name.
                $dbh->do("INSERT INTO group_control_map " .
                         "(group_id, product_id, entry, membercontrol, " .
                         "othercontrol, canedit) " .
                         "VALUES ($groupid, $productid, $entry, " .
                         CONTROLMAPDEFAULT . ", " .
                         CONTROLMAPNA . ", 0)");
            } else {
                # See if this group is a product group at all.
                my $sth2 = $dbh->prepare("SELECT id FROM products 
                    WHERE name = " .$dbh->quote($groupname));
                $sth2->execute();
                my ($id) = $sth2->fetchrow_array();
                if (!$id) {
                    # If there is no product with the same name as this
                    # group, then it is permitted for all products.
                    $dbh->do("INSERT INTO group_control_map " .
                             "(group_id, product_id, entry, membercontrol, " .
                             "othercontrol, canedit) " .
                             "VALUES ($groupid, $productid, 0, " .
                             CONTROLMAPSHOWN . ", " .
                             CONTROLMAPNA . ", 0)");
                }
            }
        }
    }
}

sub _remove_user_series_map {
    my $dbh = Bugzilla->dbh;
    # 2004-07-17 GRM - Remove "subscriptions" concept from charting, and add
    # group-based security instead.
    if ($dbh->bz_table_info("user_series_map")) {
        # Oracle doesn't like "date" as a column name, and apparently some DBs
        # don't like 'value' either. We use the changes to subscriptions as
        # something to hang these renamings off.
        $dbh->bz_rename_column('series_data', 'date', 'series_date');
        $dbh->bz_rename_column('series_data', 'value', 'series_value');

        # series_categories.category_id produces a too-long column name for the
        # auto-incrementing sequence (Oracle again).
        $dbh->bz_rename_column('series_categories', 'category_id', 'id');

        $dbh->bz_add_column("series", "public",
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'});

        # Migrate public-ness across from user_series_map to new field
        my $sth = $dbh->prepare("SELECT series_id from user_series_map " .
                                 "WHERE user_id = 0");
        $sth->execute();
        while (my ($public_series_id) = $sth->fetchrow_array()) {
            $dbh->do("UPDATE series SET public = 1 " .
                     "WHERE series_id = $public_series_id");
        }

        $dbh->bz_drop_table("user_series_map");
    }
}

sub _copy_old_charts_into_database {
    my $dbh = Bugzilla->dbh;
    my $datadir = bz_locations()->{'datadir'};
    # 2003-06-26 Copy the old charting data into the database, and create the
    # queries that will keep it all running. When the old charting system goes
    # away, if this code ever runs, it'll just find no files and do nothing.
    my $series_exists = $dbh->selectrow_array("SELECT 1 FROM series " .
                                              $dbh->sql_limit(1));
    if (!$series_exists && -d "$datadir/mining" && -e "$datadir/mining/-All-") {
        print "Migrating old chart data into database...\n";

        # We prepare the handle to insert the series data
        my $seriesdatasth = $dbh->prepare(
            "INSERT INTO series_data (series_id, series_date, series_value)
                  VALUES (?, ?, ?)");

        my $deletesth = $dbh->prepare(
            "DELETE FROM series_data WHERE series_id = ? AND series_date = ?");

        my $groupmapsth = $dbh->prepare(
            "INSERT INTO category_group_map (category_id, group_id)
                  VALUES (?, ?)");

        # Fields in the data file (matches the current collectstats.pl)
        my @statuses =
            qw(NEW ASSIGNED REOPENED UNCONFIRMED RESOLVED VERIFIED CLOSED);
        my @resolutions =
            qw(FIXED INVALID WONTFIX LATER REMIND DUPLICATE WORKSFORME MOVED);
        my @fields = (@statuses, @resolutions);

        # We have a localisation problem here. Where do we get these values?
        my $all_name = "-All-";
        my $open_name = "All Open";

        my $products = $dbh->selectall_arrayref("SELECT name FROM products");

        foreach my $product ((map { $_->[0] } @$products), "-All-") {
            # First, create the series
            my %queries;
            my %seriesids;

            my $query_prod = "";
            if ($product ne "-All-") {
                $query_prod = "product=" . html_quote($product) . "&";
            }

            # The query for statuses is different to that for resolutions.
            $queries{$_} = ($query_prod . "bug_status=$_") foreach (@statuses);
            $queries{$_} = ($query_prod . "resolution=$_") 
                foreach (@resolutions);

            foreach my $field (@fields) {
                # Create a Series for each field in this product.
                # user ID = 0 is used.
                my $series = new Bugzilla::Series(undef, $product, $all_name,
                                                  $field, 0, 1,
                                                  $queries{$field}, 1);
                $series->writeToDatabase();
                $seriesids{$field} = $series->{'series_id'};
            }

            # We also add a new query for "Open", so that migrated products get
            # the same set as new products (see editproducts.cgi.)
            my @openedstatuses = ("UNCONFIRMED", "NEW", "ASSIGNED", "REOPENED");
            my $query = join("&", map { "bug_status=$_" } @openedstatuses);
            my $series = new Bugzilla::Series(undef, $product, $all_name,
                                              $open_name, 0, 1,
                                              $query_prod . $query, 1);
            $series->writeToDatabase();
            $seriesids{$open_name} = $series->{'series_id'};

            # Now, we attempt to read in historical data, if any
            # Convert the name in the same way that collectstats.pl does
            my $product_file = $product;
            $product_file =~ s/\//-/gs;
            $product_file = "$datadir/mining/$product_file";

            # There are many reasons that this might fail (e.g. no stats 
            # for this product), so we don't worry if it does.
            my $in = new IO::File($product_file) or next;

            # The data files should be in a standard format, even for old
            # Bugzillas, because of the conversion code further up this file.
            my %data;
            my $last_date = "";

            while (<$in>) {
                if (/^(\d+\|.*)/) {
                    my @numbers = split(/\||\r/, $1);

                    # Only take the first line for each date; it was possible to
                    # run collectstats.pl more than once in a day.
                    next if $numbers[0] eq $last_date;

                    for my $i (0 .. $#fields) {
                        # $numbers[0] is the date
                        $data{$fields[$i]}{$numbers[0]} = $numbers[$i + 1];

                        # Keep a total of the number of open bugs for this day
                        if (is_open_state($fields[$i])) {
                            $data{$open_name}{$numbers[0]} += $numbers[$i + 1];
                        }
                    }

                    $last_date = $numbers[0];
                }
            }

            $in->close;

            foreach my $field (@fields, $open_name) {
                # Insert values into series_data: series_id, date, value
                my %fielddata = %{$data{$field}};
                foreach my $date (keys %fielddata) {
                    # We need to delete in case the text file had duplicate 
                    # entries in it.
                    $deletesth->execute($seriesids{$field}, $date);

                    # We prepared this above
                    $seriesdatasth->execute($seriesids{$field},
                                            $date, $fielddata{$date} || 0);
                }
            }

            # Create the groupsets for the category
            my $category_id =
                $dbh->selectrow_array("SELECT id FROM series_categories " .
                                   "WHERE name = " . $dbh->quote($product));
            my $product_id =
                $dbh->selectrow_array("SELECT id FROM products " .
                                   "WHERE name = " . $dbh->quote($product));

            if (defined($category_id) && defined($product_id)) {

                # Get all the mandatory groups for this product
                my $group_ids =
                    $dbh->selectcol_arrayref("SELECT group_id " .
                         "FROM group_control_map " .
                         "WHERE product_id = $product_id " .
                         "AND (membercontrol = " . CONTROLMAPMANDATORY .
                           " OR othercontrol = " . CONTROLMAPMANDATORY . ")");

                foreach my $group_id (@$group_ids) {
                    $groupmapsth->execute($category_id, $group_id);
                }
            }
        }
    }
}

sub _add_user_group_map_grant_type {
    my $dbh = Bugzilla->dbh;
    # 2004-04-12 - Keep regexp-based group permissions up-to-date - Bug 240325
    if ($dbh->bz_column_info("user_group_map", "isderived")) {
        $dbh->bz_add_column('user_group_map', 'grant_type',
            {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
        $dbh->do("DELETE FROM user_group_map WHERE isderived != 0");
        $dbh->do("UPDATE user_group_map SET grant_type = " . GRANT_DIRECT);
        $dbh->bz_drop_column("user_group_map", "isderived");

        $dbh->bz_drop_index('user_group_map', 'user_group_map_user_id_idx');
        $dbh->bz_add_index('user_group_map', 'user_group_map_user_id_idx',
            {TYPE => 'UNIQUE',
             FIELDS => [qw(user_id group_id grant_type isbless)]});

        # Make sure groups get rederived
        $dbh->do("UPDATE groups SET last_changed = NOW() WHERE name = 'admin'");
    }
}

sub _add_group_group_map_grant_type {
    my $dbh = Bugzilla->dbh;
    # 2004-07-16 - Make it possible to have group-group relationships other than
    # membership and bless.
    if ($dbh->bz_column_info("group_group_map", "isbless")) {
        $dbh->bz_add_column('group_group_map', 'grant_type',
            {TYPE => 'INT1', NOTNULL => 1, DEFAULT => '0'});
        $dbh->do("UPDATE group_group_map SET grant_type = " .
                 "IF(isbless, " . GROUP_BLESS . ", " .
                  GROUP_MEMBERSHIP . ")");
        $dbh->bz_drop_index('group_group_map', 'group_group_map_member_id_idx');
        $dbh->bz_drop_column("group_group_map", "isbless");
        $dbh->bz_add_index('group_group_map', 'group_group_map_member_id_idx',
            {TYPE => 'UNIQUE', 
             FIELDS => [qw(member_id grantor_id grant_type)]});
    }
}

sub _add_longdescs_already_wrapped {
    my $dbh = Bugzilla->dbh;
    # 2005-01-29 - mkanat@bugzilla.org
    if (!$dbh->bz_column_info('longdescs', 'already_wrapped')) {
        # Old, pre-wrapped comments should not be auto-wrapped
        $dbh->bz_add_column('longdescs', 'already_wrapped',
            {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 'FALSE'}, 1);
        # If an old comment doesn't have a newline in the first 81 characters,
        # (or doesn't contain a newline at all) and it contains a space,
        # then it's probably a mis-wrapped comment and we should wrap it
        # at display-time.
        print "Fixing old, mis-wrapped comments...\n";
        $dbh->do(q{UPDATE longdescs SET already_wrapped = 0
                    WHERE (} . $dbh->sql_position(q{'\n'}, 'thetext') . q{ > 81
                       OR } . $dbh->sql_position(q{'\n'}, 'thetext') . q{ = 0)
                      AND SUBSTRING(thetext FROM 1 FOR 80) LIKE '% %'});
    }
}

sub _convert_attachments_filename_from_mediumtext {
    my $dbh = Bugzilla->dbh;
    # 2002 November, myk@mozilla.org, bug 178841:
    #
    # Convert the "attachments.filename" column from a ridiculously large
    # "mediumtext" to a much more sensible "varchar(100)".  Also takes
    # the opportunity to remove paths from existing filenames, since they
    # shouldn't be there for security.  Buggy browsers include them,
    # and attachment.cgi now takes them out, but old ones need converting.
    my $ref = $dbh->bz_column_info("attachments", "filename");
    if ($ref->{TYPE} ne 'varchar(100)') {
        print "Removing paths from filenames in attachments table...\n";

        my $sth = $dbh->prepare("SELECT attach_id, filename FROM attachments " .
            "WHERE " . $dbh->sql_position(q{'/'}, 'filename') . " > 0 OR " .
            $dbh->sql_position(q{'\\\\'}, 'filename') . " > 0");
        $sth->execute;

        while (my ($attach_id, $filename) = $sth->fetchrow_array) {
            $filename =~ s/^.*[\/\\]//;
            my $quoted_filename = $dbh->quote($filename);
            $dbh->do("UPDATE attachments SET filename = $quoted_filename " .
                     "WHERE attach_id = $attach_id");
        }

        print "Done.\n";

        print "Resizing attachments.filename from mediumtext to",
              " varchar(100).\n";
        $dbh->bz_alter_column("attachments", "filename",
                              {TYPE => 'varchar(100)', NOTNULL => 1});
    }
}

sub _rename_votes_count_and_force_group_refresh {
    my $dbh = Bugzilla->dbh;
    # 2003-04-27 - bugzilla@chimpychompy.org (GavinS)
    #
    # Bug 180086 (http://bugzilla.mozilla.org/show_bug.cgi?id=180086)
    #
    # Renaming the 'count' column in the votes table because Sybase doesn't
    # like it
    if ($dbh->bz_column_info('votes', 'count')) {
        # 2003-04-24 - myk@mozilla.org/bbaetz@acm.org, bug 201018
        # Force all cached groups to be updated at login, due to security bug
        # Do this here, inside the next schema change block, so that it doesn't
        # get invalidated on every checksetup run.
        $dbh->do("UPDATE profiles SET refreshed_when='1900-01-01 00:00:00'");

        $dbh->bz_rename_column('votes', 'count', 'vote_count');
    }
}

sub _fix_group_with_empty_name {
    my $dbh = Bugzilla->dbh;
    # 2005-01-12 Nick Barnes <nb@ravenbrook.com> bug 278010
    # Rename any group which has an empty name.
    # Note that there can be at most one such group (because of
    # the SQL index on the name column).
    my ($emptygroupid) = $dbh->selectrow_array(
        "SELECT id FROM groups where name = ''");
    if ($emptygroupid) {
        # There is a group with an empty name; find a name to rename it
        # as.  Must avoid collisions with existing names.  Start with
        # group_$gid and add _<n> if necessary.
        my $trycount = 0;
        my $trygroupname;
        my $trygroupsth = $dbh->prepare("SELECT id FROM groups where name = ?");
        do {
            $trygroupname = "group_$emptygroupid";
            if ($trycount > 0) {
               $trygroupname .= "_$trycount";
            }
            $trygroupsth->execute($trygroupname);
            if ($trygroupsth->rows > 0) {
                $trycount ++;
            }
        } while ($trygroupsth->rows > 0);
        my $sth = $dbh->prepare("UPDATE groups SET name = ? " .
                                 "WHERE id = $emptygroupid");
        $sth->execute($trygroupname);
        print "Group $emptygroupid had an empty name; renamed as",
              " '$trygroupname'.\n";
    }
}

# A helper for the emailprefs subs below
sub _clone_email_event {
    my ($source, $target) = @_;
    my $dbh = Bugzilla->dbh;

    my $sth1 = $dbh->prepare("SELECT user_id, relationship FROM email_setting
                              WHERE event = $source");
    my $sth2 = $dbh->prepare("INSERT into email_setting " .
                             "(user_id, relationship, event) VALUES (" .
                             "?, ?, $target)");

    $sth1->execute();

    while (my ($userid, $relationship) = $sth1->fetchrow_array()) {
        $sth2->execute($userid, $relationship);
    }
}

sub _migrate_email_prefs_to_new_table {
    my $dbh = Bugzilla->dbh;
    # 2005-03-29 - gerv@gerv.net - bug 73665.
    # Migrate email preferences to new email prefs table.
    if ($dbh->bz_column_info("profiles", "emailflags")) {
        print "Migrating email preferences to new table...\n";

        # These are the "roles" and "reasons" from the original code, mapped to
        # the new terminology of relationships and events.
        my %relationships = ("Owner"     => REL_ASSIGNEE,
                             "Reporter"  => REL_REPORTER,
                             "QAcontact" => REL_QA,
                             "CClist"    => REL_CC,
                             "Voter"     => REL_VOTER);

        my %events = ("Removeme"    => EVT_ADDED_REMOVED,
                      "Comments"    => EVT_COMMENT,
                      "Attachments" => EVT_ATTACHMENT,
                      "Status"      => EVT_PROJ_MANAGEMENT,
                      "Resolved"    => EVT_OPENED_CLOSED,
                      "Keywords"    => EVT_KEYWORD,
                      "CC"          => EVT_CC,
                      "Other"       => EVT_OTHER,
                      "Unconfirmed" => EVT_UNCONFIRMED);

        # Request preferences
        my %requestprefs = ("FlagRequestee" => EVT_FLAG_REQUESTED,
                            "FlagRequester" => EVT_REQUESTED_FLAG);

        # Select all emailflags flag strings
        my $sth = $dbh->prepare("SELECT userid, emailflags FROM profiles");
        $sth->execute();
        my $i = 0;
        my $total = $sth->rows;

        while (my ($userid, $flagstring) = $sth->fetchrow_array()) {
            $i++;
            indicate_progress({ total => $total, current => $i, every => 10 });
            # If the user has never logged in since emailprefs arrived, and the
            # temporary code to give them a default string never ran, then
            # $flagstring will be null. In this case, they just get all mail.
            $flagstring ||= "";

            # The 255 param is here, because without a third param, split will
            # trim any trailing null fields, which causes Perl to eject lots of
            # warnings. Any suitably large number would do.
            my %emailflags = split(/~/, $flagstring, 255);

            my $sth2 = $dbh->prepare("INSERT into email_setting " .
                                     "(user_id, relationship, event) VALUES (" .
                                     "$userid, ?, ?)");
            foreach my $relationship (keys %relationships) {
                foreach my $event (keys %events) {
                    my $key = "email$relationship$event";
                    if (!exists($emailflags{$key}) 
                        || $emailflags{$key} eq 'on') 
                    {
                        $sth2->execute($relationships{$relationship},
                                       $events{$event});
                    }
                }
            }
            # Note that in the old system, the value of "excludeself" is 
            # assumed to be off if the preference does not exist in the 
            # user's list, unlike other preferences whose value is 
            # assumed to be on if they do not exist.
            #
            # This preference has changed from global to per-relationship.
            if (!exists($emailflags{'ExcludeSelf'})
                || $emailflags{'ExcludeSelf'} ne 'on')
            {
                foreach my $relationship (keys %relationships) {
                    $dbh->do("INSERT into email_setting " .
                             "(user_id, relationship, event) VALUES (" .
                             $userid . ", " .
                             $relationships{$relationship}. ", " .
                             EVT_CHANGED_BY_ME . ")");
                }
            }

            foreach my $key (keys %requestprefs) {
                if (!exists($emailflags{$key}) || $emailflags{$key} eq 'on') {
                  $dbh->do("INSERT into email_setting " .
                           "(user_id, relationship, event) VALUES (" .
                           $userid . ", " . REL_ANY . ", " .
                           $requestprefs{$key} . ")");
                }
            }
        }
        print "\n";

        # EVT_ATTACHMENT_DATA should initially have identical settings to
        # EVT_ATTACHMENT.
        _clone_email_event(EVT_ATTACHMENT, EVT_ATTACHMENT_DATA);

        $dbh->bz_drop_column("profiles", "emailflags");
    }
}

sub _initialize_dependency_tree_changes_email_pref {
    my $dbh = Bugzilla->dbh;
    # Check for any "new" email settings that wouldn't have been ported over
    # during the block above.  Since these settings would have otherwise
    # fallen under EVT_OTHER, we'll just clone those settings.  That way if
    # folks have already disabled all of that mail, there won't be any change.
    my %events = ("Dependency Tree Changes" => EVT_DEPEND_BLOCK);

    foreach my $desc (keys %events) {
        my $event = $events{$desc};
        my $sth = $dbh->prepare("SELECT COUNT(*) FROM email_setting 
                                  WHERE event = $event");
        $sth->execute();
        if (!($sth->fetchrow_arrayref()->[0])) {
            # No settings in the table yet, so we assume that this is the
            # first time it's being set.
            print "Initializing \"$desc\" email_setting ...\n";
            _clone_email_event(EVT_OTHER, $event);
        }
    }
}

sub _change_all_mysql_booleans_to_tinyint {
    my $dbh = Bugzilla->dbh;
    # 2005-03-27: Standardize all boolean fields to plain "tinyint"
    if ( $dbh->isa('Bugzilla::DB::Mysql') ) {
        # This is a change to make things consistent with Schema, so we use
        # direct-database access methods.
        my $quip_info_sth = $dbh->column_info(undef, undef, 'quips', '%');
        my $quips_cols    = $quip_info_sth->fetchall_hashref("COLUMN_NAME");
        my $approved_col  = $quips_cols->{'approved'};
        if ( $approved_col->{TYPE_NAME} eq 'TINYINT'
             and $approved_col->{COLUMN_SIZE} == 1 )
        {
            # series.public could have been renamed to series.is_public,
            # and so wouldn't need to be fixed manually.
            if ($dbh->bz_column_info('series', 'public')) {
                $dbh->bz_alter_column_raw('series', 'public',
                    {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '0'});
            }
            $dbh->bz_alter_column_raw('bug_status', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('rep_platform', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('resolution', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('op_sys', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('bug_severity', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('priority', 'isactive',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
            $dbh->bz_alter_column_raw('quips', 'approved',
                {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'});
        }
   }
}

1;

__END__

=head1 NAME

Bugzilla::Install::DB - Fix up the database during installation.

=head1 SYNOPSIS

=head1 DESCRIPTION

This module is used primarily by L<checksetup.pl> to modify the 
database during upgrades.

=over

=back

=head1 SUBROUTINES

=over

=back
