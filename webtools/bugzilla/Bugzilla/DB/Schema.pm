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
# Contributor(s): Andrew Dunstan <andrew@dunslane.net>,
#                 Edward J. Sabol <edwardjsabol@iname.com>

package Bugzilla::DB::Schema;

###########################################################################
#
# Purpose: Object-oriented, DBMS-independent database schema for Bugzilla
#
# This is the base class implementing common methods and abstract schema.
#
###########################################################################

use strict;
use Bugzilla::Error;
use Bugzilla::Util;

use Storable qw(dclone freeze thaw);

=head1 NAME

Bugzilla::DB::Schema - Abstract database schema for Bugzilla

=head1 SYNOPSIS

  # Obtain MySQL database schema.
  # Do not do this. Use Bugzilla::DB instead.
  use Bugzilla::DB::Schema;
  my $mysql_schema = new Bugzilla::DB::Schema('Mysql');

  # Recommended way to obtain database schema.
  use Bugzilla::DB;
  my $dbh = Bugzilla->dbh;
  my $schema = $dbh->_bz_schema();

  # Get the list of tables in the Bugzilla database.
  my @tables = $schema->get_table_list();

  # Get the SQL statements need to create the bugs table.
  my @statements = $schema->get_table_ddl('bugs');

  # Get the database-specific SQL data type used to implement
  # the abstract data type INT1.
  my $db_specific_type = $schema->sql_type('INT1');

=head1 DESCRIPTION

This module implements an object-oriented, abstract database schema.
It should be considered package-private to the Bugzilla::DB module.
That means that CGI scripts should never call any function in this
module directly, but should instead rely on methods provided by
Bugzilla::DB.

=cut
#--------------------------------------------------------------------------
# Define the Bugzilla abstract database schema and version as constants.

=head1 CONSTANTS

=over 4

=item C<SCHEMA_VERSION>

The 'version' of the internal schema structure. This version number
is incremented every time the the fundamental structure of Schema
internals changes.

This is NOT changed every time a table or a column is added. This 
number is incremented only if the internal structures of this 
Schema would be incompatible with the internal structures of a 
previous Schema version.

In general, unless you are messing around with serialization
and deserialization of the schema, you don't need to worry about
this constant.

=begin private

An example of the use of the version number:

Today, we store all individual columns like this:

column_name => { TYPE => 'SOMETYPE', NOTNULL => 1 }

Imagine that someday we decide that NOTNULL => 1 is bad, and we want
to change it so that the schema instead uses NULL => 0.

But we have a bunch of Bugzilla installations around the world with a
serialized schema that has NOTNULL in it! When we deserialize that 
structure, it just WILL NOT WORK properly inside of our new Schema object.
So, immediately after deserializing, we need to go through the hash 
and change all NOTNULLs to NULLs and so on.

We know that we need to do that on deserializing because we know that
version 1.00 used NOTNULL. Having made the change to NULL, we would now
be version 1.01.

=end private

=item C<ABSTRACT_SCHEMA>

The abstract database schema structure consists of a hash reference
in which each key is the name of a table in the Bugzilla database.

The value for each key is a hash reference containing the keys
C<FIELDS> and C<INDEXES> which in turn point to array references
containing information on the table's fields and indexes. 

A field hash reference should must contain the key C<TYPE>. Optional field
keys include C<PRIMARYKEY>, C<NOTNULL>, and C<DEFAULT>. 

The C<INDEXES> array reference contains index names and information 
regarding the index. If the index name points to an array reference,
then the index is a regular index and the array contains the indexed
columns. If the index name points to a hash reference, then the hash
must contain the key C<FIELDS>. It may also contain the key C<TYPE>,
which can be used to specify the type of index such as UNIQUE or FULLTEXT.

=back

=cut

use constant SCHEMA_VERSION  => '1.00';
use constant ABSTRACT_SCHEMA => {

    # BUG-RELATED TABLES
    # ------------------

    # General Bug Information
    # -----------------------
    bugs => {
        FIELDS => [
            bug_id              => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                                    PRIMARYKEY => 1},
            assigned_to         => {TYPE => 'INT3', NOTNULL => 1},
            bug_file_loc        => {TYPE => 'TEXT'},
            bug_severity        => {TYPE => 'varchar(64)', NOTNULL => 1},
            bug_status          => {TYPE => 'varchar(64)', NOTNULL => 1},
            creation_ts         => {TYPE => 'DATETIME', NOTNULL => 1},
            delta_ts            => {TYPE => 'DATETIME', NOTNULL => 1},
            short_desc          => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            op_sys              => {TYPE => 'varchar(64)', NOTNULL => 1},
            priority            => {TYPE => 'varchar(64)', NOTNULL => 1},
            product_id          => {TYPE => 'INT2', NOTNULL => 1},
            rep_platform        => {TYPE => 'varchar(64)', NOTNULL => 1},
            reporter            => {TYPE => 'INT3', NOTNULL => 1},
            version             => {TYPE => 'varchar(64)', NOTNULL => 1},
            component_id        => {TYPE => 'INT2', NOTNULL => 1},
            resolution          => {TYPE => 'varchar(64)', NOTNULL => 1},
            target_milestone    => {TYPE => 'varchar(20)',
                                    NOTNULL => 1, DEFAULT => "'---'"},
            qa_contact          => {TYPE => 'INT3'},
            status_whiteboard   => {TYPE => 'MEDIUMTEXT', NOTNULL => 1,
                                    DEFAULT => "''"},
            votes               => {TYPE => 'INT3', NOTNULL => 1,
                                    DEFAULT => '0'},
            # Note: keywords field is only a cache; the real data
            # comes from the keywords table
            keywords            => {TYPE => 'MEDIUMTEXT', NOTNULL => 1,
                                    DEFAULT => "''"},
            lastdiffed          => {TYPE => 'DATETIME'},
            everconfirmed       => {TYPE => 'BOOLEAN', NOTNULL => 1},
            reporter_accessible => {TYPE => 'BOOLEAN',
                                    NOTNULL => 1, DEFAULT => 'TRUE'},
            cclist_accessible   => {TYPE => 'BOOLEAN',
                                    NOTNULL => 1, DEFAULT => 'TRUE'},
            estimated_time      => {TYPE => 'decimal(5,2)',
                                    NOTNULL => 1, DEFAULT => '0'},
            remaining_time      => {TYPE => 'decimal(5,2)',
                                    NOTNULL => 1, DEFAULT => '0'},
            deadline            => {TYPE => 'DATETIME'},
            alias               => {TYPE => 'varchar(20)'},
        ],
        INDEXES => [
            bugs_unique_idx           => {FIELDS => ['alias'],
                                          TYPE => 'UNIQUE'},
            bugs_assigned_to_idx      => ['assigned_to'],
            bugs_creation_ts_idx      => ['creation_ts'],
            bugs_delta_ts_idx         => ['delta_ts'],
            bugs_bug_severity_idx     => ['bug_severity'],
            bugs_bug_status_idx       => ['bug_status'],
            bugs_op_sys_idx           => ['op_sys'],
            bugs_priority_idx         => ['priority'],
            bugs_product_id_idx       => ['product_id'],
            bugs_reporter_idx         => ['reporter'],
            bugs_version_idx          => ['version'],
            bugs_component_id_idx     => ['component_id'],
            bugs_resolution_idx       => ['resolution'],
            bugs_target_milestone_idx => ['target_milestone'],
            bugs_qa_contact_idx       => ['qa_contact'],
            bugs_votes_idx            => ['votes'],
            bugs_short_desc_idx       => {FIELDS => ['short_desc'],
                                          TYPE => 'FULLTEXT'},
        ],
    },

    bugs_activity => {
        FIELDS => [
            bug_id    => {TYPE => 'INT3', NOTNULL => 1},
            attach_id => {TYPE => 'INT3'},
            who       => {TYPE => 'INT3', NOTNULL => 1},
            bug_when  => {TYPE => 'DATETIME', NOTNULL => 1},
            fieldid   => {TYPE => 'INT3', NOTNULL => 1},
            added     => {TYPE => 'TINYTEXT'},
            removed   => {TYPE => 'TINYTEXT'},
        ],
        INDEXES => [
            bugs_activity_bugid_idx   => ['bug_id'],
            bugs_activity_who_idx     => ['who'],
            bugs_activity_bugwhen_idx => ['bug_when'],
            bugs_activity_fieldid_idx => ['fieldid'],
        ],
    },

    cc => {
        FIELDS => [
            bug_id => {TYPE => 'INT3', NOTNULL => 1},
            who    => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            cc_unique_idx => {FIELDS => [qw(bug_id who)],
                              TYPE => 'UNIQUE'},
            cc_who_idx    => ['who'],
        ],
    },

    longdescs => {
        FIELDS => [
            bug_id          => {TYPE => 'INT3',  NOTNULL => 1},
            who             => {TYPE => 'INT3', NOTNULL => 1},
            bug_when        => {TYPE => 'DATETIME', NOTNULL => 1},
            work_time       => {TYPE => 'decimal(5,2)', NOTNULL => 1,
                                DEFAULT => '0'},
            thetext         => {TYPE => 'MEDIUMTEXT'},
            isprivate       => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                DEFAULT => 'FALSE'},
            already_wrapped => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                DEFAULT => 'FALSE'},
        ],
        INDEXES => [
            longdescs_bugid_idx   => ['bug_id'],
            longdescs_who_idx     => ['who'],
            longdescs_bugwhen_idx => ['bug_when'],
            longdescs_thetext_idx => {FIELDS => ['thetext'],
                                      TYPE => 'FULLTEXT'},
        ],
    },

    dependencies => {
        FIELDS => [
            blocked   => {TYPE => 'INT3', NOTNULL => 1},
            dependson => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            dependencies_blocked_idx   => ['blocked'],
            dependencies_dependson_idx => ['dependson'],
        ],
    },

    votes => {
        FIELDS => [
            who        => {TYPE => 'INT3', NOTNULL => 1},
            bug_id     => {TYPE => 'INT3', NOTNULL => 1},
            vote_count => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            votes_who_idx    => ['who'],
            votes_bug_id_idx => ['bug_id'],
        ],
    },

    attachments => {
        FIELDS => [
            attach_id    => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                             PRIMARYKEY => 1},
            bug_id       => {TYPE => 'INT3', NOTNULL => 1},
            creation_ts  => {TYPE => 'DATETIME', NOTNULL => 1},
            description  => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            mimetype     => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            ispatch      => {TYPE => 'BOOLEAN'},
            filename     => {TYPE => 'varchar(100)', NOTNULL => 1},
            thedata      => {TYPE => 'LONGBLOB', NOTNULL => 1},
            submitter_id => {TYPE => 'INT3', NOTNULL => 1},
            isobsolete   => {TYPE => 'BOOLEAN', NOTNULL => 1,
                             DEFAULT => 'FALSE'},
            isprivate    => {TYPE => 'BOOLEAN', NOTNULL => 1,
                             DEFAULT => 'FALSE'},
        ],
        INDEXES => [
            attachments_bug_id_idx => ['bug_id'],
            attachments_creation_ts_idx => ['creation_ts'],
        ],
    },

    duplicates => {
        FIELDS => [
            dupe_of => {TYPE => 'INT3', NOTNULL => 1},
            dupe    => {TYPE => 'INT3', NOTNULL => 1,
                       PRIMARYKEY => 1},
        ],
    },

    # Keywords
    # --------

    keyworddefs => {
        FIELDS => [
            id          => {TYPE => 'INT2', NOTNULL => 1,
                            PRIMARYKEY => 1},
            name        => {TYPE => 'varchar(64)', NOTNULL => 1},
            description => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            keyworddefs_unique_idx => {FIELDS => ['name'],
                                       TYPE => 'UNIQUE'},
        ],
    },

    keywords => {
        FIELDS => [
            bug_id    => {TYPE => 'INT3', NOTNULL => 1},
            keywordid => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            keywords_unique_idx    => {FIELDS => [qw(bug_id keywordid)],
                                       TYPE => 'UNIQUE'},
            keywords_keywordid_idx => ['keywordid'],
        ],
    },

    # Flags
    # -----

    # "flags" stores one record for each flag on each bug/attachment.
    flags => {
        FIELDS => [
            id                => {TYPE => 'INT3', NOTNULL => 1,
                                  PRIMARYKEY => 1},
            type_id           => {TYPE => 'INT2', NOTNULL => 1},
            status            => {TYPE => 'char(1)', NOTNULL => 1},
            bug_id            => {TYPE => 'INT3', NOTNULL => 1},
            attach_id         => {TYPE => 'INT3'},
            creation_date     => {TYPE => 'DATETIME', NOTNULL => 1},
            modification_date => {TYPE => 'DATETIME'},
            setter_id         => {TYPE => 'INT3'},
            requestee_id      => {TYPE => 'INT3'},
            is_active         => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                  DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            flags_bidattid_idx     => [qw(bug_id attach_id)],
            flags_setter_id_idx    => ['setter_id'],
            flags_requestee_id_idx => ['requestee_id'],
        ],
    },

    # "flagtypes" defines the types of flags that can be set.
    flagtypes => {
        FIELDS => [
            id               => {TYPE => 'INT2', NOTNULL => 1,
                                 PRIMARYKEY => 1},
            name             => {TYPE => 'varchar(50)', NOTNULL => 1},
            description      => {TYPE => 'TEXT'},
            cc_list          => {TYPE => 'varchar(200)'},
            target_type      => {TYPE => 'char(1)', NOTNULL => 1,
                                 DEFAULT => "'b'"},
            is_active        => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                 DEFAULT => 'TRUE'},
            is_requestable   => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                 DEFAULT => 'FALSE'},
            is_requesteeble  => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                 DEFAULT => 'FALSE'},
            is_multiplicable => {TYPE => 'BOOLEAN', NOTNULL => 1,
                                 DEFAULT => 'FALSE'},
            sortkey          => {TYPE => 'INT2', NOTNULL => 1,
                                 DEFAULT => '0'},
            grant_group_id   => {TYPE => 'INT3'},
            request_group_id => {TYPE => 'INT3'},
        ],
    },

    # "flaginclusions" and "flagexclusions" specify the products/components
    #     a bug/attachment must belong to in order for flags of a given type
    #     to be set for them.
    flaginclusions => {
        FIELDS => [
            type_id      => {TYPE => 'INT2', NOTNULL => 1},
            product_id   => {TYPE => 'INT2'},
            component_id => {TYPE => 'INT2'},
        ],
        INDEXES => [
            flaginclusions_tpcid_idx =>
                [qw(type_id product_id component_id)],
        ],
    },

    flagexclusions => {
        FIELDS => [
            type_id      => {TYPE => 'INT2', NOTNULL => 1},
            product_id   => {TYPE => 'INT2'},
            component_id => {TYPE => 'INT2'},
        ],
        INDEXES => [
            flagexclusions_tpc_id_idx =>
                [qw(type_id product_id component_id)],
        ],
    },

    # General Field Information
    # -------------------------

    fielddefs => {
        FIELDS => [
            fieldid     => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                            PRIMARYKEY => 1},
            name        => {TYPE => 'varchar(64)', NOTNULL => 1},
            description => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            mailhead    => {TYPE => 'BOOLEAN', NOTNULL => 1,
                            DEFAULT => 'FALSE'},
            sortkey     => {TYPE => 'INT2', NOTNULL => 1},
            obsolete    => {TYPE => 'BOOLEAN', NOTNULL => 1,
                            DEFAULT => 'FALSE'},
        ],
        INDEXES => [
            fielddefs_unique_idx  => {FIELDS => ['name'],
                                      TYPE => 'UNIQUE'},
            fielddefs_sortkey_idx => ['sortkey'],
        ],
    },

    # Per-product Field Values
    # ------------------------

    versions => {
        FIELDS => [
            value      =>  {TYPE => 'TINYTEXT'},
            product_id =>  {TYPE => 'INT2', NOTNULL => 1},
        ],
    },

    milestones => {
        FIELDS => [
            product_id => {TYPE => 'INT2', NOTNULL => 1},
            value      => {TYPE => 'varchar(20)', NOTNULL => 1},
            sortkey    => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            milestones_unique_idx => {FIELDS => [qw(product_id value)],
                                      TYPE => 'UNIQUE'},
        ],
    },

    # Global Field Values
    # -------------------

    bug_status => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            bug_status_unique_idx  => {FIELDS => ['value'],
                                       TYPE => 'UNIQUE'},
            bug_status_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    resolution => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            resolution_unique_idx  => {FIELDS => ['value'],
                                       TYPE => 'UNIQUE'},
            resolution_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    bug_severity => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1, 
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            bug_severity_unique_idx  => {FIELDS => ['value'],
                                         TYPE => 'UNIQUE'},
            bug_severity_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    priority => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            priority_unique_idx  => {FIELDS => ['value'],
                                     TYPE => 'UNIQUE'},
            priority_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    rep_platform => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            rep_platform_unique_idx  => {FIELDS => ['value'],
                                         TYPE => 'UNIQUE'},
            rep_platform_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    op_sys => {
        FIELDS => [
            id       => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            value    => {TYPE => 'varchar(64)', NOTNULL => 1},
            sortkey  => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => 0},
            isactive => {TYPE => 'BOOLEAN', NOTNULL => 1, 
                         DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            op_sys_unique_idx  => {FIELDS => ['value'],
                                   TYPE => 'UNIQUE'},
            op_sys_sortkey_idx => ['sortkey', 'value'],
        ],
    },

    # USER INFO
    # ---------

    # General User Information
    # ------------------------

    profiles => {
        FIELDS => [
            userid         => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                               PRIMARYKEY => 1},
            login_name     => {TYPE => 'varchar(255)', NOTNULL => 1},
            cryptpassword  => {TYPE => 'varchar(128)'},
            realname       => {TYPE => 'varchar(255)'},
            disabledtext   => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            mybugslink     => {TYPE => 'BOOLEAN', NOTNULL => 1,
                               DEFAULT => 'TRUE'},
            emailflags     => {TYPE => 'MEDIUMTEXT'},
            refreshed_when => {TYPE => 'DATETIME', NOTNULL => 1},
            extern_id      => {TYPE => 'varchar(64)'},
        ],
        INDEXES => [
            profiles_unique_idx => {FIELDS => ['login_name'],
                                    TYPE => 'UNIQUE'},
        ],
    },

    profiles_activity => {
        FIELDS => [
            userid        => {TYPE => 'INT3', NOTNULL => 1},
            who           => {TYPE => 'INT3', NOTNULL => 1},
            profiles_when => {TYPE => 'DATETIME', NOTNULL => 1},
            fieldid       => {TYPE => 'INT3', NOTNULL => 1},
            oldvalue      => {TYPE => 'TINYTEXT'},
            newvalue      => {TYPE => 'TINYTEXT'},
        ],
        INDEXES => [
            profiles_activity_userid_idx  => ['userid'],
            profiles_activity_when_idx    => ['profiles_when'],
            profiles_activity_fieldid_idx => ['fieldid'],
        ],
    },

    watch => {
        FIELDS => [
            watcher => {TYPE => 'INT3', NOTNULL => 1},
            watched => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            watch_unique_idx  => {FIELDS => [qw(watcher watched)],
                                  TYPE => 'UNIQUE'},
            watch_watched_idx => ['watched'],
        ],
    },

    namedqueries => {
        FIELDS => [
            userid       => {TYPE => 'INT3', NOTNULL => 1},
            name         => {TYPE => 'varchar(64)', NOTNULL => 1},
            linkinfooter => {TYPE => 'BOOLEAN', NOTNULL => 1},
            query        => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
        ],
        INDEXES => [
            namedqueries_unique_idx => {FIELDS => [qw(userid name)],
                                        TYPE => 'UNIQUE'},
        ],
    },

    # Authentication
    # --------------

    logincookies => {
        FIELDS => [
            cookie   => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            userid   => {TYPE => 'INT3', NOTNULL => 1},
            ipaddr   => {TYPE => 'varchar(40)', NOTNULL => 1},
            lastused => {TYPE => 'DATETIME', NOTNULL => 1},
        ],
        INDEXES => [
            logincookies_lastused_idx => ['lastused'],
        ],
    },

    # "tokens" stores the tokens users receive when a password or email
    #     change is requested.  Tokens provide an extra measure of security
    #     for these changes.
    tokens => {
        FIELDS => [
            userid    => {TYPE => 'INT3', NOTNULL => 1} ,
            issuedate => {TYPE => 'DATETIME', NOTNULL => 1} ,
            token     => {TYPE => 'varchar(16)', NOTNULL => 1,
                          PRIMARYKEY => 1},
            tokentype => {TYPE => 'varchar(8)', NOTNULL => 1} ,
            eventdata => {TYPE => 'TINYTEXT'},
        ],
        INDEXES => [
            tokens_userid_idx => ['userid'],
        ],
    },

    # GROUPS
    # ------

    groups => {
        FIELDS => [
            id           => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                             PRIMARYKEY => 1},
            name         => {TYPE => 'varchar(255)', NOTNULL => 1},
            description  => {TYPE => 'TEXT', NOTNULL => 1},
            isbuggroup   => {TYPE => 'BOOLEAN', NOTNULL => 1},
            last_changed => {TYPE => 'DATETIME', NOTNULL => 1},
            userregexp   => {TYPE => 'TINYTEXT', NOTNULL => 1},
            isactive     => {TYPE => 'BOOLEAN', NOTNULL => 1,
                             DEFAULT => 'TRUE'},
        ],
        INDEXES => [
            groups_unique_idx => {FIELDS => ['name'], TYPE => 'UNIQUE'},
        ],
    },

    group_control_map => {
        FIELDS => [
            group_id      => {TYPE => 'INT3', NOTNULL => 1},
            product_id    => {TYPE => 'INT3', NOTNULL => 1},
            entry         => {TYPE => 'BOOLEAN', NOTNULL => 1},
            membercontrol => {TYPE => 'BOOLEAN', NOTNULL => 1},
            othercontrol  => {TYPE => 'BOOLEAN', NOTNULL => 1},
            canedit       => {TYPE => 'BOOLEAN', NOTNULL => 1},
        ],
        INDEXES => [
            group_control_map_unique_idx =>
            {FIELDS => [qw(product_id group_id)], TYPE => 'UNIQUE'},
            group_control_map_gid_idx    => ['group_id'],
        ],
    },

    # "user_group_map" determines the groups that a user belongs to
    # directly or due to regexp and which groups can be blessed by a user.
    #
    # grant_type:
    # if GRANT_DIRECT - record was explicitly granted
    # if GRANT_DERIVED - record was derived from expanding a group hierarchy
    # if GRANT_REGEXP - record was created by evaluating a regexp
    user_group_map => {
        FIELDS => [
            user_id    => {TYPE => 'INT3', NOTNULL => 1},
            group_id   => {TYPE => 'INT3', NOTNULL => 1},
            isbless    => {TYPE => 'BOOLEAN', NOTNULL => 1,
                           DEFAULT => 'FALSE'},
            grant_type => {TYPE => 'INT1', NOTNULL => 1,
                           DEFAULT => '0'},
        ],
        INDEXES => [
            user_group_map_unique_idx =>
                {FIELDS => [qw(user_id group_id grant_type isbless)],
                 TYPE => 'UNIQUE'},
        ],
    },

    # This table determines which groups are made a member of another
    # group, given the ability to bless another group, or given
    # visibility to another groups existence and membership
    # grant_type:
    # if GROUP_MEMBERSHIP - member groups are made members of grantor
    # if GROUP_BLESS - member groups may grant membership in grantor
    # if GROUP_VISIBLE - member groups may see grantor group
    group_group_map => {
        FIELDS => [
            member_id  => {TYPE => 'INT3', NOTNULL => 1},
            grantor_id => {TYPE => 'INT3', NOTNULL => 1},
            grant_type => {TYPE => 'INT1', NOTNULL => 1,
                           DEFAULT => '0'},
        ],
        INDEXES => [
            group_group_map_unique_idx =>
                {FIELDS => [qw(member_id grantor_id grant_type)],
                 TYPE => 'UNIQUE'},
        ],
    },

    # This table determines which groups a user must be a member of
    # in order to see a bug.
    bug_group_map => {
        FIELDS => [
            bug_id   => {TYPE => 'INT3', NOTNULL => 1},
            group_id => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            bug_group_map_unique_idx   =>
                {FIELDS => [qw(bug_id group_id)], TYPE => 'UNIQUE'},
            bug_group_map_group_id_idx => ['group_id'],
        ],
    },

    category_group_map => {
        FIELDS => [
            category_id => {TYPE => 'INT2', NOTNULL => 1},
            group_id    => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            category_group_map_unique_idx =>
                {FIELDS => [qw(category_id group_id)], TYPE => 'UNIQUE'},
        ],
    },


    # PRODUCTS
    # --------

    classifications => {
        FIELDS => [
            id          => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                            PRIMARYKEY => 1},
            name        => {TYPE => 'varchar(64)', NOTNULL => 1},
            description => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            classifications_unique_idx => {FIELDS => ['name'],
                                           TYPE => 'UNIQUE'},
        ],
    },

    products => {
        FIELDS => [
            id                => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                                  PRIMARYKEY => 1},
            name              => {TYPE => 'varchar(64)', NOTNULL => 1},
            classification_id => {TYPE => 'INT2', NOTNULL => 1,
                                  DEFAULT => '1'},
            description       => {TYPE => 'MEDIUMTEXT'},
            milestoneurl      => {TYPE => 'TINYTEXT', NOTNULL => 1},
            disallownew       => {TYPE => 'BOOLEAN', NOTNULL => 1},
            votesperuser      => {TYPE => 'INT2', NOTNULL => 1},
            maxvotesperbug    => {TYPE => 'INT2', NOTNULL => 1,
                                  DEFAULT => '10000'},
            votestoconfirm    => {TYPE => 'INT2', NOTNULL => 1},
            defaultmilestone  => {TYPE => 'varchar(20)',
                                  NOTNULL => 1, DEFAULT => "'---'"},
        ],
        INDEXES => [
            products_unique_idx => {FIELDS => ['name'],
                                    TYPE => 'UNIQUE'},
        ],
    },

    components => {
        FIELDS => [
            id               => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                                 PRIMARYKEY => 1},
            name             => {TYPE => 'varchar(64)', NOTNULL => 1},
            product_id       => {TYPE => 'INT2', NOTNULL => 1},
            initialowner     => {TYPE => 'INT3'},
            initialqacontact => {TYPE => 'INT3'},
            description      => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
        ],
        INDEXES => [
            components_unique_idx => {FIELDS => [qw(product_id name)],
                                      TYPE => 'UNIQUE'},
            components_name_idx   => ['name'],
        ],
    },

    # CHARTS
    # ------

    series => {
        FIELDS => [
            series_id   => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                            PRIMARYKEY => 1},
            creator     => {TYPE => 'INT3', NOTNULL => 1},
            category    => {TYPE => 'INT2', NOTNULL => 1},
            subcategory => {TYPE => 'INT2', NOTNULL => 1},
            name        => {TYPE => 'varchar(64)', NOTNULL => 1},
            frequency   => {TYPE => 'INT2', NOTNULL => 1},
            last_viewed => {TYPE => 'DATETIME'},
            query       => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            public      => {TYPE => 'BOOLEAN', NOTNULL => 1,
                            DEFAULT => 'FALSE'},
        ],
        INDEXES => [
            series_unique_idx  =>
                {FIELDS => [qw(creator category subcategory name)],
                 TYPE => 'UNIQUE'},
            series_creator_idx => ['creator'],
        ],
    },

    series_data => {
        FIELDS => [
            series_id    => {TYPE => 'INT3', NOTNULL => 1},
            series_date  => {TYPE => 'DATETIME', NOTNULL => 1},
            series_value => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            series_data_unique_idx =>
                {FIELDS => [qw(series_id series_date)],
                 TYPE => 'UNIQUE'},
        ],
    },

    series_categories => {
        FIELDS => [
            id   => {TYPE => 'SMALLSERIAL', NOTNULL => 1,
                     PRIMARYKEY => 1},
            name => {TYPE => 'varchar(64)', NOTNULL => 1},
        ],
        INDEXES => [
            series_cats_unique_idx => {FIELDS => ['name'],
                                       TYPE => 'UNIQUE'},
        ],
    },

    # WHINE SYSTEM
    # ------------

    whine_queries => {
        FIELDS => [
            id            => {TYPE => 'MEDIUMSERIAL', PRIMARYKEY => 1},
            eventid       => {TYPE => 'INT3', NOTNULL => 1},
            query_name    => {TYPE => 'varchar(64)', NOTNULL => 1,
                              DEFAULT => "''"},
            sortkey       => {TYPE => 'INT2', NOTNULL => 1,
                              DEFAULT => '0'},
            onemailperbug => {TYPE => 'BOOLEAN', NOTNULL => 1,
                              DEFAULT => 'FALSE'},
            title         => {TYPE => 'varchar(128)', NOTNULL => 1},
        ],
        INDEXES => [
            whine_queries_eventid_idx => ['eventid'],
        ],
    },

    whine_schedules => {
        FIELDS => [
            id          => {TYPE => 'MEDIUMSERIAL', PRIMARYKEY => 1},
            eventid     => {TYPE => 'INT3', NOTNULL => 1},
            run_day     => {TYPE => 'varchar(32)'},
            run_time    => {TYPE => 'varchar(32)'},
            run_next    => {TYPE => 'DATETIME'},
            mailto      => {TYPE => 'INT3', NOTNULL => 1},
            mailto_type => {TYPE => 'INT2', NOTNULL => 1, DEFAULT => '0'},
        ],
        INDEXES => [
            whine_schedules_run_next_idx => ['run_next'],
            whine_schedules_eventid_idx  => ['eventid'],
        ],
    },

    whine_events => {
        FIELDS => [
            id           => {TYPE => 'MEDIUMSERIAL', PRIMARYKEY => 1},
            owner_userid => {TYPE => 'INT3', NOTNULL => 1},
            subject      => {TYPE => 'varchar(128)'},
            body         => {TYPE => 'MEDIUMTEXT'},
        ],
    },

    # QUIPS
    # -----

    quips => {
        FIELDS => [
            quipid   => {TYPE => 'MEDIUMSERIAL', NOTNULL => 1,
                         PRIMARYKEY => 1},
            userid   => {TYPE => 'INT3'},
            quip     => {TYPE => 'TEXT', NOTNULL => 1},
            approved => {TYPE => 'BOOLEAN', NOTNULL => 1,
                         DEFAULT => 'TRUE'},
        ],
    },

    # SETTINGS
    # --------
    # setting          - each global setting will have exactly one entry
    #                    in this table.
    # setting_value    - stores the list of acceptable values for each
    #                    setting, and a sort index that controls the order
    #                    in which the values are displayed.
    # profile_setting  - If a user has chosen to use a value other than the
    #                    global default for a given setting, it will be
    #                    stored in this table. Note: even if a setting is
    #                    later changed so is_enabled = false, the stored
    #                    value will remain in case it is ever enabled again.
    #
    setting => {
        FIELDS => [
            name          => {TYPE => 'varchar(32)', NOTNULL => 1,
                              PRIMARYKEY => 1}, 
            default_value => {TYPE => 'varchar(32)', NOTNULL => 1},
            is_enabled    => {TYPE => 'BOOLEAN', NOTNULL => 1,
                              DEFAULT => 'TRUE'},
        ],
    },

    setting_value => {
        FIELDS => [
            name        => {TYPE => 'varchar(32)', NOTNULL => 1},
            value       => {TYPE => 'varchar(32)', NOTNULL => 1},
            sortindex   => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            setting_value_nv_unique_idx  => {FIELDS => [qw(name value)],
                                             TYPE => 'UNIQUE'},
            setting_value_ns_unique_idx  => {FIELDS => [qw(name sortindex)],
                                             TYPE => 'UNIQUE'},
        ],
     },

    profile_setting => {
        FIELDS => [
            user_id       => {TYPE => 'INT3', NOTNULL => 1},
            setting_name  => {TYPE => 'varchar(32)', NOTNULL => 1},
            setting_value => {TYPE => 'varchar(32)', NOTNULL => 1},
        ],
        INDEXES => [
            profile_setting_value_unique_idx  => {FIELDS => [qw(user_id setting_name)],
                                                  TYPE => 'UNIQUE'},
        ],
     },

    # SCHEMA STORAGE
    # --------------

    bz_schema => {
        FIELDS => [
            schema_data => {TYPE => 'LONGBLOB', NOTNULL => 1},
            version     => {TYPE => 'decimal(3,2)', NOTNULL => 1},
        ],
    },

};
#--------------------------------------------------------------------------

=head1 METHODS

Note: Methods which can be implemented generically for all DBs are
implemented in this module. If needed, they can be overriden with
DB-specific code in a subclass. Methods which are prefixed with C<_>
are considered protected. Subclasses may override these methods, but
other modules should not invoke these methods directly.

=over 4

=cut

#--------------------------------------------------------------------------
sub new {

=item C<new>

 Description: Public constructor method used to instantiate objects of this
              class. However, it also can be used as a factory method to
              instantiate database-specific subclasses when an optional
              driver argument is supplied.
 Parameters:  $driver (optional) - Used to specify the type of database.
              This routine C<die>s if no subclass is found for the specified
              driver.
              $schema (optional) - A reference to a hash. Callers external
                  to this package should never use this parameter.
 Returns:     new instance of the Schema class or a database-specific subclass

=cut

    my $this = shift;
    my $class = ref($this) || $this;
    my $driver = shift;

    if ($driver) {
        (my $subclass = $driver) =~ s/^(\S)/\U$1/;
        $class .= '::' . $subclass;
        eval "require $class;";
        die "The $class class could not be found ($subclass " .
            "not supported?): $@" if ($@);
    }
    die "$class is an abstract base class. Instantiate a subclass instead."
      if ($class eq __PACKAGE__);

    my $self = {};
    bless $self, $class;
    $self = $self->_initialize(@_);

    return($self);

} #eosub--new
#--------------------------------------------------------------------------
sub _initialize {

=item C<_initialize>

 Description: Protected method that initializes an object after
              instantiation with the abstract schema. All subclasses should
              override this method. The typical subclass implementation
              should first call the C<_initialize> method of the superclass,
              then do any database-specific initialization (especially
              define the database-specific implementation of the all
              abstract data types), and then call the C<_adjust_schema>
              method.
 Parameters:  $abstract_schema (optional) - A reference to a hash. If 
                  provided, this hash will be used as the internal
                  representation of the abstract schema instead of our
                  default abstract schema. This is intended for internal 
                  use only by deserialize_abstract.
 Returns:     the instance of the Schema class

=cut

    my $self = shift;
    my $abstract_schema = shift;

    $abstract_schema ||= ABSTRACT_SCHEMA;

    $self->{schema} = dclone($abstract_schema);
    # While ABSTRACT_SCHEMA cannot be modified, 
    # $self->{abstract_schema} can be. So, we dclone it to prevent
    # anything from mucking with the constant.
    $self->{abstract_schema} = dclone($abstract_schema);

    return $self;

} #eosub--_initialize
#--------------------------------------------------------------------------
sub _adjust_schema {

=item C<_adjust_schema>

 Description: Protected method that alters the abstract schema at
              instantiation-time to be database-specific. It is a generic
              enough routine that it can be defined here in the base class.
              It takes the abstract schema and replaces the abstract data
              types with database-specific data types.
 Parameters:  none
 Returns:     the instance of the Schema class

=cut

    my $self = shift;

    # The _initialize method has already set up the db_specific hash with
    # the information on how to implement the abstract data types for the
    # instantiated DBMS-specific subclass.
    my $db_specific = $self->{db_specific};

    # Loop over each table in the abstract database schema.
    foreach my $table (keys %{ $self->{schema} }) {
        my %fields = (@{ $self->{schema}{$table}{FIELDS} });
        # Loop over the field defintions in each table.
        foreach my $field_def (values %fields) {
            # If the field type is an abstract data type defined in the
            # $db_specific hash, replace it with the DBMS-specific data type
            # that implements it.
            if (exists($db_specific->{$field_def->{TYPE}})) {
                $field_def->{TYPE} = $db_specific->{$field_def->{TYPE}};
            }
            # Replace abstract default values (such as 'TRUE' and 'FALSE')
            # with their database-specific implementations.
            if (exists($field_def->{DEFAULT})
                && exists($db_specific->{$field_def->{DEFAULT}})) {
                $field_def->{DEFAULT} = $db_specific->{$field_def->{DEFAULT}};
            }
        }
    }

    return $self;

} #eosub--_adjust_schema
#--------------------------------------------------------------------------
sub get_type_ddl {

=item C<get_type_ddl>

 Description: Public method to convert abstract (database-generic) field
              specifiers to database-specific data types suitable for use
              in a C<CREATE TABLE> or C<ALTER TABLE> SQL statment. If no
              database-specific field type has been defined for the given
              field type, then it will just return the same field type.
 Parameters:  a hash or a reference to a hash of a field containing the
              following keys: C<TYPE> (required), C<NOTNULL> (optional),
              C<DEFAULT> (optional), C<PRIMARYKEY> (optional), C<REFERENCES>
              (optional)
 Returns:     a DDL string suitable for describing a field in a
              C<CREATE TABLE> or C<ALTER TABLE> SQL statement

=cut

    my $self = shift;
    my $finfo = (@_ == 1 && ref($_[0]) eq 'HASH') ? $_[0] : { @_ };

    my $type = $finfo->{TYPE};
    die "A valid TYPE was not specified for this column." unless ($type);

    my $default = $finfo->{DEFAULT};
    # Replace any abstract default value (such as 'TRUE' or 'FALSE')
    # with its database-specific implementation.
    if ( defined $default && exists($self->{db_specific}->{$default}) ) {
        $default = $self->{db_specific}->{$default};
    }

    my $fkref = $self->{enable_references} ? $finfo->{REFERENCES} : undef;
    my $type_ddl = $self->{db_specific}{$type} || $type;
    $type_ddl .= " NOT NULL" if ($finfo->{NOTNULL});
    $type_ddl .= " DEFAULT $default" if (defined($default));
    $type_ddl .= " PRIMARY KEY" if ($finfo->{PRIMARYKEY});
    $type_ddl .= "\n\t\t\t\tREFERENCES $fkref" if $fkref;

    return($type_ddl);

} #eosub--get_type_ddl
#--------------------------------------------------------------------------
sub get_column_ddl {

=item C<get_column_ddl>

 Description: Public method to generate a DDL segment of a "create table"
              SQL statement for a given table and field.
 Parameters:  $table - the table name
              $column - a column in the table
 Returns:     a hash containing information about the column including its
              type (C<TYPE>), whether or not it can be null (C<NOTNULL>),
              its default value if it has one (C<DEFAULT), whether it is
              a etc. The hash will be empty if either the specified
              table or column does not exist in the database schema.

=cut

    my($self, $table, $column) = @_;

    my $thash = $self->{schema}{$table};
    return() unless ($thash);

    my %fields = @{ $thash->{FIELDS} };
    return() unless ($fields{$column});
    return %{ $fields{$column} };

} #eosub--get_column_ddl
#--------------------------------------------------------------------------
sub get_table_list {

=item C<get_table_list>

 Description: Public method for discovering what tables should exist in the
              Bugzilla database.
 Parameters:  none
 Returns:     an array of table names

=cut

    my $self = shift;

    return(sort(keys %{ $self->{schema} }));

} #eosub--get_table_list
#--------------------------------------------------------------------------
sub get_table_columns {

=item C<get_table_columns>

 Description: Public method for discovering what columns are in a given
              table in the Bugzilla database.
 Parameters:  $table - the table name
 Returns:     array of column names

=cut

    my($self, $table) = @_;
    my @ddl = ();

    my $thash = $self->{schema}{$table};
    die "Table $table does not exist in the database schema."
        unless (ref($thash));

    my @columns = ();
    my @fields = @{ $thash->{FIELDS} };
    while (@fields) {
        push(@columns, shift(@fields));
        shift(@fields);
    }

    return @columns;

} #eosub--get_table_columns
#--------------------------------------------------------------------------
sub get_table_ddl {

=item C<get_table_ddl>

 Description: Public method to generate the SQL statements needed to create
              the a given table and its indexes in the Bugzilla database.
              Subclasses may override or extend this method, if needed, but
              subclasses probably should override C<_get_create_table_ddl>
              or C<_get_create_index_ddl> instead.
 Parameters:  $table - the table name
 Returns:     an array of strings containing SQL statements

=cut

    my($self, $table) = @_;
    my @ddl = ();

    die "Table $table does not exist in the database schema."
        unless (ref($self->{schema}{$table}));

    my $create_table = $self->_get_create_table_ddl($table);
    push(@ddl, $create_table) if $create_table;

    my @indexes = @{ $self->{schema}{$table}{INDEXES} || [] };
    while (@indexes) {
        my $index_name = shift(@indexes);
        my $index_info = shift(@indexes);
        my($index_fields,$index_type);
        if (ref($index_info) eq 'HASH') {
            $index_fields = $index_info->{FIELDS};
            $index_type = $index_info->{TYPE};
        } else {
            $index_fields = $index_info;
            $index_type = '';
        }
        my $index_sql = $self->_get_create_index_ddl($table,
                                                     $index_name,
                                                     $index_fields,
                                                     $index_type);
        push(@ddl, $index_sql) if $index_sql;
    }

    push(@ddl, @{ $self->{schema}{$table}{DB_EXTRAS} })
      if (ref($self->{schema}{$table}{DB_EXTRAS}));

    return @ddl;

} #eosub--get_table_ddl
#--------------------------------------------------------------------------
sub _get_create_table_ddl {

=item C<_get_create_table_ddl>

 Description: Protected method to generate the "create table" SQL statement
              for a given table.
 Parameters:  $table - the table name
 Returns:     a string containing the DDL statement for the specified table

=cut

    my($self, $table) = @_;

    my $thash = $self->{schema}{$table};
    die "Table $table does not exist in the database schema."
        unless (ref($thash));

    my $create_table = "CREATE TABLE $table \(\n";

    my @fields = @{ $thash->{FIELDS} };
    while (@fields) {
        my $field = shift(@fields);
        my $finfo = shift(@fields);
        $create_table .= "\t$field\t" . $self->get_type_ddl($finfo);
        $create_table .= "," if (@fields);
        $create_table .= "\n";
    }

    $create_table .= "\)";

    return($create_table)

} #eosub--_get_create_table_ddl
#--------------------------------------------------------------------------
sub _get_create_index_ddl {

=item C<_get_create_index_ddl>

 Description: Protected method to generate a "create index" SQL statement
              for a given table and index.
 Parameters:  $table_name - the name of the table
              $index_name - the name of the index
              $index_fields - a reference to an array of field names
              $index_type (optional) - specify type of index (e.g., UNIQUE)
 Returns:     a string containing the DDL statement

=cut

    my($self, $table_name, $index_name, $index_fields, $index_type) = @_;

    my $sql = "CREATE ";
    $sql .= "$index_type " if ($index_type eq 'UNIQUE');
    $sql .= "INDEX $index_name ON $table_name \(" .
      join(", ", @$index_fields) . "\)";

    return($sql);

} #eosub--_get_create_index_ddl
#--------------------------------------------------------------------------

sub get_add_column_ddl {

=item C<get_alter_ddl($table, $column, \%definition)>

 Description: Generate SQL to add a column to a table.
 Params:      $table - The table containing the column.
              $column - The name of the column being added.
              \%definition - The new definition for the column,
                  in standard C<ABSTRACT_SCHEMA> format.
 Returns:     An array of SQL statements.

=cut
    my ($self, $table, $column, $definition) = @_;

    my $statement = "ALTER TABLE $table ADD COLUMN $column " .
        $self->get_type_ddl($definition);

    return ($statement);
}

sub get_alter_column_ddl {

=item C<get_alter_ddl($table, $column, \%definition)>

 Description: Generate SQL to alter a column in a table.
              The column that you are altering must exist,
              and the table that it lives in must exist.
 Params:      $table - The table containing the column.
              $column - The name of the column being changed.
              \%definition - The new definition for the column,
                  in standard C<ABSTRACT_SCHEMA> format.
 Returns:     An array of SQL statements.

=cut

    my ($self, $table, $column, $new_def) = @_;

    my @statements;
    my $old_def = $self->get_column_abstract($table, $column);
    my $specific = $self->{db_specific};

    my $typechange = 0;
    # If the types have changed, we have to deal with that.
    if (uc(trim($old_def->{TYPE})) ne uc(trim($new_def->{TYPE}))) {
        $typechange = 1;
        my $type = $new_def->{TYPE};
        $type = $specific->{$type} if exists $specific->{$type};
        # Make sure we can CAST from the old type to the new without an error.
        push(@statements, "SELECT CAST($column AS $type) FROM $table LIMIT 1");
        # Add a new temporary column of the new type
        push(@statements, "ALTER TABLE $table ADD COLUMN ${column}_ALTERTEMP"
                        . " $type");
        # UPDATE the temp column to have the same values as the old column
        push(@statements, "UPDATE $table SET ${column}_ALTERTEMP = " 
                        . " CAST($column AS $type)");
        # DROP the old column
        push(@statements, "ALTER TABLE $table DROP COLUMN $column");
        # And rename the temp column to be the new one.
        push(@statements, "ALTER TABLE $table RENAME COLUMN "
                        . " ${column}_ALTERTEMP TO $column");

        # FIXME - And now, we have to regenerate any indexes that got
        #         dropped, except for the PK index which will be handled
        #         below.
    }

    my $default = $new_def->{DEFAULT};
    my $default_old = $old_def->{DEFAULT};
    # If we went from having a default to not having one
    if (!defined $default && defined $default_old) {
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column"
                        . " DROP DEFAULT");
    }
    # If we went from no default to a default, or we changed the default,
    # or we have a default and we changed the data type of the field
    elsif ( (defined $default && !defined $default_old) || 
         ($default ne $default_old) ||
         ($typechange && defined $new_def->{DEFAULT}) ) {
        $default = $specific->{$default} if exists $specific->{$default};
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column "
                         . " SET DEFAULT $default");
    }

    # If we went from NULL to NOT NULL
    # OR if we changed the type and we are NOT NULL
    if ( (!$old_def->{NOTNULL} && $new_def->{NOTNULL}) ||
         ($typechange && $new_def->{NOTNULL}) ) {
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column"
                          . " SET NOT NULL");
    }
    # If we went from NOT NULL to NULL
    elsif ($old_def->{NOTNULL} && !$new_def->{NOTNULL}) {
        push(@statements, "ALTER TABLE $table ALTER COLUMN $column"
                        . " DROP NOT NULL");
    }

    # If we went from not being a PRIMARY KEY to being a PRIMARY KEY,
    # or if we changed types and we are a PK.
    if ( (!$old_def->{PRIMARYKEY} && $new_def->{PRIMARYKEY}) ||
         ($typechange && $new_def->{PRIMARYKEY}) ) {
        push(@statements, "ALTER TABLE $table ADD PRIMARY KEY ($column)");
    }
    # If we went from being a PK to not being a PK
    elsif ( $old_def->{PRIMARYKEY} && !$new_def->{PRIMARYKEY} ) {
        push(@statements, "ALTER TABLE $table DROP PRIMARY KEY");
    }

    return @statements;
}

sub get_column_abstract {

=item C<get_column_abstract($table, $column)>


 Description: A column definition from the abstract internal schema.
              cross-database format.
 Params:      $table - The name of the table
              $column - The name of the column that you want
 Returns:     A hash reference. For the format, see the docs for
              C<ABSTRACT_SCHEMA>.
              Returns undef if the column or table does not exist.

=cut

    my ($self, $table, $column) = @_;

    # Prevent a possible dereferencing of an undef hash, if the
    # table doesn't exist.
    if (exists $self->{abstract_schema}->{$table}) {
        my %fields = (@{ $self->{abstract_schema}{$table}{FIELDS} });
        return $fields{$column};
    }
    return undef;
}

sub get_index_abstract {

=item C<get_index_abstract($table, $index)

 Description: Returns an index definition from the internal abstract schema.
 Params:      $table - The table the index is on.
              $index - The name of the index.
 Returns:     A hash reference representing an index definition.
              See the C<ABSTRACT_SCHEMA> docs for details.
              Returns undef if the index does not exist.

=cut

    my ($self, $table, $index) = @_;

    # Prevent a possible dereferencing of an undef hash, if the
    # table doesn't exist.
    if (exists $self->{abstract_schema}->{$table}) {
        my %indexes = (@{ $self->{abstract_schema}{$table}{INDEXES} });
        return dclone($indexes{$index});
    }
    return undef;
}

sub set_column {

=item C<set_column($table, $column, \%new_def)>

 Description: Changes the definition of a column in this Schema object.
              If the column doesn't exist, it will be added.
              The table that you specify must already exist in the Schema.
              NOTE: This does not affect the database on the disk.
              Use the C<Bugzilla::DB> "Schema Modification Methods"
              if you want to do that.
 Params:      $table - The name of the table that the column is on.
              $column - The name of the column.
              \%new_def - The new definition for the column, in 
                  C<ABSTRACT_SCHEMA> format.
 Returns:     nothing

=cut

    my ($self, $table, $column, $new_def) = @_;

    my $abstract_fields = \@{ $self->{abstract_schema}{$table}{FIELDS} };

    my $field_position = lsearch($abstract_fields, $column) + 1;
    # If the column doesn't exist, then add it.
    if (!$field_position) {
        push(@$abstract_fields, $column);
        push(@$abstract_fields, $new_def);
    }
    # We're modifying an existing column.
    else {
        splice(@$abstract_fields, $field_position, 1, $new_def);
    }

    $self->{schema} = dclone($self->{abstract_schema});
    $self->_adjust_schema();
}

sub columns_equal {

=item C<columns_equal($col_one, $col_two)>

 Description: Tells you if two columns have entirely identical definitions.
              The TYPE field's value will be compared case-insensitive.
              However, all other fields will be case-sensitive.
 Params:      $col_one, $col_two - The columns to compare. Hash 
                  references, in C<ABSTRACT_SCHEMA> format.
 Returns:     C<1> if the columns are identical, C<0> if they are not.
=cut

    my $self = shift;
    my $col_one = dclone(shift);
    my $col_two = dclone(shift);

    $col_one->{TYPE} = uc($col_one->{TYPE});
    $col_two->{TYPE} = uc($col_two->{TYPE});

    my @col_one_array = %$col_one;
    my @col_two_array = %$col_two;

    my ($removed, $added) = diff_arrays(\@col_one_array, \@col_two_array);

    # If there are no differences between the arrays,
    # then they are equal.
    return !scalar(@$removed) && !scalar(@$added) ? 1 : 0;
}


=head1 SERIALIZATION/DESERIALIZATION

=over 4

=item C<serialize_abstract()>

 Description: Serializes the "abstract" schema into a format
              that deserialize_abstract() can read in. This is
              a method, called on a Schema instance.
 Parameters:  none
 Returns:     A scalar containing the serialized, abstract schema.
              Do not attempt to manipulate this data directly,
              as the format may change at any time in the future.
              The only thing you should do with the returned value
              is either store it somewhere or deserialize it.

=cut
sub serialize_abstract {
    my ($self) = @_;
    # We do this so that any two stored Schemas will have the
    # same byte representation if they are identical.
    # We don't need it currently, but it might make things
    # easier in the future.
    local $Storable::canonical = 1;
    return freeze($self->{abstract_schema});
}

=item C<deserialize_abstract($serialized, $version)>

 Description: Used for when you've read a serialized Schema off the disk,
              and you want a Schema object that represents that data.
 Params:      $serialized - scalar. The serialized data.
              $version - A number in the format X.YZ. The "version"
                  of the Schema that did the serialization.
                  See the docs for C<SCHEMA_VERSION> for more details.
 Returns:     A Schema object. It will have the methods of (and work 
              in the same fashion as) the current version of Schema. 
              However, it will represent the serialized data instead of
              ABSTRACT_SCHEMA.
=cut
sub deserialize_abstract {
    my ($class, $serialized, $version) = @_;

    my $thawed_hash = thaw($serialized);

    # At this point, we have no backwards-compatibility
    # code to write, so $version is ignored.
    # For what $version ought to be used for, see the
    # "private" section of the SCHEMA_VERSION docs.

    return $class->new(undef, $thawed_hash);
}

1;
__END__

=back

=head1 ABSTRACT DATA TYPES

The following abstract data types are used:

=over 4

=item C<BOOLEAN>

=item C<INT1>

=item C<INT2>

=item C<INT3>

=item C<INT4>

=item C<SMALLSERIAL>

=item C<MEDIUMSERIAL>

=item C<INTSERIAL>

=item C<TINYTEXT>

=item C<MEDIUMTEXT>

=item C<TEXT>

=item C<LONGBLOB>

=item C<DATETIME>

=back

Database-specific subclasses should define the implementation for these data
types as a hash reference stored internally in the schema object as
C<db_specific>. This is typically done in overriden L<_initialize> method.

The following abstract boolean values should also be defined on a
database-specific basis:

=over 4

=item C<TRUE>

=item C<FALSE>

=back

=head1 SEE ALSO

L<Bugzilla::DB>

=cut
