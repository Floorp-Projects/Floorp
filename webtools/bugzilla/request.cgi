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
# Contributor(s): Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# Include the Bugzilla CGI and general utility library.
use lib qw(.);
require "CGI.pl";

# Establish a connection to the database backend.
ConnectToDatabase();

# Use Bugzilla's Request module which contains utilities for handling requests.
use Bugzilla::Flag;
use Bugzilla::FlagType;

# use Bugzilla's User module which contains utilities for handling users.
use Bugzilla::User;

use vars qw($template $vars @legal_product @legal_components %components);

# Make sure the user is logged in.
quietly_check_login();

################################################################################
# Main Body Execution
################################################################################

queue();
exit;

################################################################################
# Functions
################################################################################

sub queue {
    validateStatus();
    validateGroup();
    
    my $attach_join_clause = "flags.attach_id = attachments.attach_id";
    if (Param("insidergroup") && !UserInGroup(Param("insidergroup"))) {
        $attach_join_clause .= " AND attachments.isprivate < 1";
    }

    my $query = 
    # Select columns describing each flag, the bug/attachment on which
    # it has been set, who set it, and of whom they are requesting it.
    " SELECT    flags.id, flagtypes.name,
                flags.status,
                flags.bug_id, bugs.short_desc,
                products.name, components.name,
                flags.attach_id, attachments.description,
                requesters.realname, requesters.login_name,
                requestees.realname, requestees.login_name,
                flags.creation_date,
    " . 
    # Select columns that help us weed out secure bugs to which the user
    # should not have access.
    "            COUNT(DISTINCT ugmap.group_id) AS cntuseringroups, 
                COUNT(DISTINCT bgmap.group_id) AS cntbugingroups, 
                ((COUNT(DISTINCT ccmap.who) AND cclist_accessible) 
                  OR ((bugs.reporter = $::userid) AND bugs.reporter_accessible) 
                  OR bugs.assigned_to = $::userid ) AS canseeanyway 
    " . 
    # Use the flags and flagtypes tables for information about the flags,
    # the bugs and attachments tables for target info, the profiles tables
    # for setter and requestee info, the products/components tables
    # so we can display product and component names, and the bug_group_map
    # and user_group_map tables to help us weed out secure bugs to which
    # the user should not have access.
    " FROM      flags 
                LEFT JOIN attachments ON ($attach_join_clause), 
                flagtypes, 
                profiles AS requesters
                LEFT JOIN profiles AS requestees 
                  ON flags.requestee_id  = requestees.userid, 
                bugs 
                LEFT JOIN products ON bugs.product_id = products.id
                LEFT JOIN components ON bugs.component_id = components.id
                LEFT JOIN bug_group_map AS bgmap 
                  ON bgmap.bug_id = bugs.bug_id 
                LEFT JOIN user_group_map AS ugmap 
                  ON bgmap.group_id = ugmap.group_id 
                  AND ugmap.user_id = $::userid 
                  AND ugmap.isbless = 0
                LEFT JOIN cc AS ccmap 
                ON ccmap.who = $::userid AND ccmap.bug_id = bugs.bug_id 
    " . 
    # All of these are inner join clauses.  Actual match criteria are added
    # in the code below.
    " WHERE     flags.type_id       = flagtypes.id
      AND       flags.setter_id     = requesters.userid
      AND       flags.bug_id        = bugs.bug_id
    ";
    
    # Limit query to pending requests.
    $query .= " AND flags.status = '?' " unless $::FORM{'status'};

    # The set of criteria by which we filter records to display in the queue.
    my @criteria = ();
    
    # A list of columns to exclude from the report because the report conditions
    # limit the data being displayed to exact matches for those columns.
    # In other words, if we are only displaying "pending" , we don't
    # need to display a "status" column in the report because the value for that
    # column will always be the same.
    my @excluded_columns = ();
    
    # Filter requests by status: "pending", "granted", "denied", "all" 
    # (which means any), or "fulfilled" (which means "granted" or "denied").
    if ($::FORM{'status'}) {
        if ($::FORM{'status'} eq "+-") {
            push(@criteria, "flags.status IN ('+', '-')");
            push(@excluded_columns, 'status') unless $::FORM{'do_union'};
        }
        elsif ($::FORM{'status'} ne "all") {
            push(@criteria, "flags.status = '$::FORM{'status'}'");
            push(@excluded_columns, 'status') unless $::FORM{'do_union'};
        }
    }
    
    # Filter results by exact email address of requester or requestee.
    if (defined($::FORM{'requester'}) && $::FORM{'requester'} ne "") {
        push(@criteria, "requesters.login_name = " . SqlQuote($::FORM{'requester'}));
        push(@excluded_columns, 'requester') unless $::FORM{'do_union'};
    }
    if (defined($::FORM{'requestee'}) && $::FORM{'requestee'} ne "") {
        push(@criteria, "requestees.login_name = " . SqlQuote($::FORM{'requestee'}));
        push(@excluded_columns, 'requestee') unless $::FORM{'do_union'};
    }
    
    # Filter results by exact product or component.
    if (defined($::FORM{'product'}) && $::FORM{'product'} ne "") {
        my $product_id = get_product_id($::FORM{'product'});
        if ($product_id) {
            push(@criteria, "bugs.product_id = $product_id");
            push(@excluded_columns, 'product') unless $::FORM{'do_union'};
            if (defined($::FORM{'component'}) && $::FORM{'component'} ne "") {
                my $component_id = get_component_id($product_id, $::FORM{'component'});
                if ($component_id) {
                    push(@criteria, "bugs.component_id = $component_id");
                    push(@excluded_columns, 'component') unless $::FORM{'do_union'};
                }
                else { ThrowCodeError("unknown_component", { %::FORM }) }
            }
        }
        else { ThrowCodeError("unknown_product", { %::FORM }) }
    }
    
    # Filter results by flag types.
    if (defined($::FORM{'type'}) && !grep($::FORM{'type'} eq $_, ("", "all"))) {
        # Check if any matching types are for attachments.  If not, don't show
        # the attachment column in the report.
        my $types = Bugzilla::FlagType::match({ 'name' => $::FORM{'type'} });
        my $has_attachment_type = 0;
        foreach my $type (@$types) {
            if ($type->{'target_type'} eq "attachment") {
                $has_attachment_type = 1;
                last;
            }
        }
        if (!$has_attachment_type) { push(@excluded_columns, 'attachment') }
        
        push(@criteria, "flagtypes.name = " . SqlQuote($::FORM{'type'}));
        push(@excluded_columns, 'type') unless $::FORM{'do_union'};
    }
    
    # Add the criteria to the query.  We do an intersection by default 
    # but do a union if the "do_union" URL parameter (for which there is no UI 
    # because it's an advanced feature that people won't usually want) is true.
    my $and_or = $::FORM{'do_union'} ? " OR " : " AND ";
    $query .= " AND (" . join($and_or, @criteria) . ") " if scalar(@criteria);
    
    # Group the records by flag ID so we don't get multiple rows of data
    # for each flag.  This is only necessary because of the code that
    # removes flags on bugs the user is unauthorized to access.
    $query .= " GROUP BY flags.id " . 
              "HAVING cntuseringroups = cntbugingroups OR canseeanyway ";

    # Group the records, in other words order them by the group column
    # so the loop in the display template can break them up into separate
    # tables every time the value in the group column changes.
    $::FORM{'group'} ||= "requestee";
    if ($::FORM{'group'} eq "requester") {
        $query .= " ORDER BY requesters.realname, requesters.login_name";
    }
    elsif ($::FORM{'group'} eq "requestee") {
        $query .= " ORDER BY requestees.realname, requestees.login_name";
    }
    elsif ($::FORM{'group'} eq "category") {
        $query .= " ORDER BY products.name, components.name";
    }
    elsif ($::FORM{'group'} eq "type") {
        $query .= " ORDER BY flagtypes.name";
    }

    # Order the records (within each group).
    $query .= " , flags.creation_date";
    
    # Pass the query to the template for use when debugging this script.
    $vars->{'query'} = $query;
    $vars->{'debug'} = $::FORM{'debug'} ? 1 : 0;
    
    SendSQL($query);
    my @requests = ();
    while (MoreSQLData()) {
        my @data = FetchSQLData();
        my $request = {
          'id'              => $data[0] , 
          'type'            => $data[1] , 
          'status'          => $data[2] , 
          'bug_id'          => $data[3] , 
          'bug_summary'     => $data[4] , 
          'category'        => "$data[5]: $data[6]" , 
          'attach_id'       => $data[7] , 
          'attach_summary'  => $data[8] ,
          'requester'       => ($data[9] ? "$data[9] <$data[10]>" : $data[10]) , 
          'requestee'       => ($data[11] ? "$data[11] <$data[12]>" : $data[12]) , 
          'created'         => $data[13]
        };
        push(@requests, $request);
    }

    # Get a list of request type names to use in the filter form.
    my @types = ("all");
    SendSQL("SELECT DISTINCT(name) FROM flagtypes ORDER BY name");
    push(@types, FetchOneColumn()) while MoreSQLData();
    
    # products and components and the function used to modify the components
    # menu when the products menu changes; used by the template to populate
    # the menus and keep the components menu consistent with the products menu
    GetVersionTable();
    $vars->{'products'} = \@::legal_product;
    $vars->{'components'} = \@::legal_components;
    $vars->{'components_by_product'} = \%::components;
    
    $vars->{'excluded_columns'} = \@excluded_columns;
    $vars->{'group_field'} = $::FORM{'group'};
    $vars->{'requests'} = \@requests;
    $vars->{'form'} = \%::FORM;
    $vars->{'types'} = \@types;

    # Return the appropriate HTTP response headers.
    print "Content-type: text/html\n\n";

    # Generate and return the UI (HTML page) from the appropriate template.
    $template->process("request/queue.html.tmpl", $vars)
      || ThrowTemplateError($template->error());
}

################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateStatus {
    return if !defined($::FORM{'status'});
    
    grep($::FORM{'status'} eq $_, qw(? +- + - all))
      || ThrowCodeError("flag_status_invalid", { status => $::FORM{'status'} });
}

sub validateGroup {
    return if !defined($::FORM{'group'});
    
    grep($::FORM{'group'} eq $_, qw(requester requestee category type))
      || ThrowCodeError("request_queue_group_invalid", 
                        { group => $::FORM{'group'} });
}

