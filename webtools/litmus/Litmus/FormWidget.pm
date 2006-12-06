# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Chris Cooper <ccooper@deadsquid.com>
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::FormWidget;
use strict;

BEGIN {
    use Exporter ();
    use vars qw ($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
    $VERSION     = 0.01;
    @ISA         = qw (Exporter);
    #Give a hoot don't pollute, do not export more than needed by default
    @EXPORT      = qw ();
    @EXPORT_OK   = qw ();
    %EXPORT_TAGS = ();
}

use DBI;
use Litmus::DBI;
use Litmus::DB::TestDay;

our $_dbh = Litmus::DBI->db_Main();

#########################################################################
=head1 NAME

Litmus::FormWidget - Create value lists to be used in HTML forms

=head1 SYNOPSIS

  use Litmus::FormWidget

=head1 DESCRIPTION

Litmus::FormWidget creates value lists to be used in HTML forms.

=head1 USAGE

=head1 BUGS

=head1 SUPPORT

=head1 AUTHOR

    Chris Cooper
    CPAN ID: CCOOPER
    Mozilla Corporation
    ccooper@deadsquid.com
    http://litmus.mozilla.org/

=head1 SEE ALSO

perl(1).

=cut

#########################################################################
sub getProducts()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT name, product_id FROM products";
    if ($enabled) {
      $sql .= " WHERE enabled=1";
    }
    $sql .= " ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getUniquePlatforms()
{
    my $sql = "SELECT DISTINCT(name), platform_id FROM platforms ORDER BY name";
    return _getValues($sql);
}

#########################################################################
sub getPlatforms()
{
    my $sql = "SELECT platform_id, name from platforms ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getBranches()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT name, branch_id, product_id FROM branches";
    if ($enabled) {
      $sql .= " WHERE enabled=1";
    }
    $sql .= " ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getUniqueBranches()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT DISTINCT(name) FROM branches";
    if ($enabled) {
      $sql .= " WHERE enabled=1";
    }
    $sql .= " ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getOpsyses()
{
    my $sql = "SELECT name, opsys_id, platform_id FROM opsyses ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getUniqueOpsyses()
{
    my $sql = "SELECT DISTINCT(name) FROM opsyses ORDER BY name ASC";
    return _getValues($sql);
}

#########################################################################
sub getLogTypes()
{
    my $sql = "SELECT DISTINCT(name) FROM log_type_lookup ORDER BY name";
    return _getValues($sql);
}

#########################################################################
sub getTestStatuses()
{
    my @TestStatuses = ({name => 'Enabled'},
                        {name => 'Disabled'});
    return \@TestStatuses;
}

#########################################################################
sub getResultStatuses()
{
    my $sql = "SELECT result_status_id,class_name FROM test_result_status_lookup ORDER BY result_status_id";
    return _getValues($sql);
}

#########################################################################
sub getTestcaseIDs()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT testcase_id FROM testcases";
    if ($enabled) {
      $sql .= " WHERE enabled=1";
    }
    $sql .= " ORDER BY testcase_id";
    return _getValues($sql);
}

#########################################################################
sub getTestcases()
{
    my ($self, $enabled, $sort_by) = @_;
    my $sql = "SELECT testcase_id, summary, product_id, branch_id FROM testcases";
    if ($enabled) {
      $sql .= " WHERE enabled=1";
    }
    
    if (!$sort_by) {
      $sort_by='id';
    }

    if ($sort_by eq 'name') {
      $sql .= " ORDER BY summary ASC, testcase_id ASC";
    } elsif ($sort_by eq 'id') {
      $sql .= " ORDER BY testcase_id ASC";
    } else {
      print STDERR "Unknown sort_by type: $sort_by\n";
    }

    return _getValues($sql);
}

#########################################################################
sub getDistinctSubgroups()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT DISTINCT(sg.subgroup_id), sg.name, sg.product_id, sg.branch_id, sgtg.testgroup_id FROM subgroups sg LEFT JOIN subgroup_testgroups sgtg ON (sg.subgroup_id=sgtg.subgroup_id)";
    if ($enabled) {
      $sql .= " AND sg.enabled=1";
    }
    $sql .= " ORDER BY sgtg.sort_order ASC, sg.name ASC, sg.subgroup_id ASC";
    return _getValues($sql);
}

#########################################################################
sub getSubgroups()
{
    my ($self, $enabled, $sort_by) = @_;
    my $sql = "SELECT sg.subgroup_id, sg.name, sg.product_id, sg.branch_id, sgtg.testgroup_id FROM subgroups sg LEFT JOIN subgroup_testgroups sgtg ON (sg.subgroup_id=sgtg.subgroup_id)";
    if ($enabled) {
      $sql .= " AND sg.enabled=1";
    }

    if (!$sort_by) {
      $sort_by='id';
    }

    if ($sort_by eq 'name') {
      $sql .= " ORDER BY sg.name ASC, sg.subgroup_id ASC";
    } elsif ($sort_by eq 'id') {
      $sql .= " ORDER BY sg.subgroup_id ASC";
    } elsif ($sort_by eq 'sort_order') {
      $sql .= " ORDER BY sgtg.sort_order ASC, sg.name ASC, sg.subgroup_id ASC";
    } else {
      print STDERR "Unknown sort_by type: $sort_by\n";
    }

    return _getValues($sql);
}

#########################################################################
sub getTestgroups()
{
    my ($self, $enabled) = @_;
    my $sql = "SELECT tg.testgroup_id, tg.name, tg.product_id, tg.branch_id FROM testgroups tg";
    if ($enabled) {
      $sql .= " WHERE tg.enabled=1";
    }
    $sql .= " ORDER BY tg.name, tg.testgroup_id";
    return _getValues($sql);
}


#########################################################################
sub getLocales()
{
  my @locales = Litmus::DB::Locale->retrieve_all();
  return \@locales;
}

#########################################################################
sub getUsers()
{
  my @users = Litmus::DB::User->retrieve_all();
  return \@users;
}

#########################################################################
sub getTestdays()
{
  my @testdays = reverse Litmus::DB::TestDay->retrieve_all();
  return \@testdays;
}

#########################################################################
sub getAuthors()
{
  my $sql = "SELECT user_id, email FROM users WHERE is_admin=1 ORDER BY email";
  return _getValues($sql);
}

#########################################################################
sub getTestRuns() {
  my ($self, $enabled) = @_;
  my $sql = "SELECT test_run_id, name FROM test_runs";
  if ($enabled) {
    $sql .= " WHERE enabled=1";
  }
  $sql .= " ORDER BY finish_timestamp DESC, name DESC";
  return _getValues($sql);
}

#########################################################################
sub getFields()
{
    my @fields = (
                  { name => 'build_id',
                    display_string => "Build ID", },
                  { name => 'comment',
                    display_string => "Comments", }, 
                  { name => 'locale',
                    display_string => "Locale", }, 
                  { name => 'opsys',
                    display_string => "Operating System", }, 
                  { name => 'platform',
                    display_string => "Platform", }, 
                  { name => 'product',
                    display_string => "Product", },
                  { name => 'result_status',
                    display_string => "Result Status", },
                  { name => 'subgroup',
                    display_string => "Subgroup", },
                  { name => 'email',
                    display_string => "Submitted By", },
                  { name => 'summary',
                    display_string => "Summary", }, 
                  { name => 'testgroup',
                    display_string => "Testgroup", },
                  { name => 'user_agent',
                    display_string => "User Agent", },
                  );
    return \@fields;
}

#########################################################################
sub getMatchCriteria()
{
    my @match_criteria = (
                          { name => "contains_all",
                            display_string => "contains all of the words/strings" },
                          { name => "contains_any",
                            display_string => "contains any of the words/strings" },
                          { name => "contains",
                            display_string => "contains the word/string" },
                          { name => "contains_case",
                            display_string => "contains the word/string (exact case)" },
                          { name => "not_contain",
                            display_string => "does not contains the word/string" },
                          { name => "not_contain_any",
                            display_string => "does not contains any of the words/string" },
                          { name => "regexp",
                            display_string => "matches the regexp" },
                          { name => "not_regexp",
                            display_string => "does not match the regexp" },
                          );
    return \@match_criteria;
}

#########################################################################
sub getSortFields()
{
    my @sort_fields = (
                       { name => "branch", 
                         display_string => "Branch"},
                       { name => "created", 
                         display_string => "Date"},
                       { name => "locale", 
                         display_string => "Locale"},
                       { name => "platform", 
                         display_string => "Platform"},
                       { name => "product", 
                         display_string => "Product"},
                       { name => "email", 
                         display_string => "Submitted By"},
                       { name => "summary", 
                         display_string => "Summary"},
                       { name => "result_status", 
                         display_string => "Status"},
                       { name => "testcase_id", 
                         display_string => "Testcase ID#"},
                       { name => "testgroup", 
                         display_string => "Testgroup"},
                       );
    return \@sort_fields;
}

#########################################################################
sub _getValues($)
{
    my ($sql) = @_;
    my $sth = $_dbh->prepare($sql);
    $sth->execute();
    my @rows;
    while (my $data = $sth->fetchrow_hashref) {
    push @rows, $data;
    }
    $sth->finish();
    return \@rows;
}

1;
