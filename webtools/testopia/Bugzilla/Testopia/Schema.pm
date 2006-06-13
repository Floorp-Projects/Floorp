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
# Contributor(s): Justin C. De Vries <judevries@novell.com>

package Bugzilla::Testopia::Schema;

use base qw(Bugzilla::DB::Schema);

use strict;
use Bugzilla::Error;
use Bugzilla::Util;
use Carp;
use Safe;

# Historical, needed for SCHEMA_VERSION = '1.00'
use Storable qw(dclone freeze thaw);

#New SCHEMA_Version (2.00) use this
use Data::Dumper;

use constant SCHEMA_VERSION  => '2.00';
use constant ABSTRACT_SCHEMA => {
    #Note: Most of types will need to be defined in a MySQL specific file some where.
    test_category_templates => {
        FIELDS => [
            # Note: UNSMALLSERIAL might need to be added to the MySQL.pm file. Its
            # definition is UNSIGNED SMALLINT AUTO_INCREMENT.
            category_template_id    => {TYPE => 'UNSMALLSERIAL', NOTNULL => 1},
            name                    => {TYPE => 'varchar(255)', NOTNULL => 1},
            description             => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            category_template_name_idx  => ['name'],
        ],
    },

    test_attachments => {
        FIELDS => [
            # Note: Serial is a MySQL specific type. This will need to
            # be written or modified to be database generic.
            attachment_id => {TYPE => 'SERIAL'},
            # Note: UNBIGINT need to be added to the MySQL.pm file. Its
            # definition is UNSIGNED BIGINT.
            plan_id       => {TYPE => 'UNBIGINT'},
            case_id       => {TYPE => 'UNBIGINT'},
            submitter_id  => {TYPE => 'INT3', NOTNULL => 1},
            description   => {TYPE => 'MEDIUMTEXT'},
            filename      => {TYPE => 'MEDIUMTEXT'},
            creation_ts   => {TYPE => 'DATETIME'},
            mime_type     => {TYPE => 'varchar(100)', NOTNULL => 1},
        ],
        INDEXES => [
            attachment_plan_idx => ['plan_id'],
            attachment_case_idx => ['case_id'],
        ],
    },

    test_case_categories => {
        FIELDS => [
            category_id => {TYPE => 'UNSMALLSERIAL', NOTNULL => 1},
            product_id  => {TYPE => 'INT2', NOTNULL => 1},
            name        => {TYPE => 'varchar(240)', NOTNULL => 1},
            description => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            category_name_idx => ['name'],
        ],
    },
   
   test_cases => {
        FIELDS => [
            case_id        => {TYPE => 'SERIAL'},
            case_status_id => {TYPE => 'INT1', NOTNULL => 1},
            # Note: UNINT2 need to be added to the MySQL.pm file. Its
            # definition is UNSIGNED SMALLINT.
            category_id    => {TYPE => 'UNINT2', NOTNULL => 1},
            priority_id    => {TYPE => 'INT2'},
            author_id      => {TYPE => 'INT3', NOTNULL => 1},
            creation_date  => {TYPE => 'DATETIME', NOTNULL => 1},
            isautomated    => {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '0'},
            sortkey        => {TYPE => 'INT4'},
            script         => {TYPE => 'MEDIUMTEXT'},
            arguments      => {TYPE => 'MEDIUMTEXT'},
            summary        => {TYPE => 'varchar(255)'},
            requirement    => {TYPE => 'varchar(255)'},
            alias          => {TYPE => 'varchar(255)'},
        ],
        INDEXES => [
            test_case_category_idx      => ['category_id'],
            test_case_author_idx        => ['author_id'],
            test_case_creation_date_idx => ['creation_date'],
            test_case_sortkey_idx       => ['sortkey'],
            test_case_requirement_idx   => ['requirement'],
            test_case_shortname_idx     => ['alias'],
        ],     
    },
   
    test_case_bugs => {
        FIELDS => [
            bug_id      => {TYPE => 'INT3', NOTNUTLL => 1},
            case_run_id => {TYPE => 'UNBIGINT'},
        ],    
        INDEXES => [
            case_run_id_idx     => [qw(case_run_id bug_id)],
            case_run_bug_id_idx => ['bug_id'],
        ],
    },        
    
    test_case_runs => {
        FIELDS => [ 
            case_run_id         => {TYPE => 'SERIAL'},
            run_id              => {TYPE => 'UNBIGINT', NOTNULL => 1},
            case_id             => {TYPE => 'UNBIGINT', NOTNULL => 1},
            assignee            => {TYPE => 'INT3'},
            testedby            => {TYPE => 'INT3'},
            case_run_status_id  => {TYPE => 'UNINT1', NOTNULL => 1},
            case_text_version   => {TYPE => 'UNINT3', NOTNULL => 1},
            build_id            => {TYPE => 'UNBIGINT', NOTNULL => 1},
            close_date          => {TYPE => 'DATETIME'},
            notes               => {TYPE => 'TEXT'},
            iscurrent           => {TYPE => 'BOOLEAN', DEFAULT => '0'},
            sortkey             => {TYPE => 'INT4'},
        ],
        INDEXES => [
            case_run_run_id_idx     => ['run_id'],
            case_run_case_id_idx    => ['case_id'],
            case_run_assignee_idx   => ['assignee'],
            case_run_testedby_idx   => ['testedby'],
            case_run_close_date_idx => ['close_date'],
            case_run_shortkey_idx   => ['sortkey'],
        ],
    },
    
    test_case_texts => {
        FIELDS => [
            case_id           => {TYPE => 'UNBIGINT', NOTNULL =>1},        
            case_text_version => {TYPE => 'INT3', NOTNULL =>1},        
            who               => {TYPE => 'INT3', NOTNULL =>1},        
            creation_ts       => {TYPE => 'DATETIME', NOTNULL =>1},        
            action            => {TYPE => 'MEDIUMTEXT'},        
            effect            => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            case_versions_idx             => {FIELDS => [qw(case_id case_text_version)],
                                              TYPE => 'UNIQUE'},
            case_versions_who_idx         => ['who'],                      
            case_versions_creation_ts_idx => ['creation_ts'],
        ],
    },
    
    test_case_tags => {
        FIELDS => [
            tag_id  => {TYPE => 'UNINT4', NOTNULL => 1},
            case_id => {TYPE => 'UNBIGINT'},
        ],
        INDEXES => [
            case_tags_case_id_idx => [qw(case_id tag_id)],
            case_tags_tag_id_idx  => [qw(tag_id case_id)],
            test_tag_name_indx    => ['tag_name'],
        ],
    },
    
    test_plan_testers => {
        FIELDS => [
            tester_id  => {TYPE => 'INT3', NOTNULL => 1},
            product_id => {TYPE => 'INT2'},
            read_only  => {TYPE => 'BOOLEAN', DEFAULT => '1'},
        ],
    },
    
    test_tags => {
        FIELDS => [
            # Note: UNINT4SERIAL need to be added to the MySQL.pm file. Its
            # definition is UNSIGNED INTEGER AUTO_INCREMENT.
            tag_id   => {TYPE => 'UNINT4SERIAL', NOTNULL => 1},
            tag_name => {TYPE => 'varchar(255)', NOTNULL => 1},
        ],
    },
    
    test_plans => {
        FIELDS => [
            plan_id                 => {TYPE => 'SERIAL'},
            product_id              => {TYPE => 'INT2', NOTNULL => 1},
            author_id               => {TYPE => 'INT3', NOTNULL => 1},
            editor_id               => {TYPE => 'INT3', NOTNULL => 1},
            type_id                 => {TYPE => 'UNINT1', NOTNULL => 1},
            default_product_version => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
            name                    => {TYPE => 'varchar(255)', NOTNULL => 1},
            creation_date           => {TYPE => 'DATETIME', NOTNULL => 1},
            isactive                => {TYPE => 'BOOLEAN', NOTNULL =>, DEFAULT => '1'},
        ],
        INDEXES => [
            plan_product_plan_id_idx => [qw(product_id plan_id)],
            plan_author_idx          => ['author_id'],        
            plan_type_idx            => ['type_id'],        
            plan_editor_idx          => ['editor_id'],        
            plan_isactive_idx        => ['isactive'],
        ],
    },

    text_plan_text => {
        FIELDS => [
            plan_id           => {TYPE => 'UNBIGINT', NOTNULL => 1},
            plan_text_version => {TYPE => 'INT4', NOTNULL => 1},
            who               => {TYPE => 'INT3', NOTNULL => 1},
            creation_ts       => {TYPE => 'DATETIME', NOTNULL => 1},
            # Note: LONGTEXT needs to be added to the MySQL.pm file. Its
            # definition is LONGTEXT.
            plan_text         => {TYPE => 'LONGTEXT'},
        ],
        INDEXES => [
            test_plan_text_version_idx => [qw(plan_id plan_text_version)],
            test_plan_text_who_idx     => ['who'],
        ],
    },
    
    test_plan_types => {
        FIELDS => [
            # Note: UNTINYINTSERIAL needs to be added to the MySQL.pm file. Its
            # definition is UNSIGNED TINYINT AUTO_INCREMENT.
            type_id => {TYPE => 'UNTINYSERIAL', NOTNULL => 1},
            name    => {TYPE => 'varchar(64)', NOTNULL => 1},
        ],
        INDEXES => [
            plan_type_name_idx => ['name'],
        ],
    },
    
    test_runs => {
        FIELDS => [
            run_id            => {TYPE => 'SERIAL'},
            plan_id           => {TYPE => 'UNBIGINT', NOTNULL => 1},                           
            environment_id    => {TYPE => 'UNBIGINT', NOTNULL => 1},                           
            product_version   => {TYPE => 'MEDIUMTEXT'},                           
            build_id          => {TYPE => 'UNBIGINT', NOTNULL => 1},                           
            plan_text_version => {TYPE => 'INT4', NOTNULL => 1},                           
            manager_id        => {TYPE => 'INT3', NOTNULL => 1},                           
            default_tester_id => {TYPE => 'INT3'},                           
            start_date        => {TYPE => 'DATETIME'},                           
            stop_date         => {TYPE => 'DATETIME'},                           
            summary           => {TYPE => 'TINYTEXT', NOTNULL => 1},                           
            notes             => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            test_run_plan_id_run_id_idx => [qw(plan_id run_id)],
            test_run_manager_idx        => ['manager_id'],
            test_run_start_date_idx     => ['start_date'],
        ],
    },
    
    test_case_plans => {
        FIELDS => [
            plan_id => {TYPE => 'UNBIGINT', NOTNULL => 1},                           
            case_id => {TYPE => 'UNBIGINT', NOTNULL => 1},
        ],
        INDEXES => [
            case_plans_plan_id_idx => ['plan_id'],
            case_plans_case_id_idx => ['plan_id'],
        ],
    },

    test_case_activity => {
        FIELDS => [
            case_id  => {TYPE => 'UNBIGINT', NOTNULL => 1},
            fieldid  => {TYPE => 'UNINT2', NOTNULL => 1},
            who      => {TYPE => 'INT3', NOTNULL => 1},
            changed  => {TYPE => 'DATETIME', NOTNULL => 1},
            oldvalue => {TYPE => 'MEDUIMTEXT'},
            newvalue => {TYPE => 'MEDUIMTEXT'},
        ],
        INDEXES => [
            case_activity_case_id_idx => ['case_id'],    
            case_activity_who_idx     => ['who'],    
            case_activity_when_idx    => ['changed'],    
            case_activity_field_idx   => ['fieldid'],
        ],
    },

    test_fielddefs => {
        FIELDS => [
            fieldid     => {TYPE => 'UNSMALLSERIAL', NOTNULL => 1},
            name        => {TPYE => 'varchar(100)', NOTNULL => 1},
            description => {TYPE => 'MEDIUMTEXT'},
            table_name  => {TYPE => 'varchar(100)', NOTNULL => 1},
        ],
        INDEXES => [
            fielddefs_name_idx => ['name'],
        ],
    },
    
    test_plan_activity => {
        FIELDS => [
            plan_id  => {TYPE => 'UNBIGINT', NOTNULL => 1},             
            fieldid  => {TYPE => 'UNINT2', NOTNULL => 1},             
            who      => {TYPE => 'INT3', NOTNULL => 1},             
            changed  => {TYPE => 'DATETIME', NOTNULL => 1},             
            oldvalue => {TYPE => 'MEDIUMTEXT'},             
            newvalue => {TYPE => 'MEDIUMTEXT'},             
        ],
        INDEXES => [
            plan_activity_who_idx => ['who'],
        ],
    },
    
    test_case_components => {
        FIELDS => [
            case_id      => {TYPE => 'UNBIGINT', NOTNULL => 1},
            component_id => {TYPE => 'INT2', NOTNULL => 1},
        ],
        INDEXES => [
            case_components_case_id_idx       => ['case_id'],    
            case_commponents_component_id_idx => ['case_id'],
        ],
    },
    
    test_run_activity => {
        FIELDS => [
            run_id   => {TYPE => 'UNBIGINT', NOTNULL => 1},
            fieldid  => {TYPE => 'UNINT2', NOTNULL => 1},
            who      => {TYPE => 'INT3', NOTNULL => 1},
            changed  => {TYPE => 'DATETIME', NOTNULL => 1},
            oldvalue => {TYPE => 'MEDIUMTEXT'},
            newvalue => {TYPE => 'MEDIUMTEXT'},
        ],
        INDEXES => [
            run_activity_run_id_idx => ['run_id'],    
            run_activity_who_idx    => ['who'],    
            run_activity_when_idx   => ['changed'],
        ],
    },
    
    test_run_cc => {
        FIELDS => [
            run_id => {TYPE => 'UNBIGINT', NOTNULL => 1},                     
            who    => {TYPE => 'INT3', NOTNULL => 1},
        ],
        INDEXES => [
            run_cc_run_id_who_idx => [qw(run_id who)],
        ],
    },
    
    test_email_settings => {
        FIELDS => [
            userid          => {TYPE => 'INT3', NOTNULL => 1},                                   
            eventid         => {TYPE => 'UNINT1', NOTNULL => 1},                                   
            relationship_id => {TYPE => 'UNINT1', NOTNULL => 1},
        ],
        INDEXES => [
            test_event_user_event_idx        => [qw(userid eventid)],    
            test_event_user_relationship_idx => [qw(userid relationship_id)],    
        ],
    },
    
    test_events => {
        FIELDS => [
            eventid => {TYPE => 'UNIN1', NOTNULL => 1},        
            name    => {TYPE => 'varchar(50)'},
        ],
        INDEXES => [
            test_event_name_idx => ['name'],
        ],
    },
    
    test_relationships => {
        FIELDS => [
            relationship_id => {TYPE => 'UNINT1', NOTNULL => 1},            
            name            => {TYPE => 'varchar(50)'},
        ],
    },
    
    test_case_run_status => {
        FIELDS => [
            case_run_status_id => {TYPE => 'UNTINYSERIAL', NOTNULL => 1},                    
            name               => {TYPE => 'varchar(20)'},                    
            sortkey            => {TYPE => 'INT4'},
        ],
        INDEXES => [
            case_run_status_name_idx => ['name'],    
            case_run_status_sortkey_idx => ['sortkey'],    
        ],
    },
    
    test_case_status => {
        FIELDS => [
            case_status_id => {TYPE => 'UNTINYSERIAL', NOTNULL => 1},                    
            name           => {TYPE => 'varchar(255)'},                    
        ],
        INDEXES => [
            test_case_status_name_idx => ['name'],    
        ],
    },    
    
    test_case_dependencies => {
        FIELDS => [
            dependson => {TYPE => 'UNBIGINT', NOTNULL => 1},                    
            blocks    => {TYPE => 'UNBIGINT'},                    
        ],
    },

    test_case_group_map => {
        FIELDS => [
            case_id  => {TYPE => 'UNBIGINT', NOTNULL =>1},
            group_id => {TYPE => 'INT3'},
        ],
    },    
        
    test_environments => {
        FIELDS => [
            environment_id  => {TYPE => 'SERIAL', NOTNULL => 1},        
            op_sys_id       => {TYPE => 'INT4'},        
            rep_platform_id => {TYPE => 'INT4'},        
            name            => {TYPE => 'varchar(255)'},        
            xml             => {TYPE => 'MEDIUMTEXT'},        
        ],    
        INDEXES => [
            environment_op_sys_idx   => ['op_sys_id'],
            environment_platform_idx => ['rep_platform_id'],
            environment_name_idx     => ['name'],
        ],    
    },

    test_run_tags => {    
        FIELDS => [
            tag_id => {TYPE => 'UNINT4', NOTNULL => 1},
            run_id => {TYPE => 'UNBIGINT'},
        ],
        INDEXES => [
            run_tags_idx => ['qw(tag_id, run_id'],
        ],    
    },
    
    test_plan_tags => {
        FIELDS => [
            tag_id  => {TYPE => 'UNINT4', NOTNULL => 1},
            plan_id => {TYPE => 'UNBIGINT'},        
        ],
        INDEXES => [
            plan_tags_idx => ['qw(tag_id, plan_id'],
        ],    
    },
    
    test_build => {
        FIELDS => [
            build_id    => {TYPE => 'SERIAL', NOTNULL => 1},
            product_id  => {TYPE => 'INT2', NOTNULL => 1},
            name        => {TYPE => 'varchar(255)'},
            description => {TYPE => 'TEXT'},        
        ],
    },
    
    test_attachment_data => {
        FIELDS => [
            attachment_id => {TYPE => 'UNBIGINT', NOTNULL => 1},
            contents      => {TYPE => 'LONGBLOB'},
        ],          
    },    

    test_named_queries => {
        FIELDS => [
            userid => {TYPE => 'INT3', NOTNULL => 1},
            name => {TYPE => 'varchar(64)', NOTNULL => 1},
            isvisible => {TYEP => 'BOOLEAN', NOTNULL => 1, DEFAULT => 1},
            query => {TYPE => 'MEDIUMTEXT', NOTNULL => 1},
        ],
        INDEXES => [
            test_namedquery_name_idx => ['name'],
        ],    
    },        
            
};    

1;
