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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Greg Hendricks.
# Portions created by Greg Hendricks are Copyright (C) 2006
# Greg Hendricks. All Rights Reserved.
#
# Large portions lifted from bugzilla's report.cgi written by 
#                 Gervase Markham <gerv@gerv.net>
#
# Contributor(s): Greg Hendricks <ghendricks@novell.com>

=head1 NAME

Bugzilla::Testopia::Report - Generates report data.

=head1 DESCRIPTION

Reports

=over 

=back

=head1 SYNOPSIS


=cut

package Bugzilla::Testopia::Report;

use strict;

use Bugzilla;
use Bugzilla::Util;
use Bugzilla::Error;
use Bugzilla::Testopia::Util;
use Bugzilla::Testopia::Search;


###############################
####    Initialization     ####
###############################

=head1 METHODS

=head2 new

Instantiates a new report object

=cut

sub new {
    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    my $self = {};
    bless($self, $class);

    $self->init(@_);
 
    return $self;
}

=head2 init

Private constructor for this class

=cut

sub init {
    my $self = shift;
    my ($type, $url, $cgi)  = @_;
    $self->{'type'} = $type ||   ThrowCodeError('bad_arg',
                                   {argument => 'type',
                                    function => 'Testopia::Table::_init'});
    $self->{'url_loc'} = $url;
    $self->{'cgi'} = $cgi;
    my $debug = $cgi->param('debug') if $cgi;
    
    my $col_field = $cgi->param('x_axis_field') || '';
    my $row_field = $cgi->param('y_axis_field') || '';
    my $tbl_field = $cgi->param('z_axis_field') || '';
    
    if (!($col_field || $row_field || $tbl_field)) {
        ThrowUserError("no_axes_defined");
    }
    
    my $width = $cgi->param('width');
    my $height = $cgi->param('height');
    
    if (defined($width)) {
       (detaint_natural($width) && $width > 0)
         || ThrowCodeError("invalid_dimensions");
       $width <= 2000 || ThrowUserError("chart_too_large");
    }
    
    if (defined($height)) {
       (detaint_natural($height) && $height > 0)
         || ThrowCodeError("invalid_dimensions");
       $height <= 2000 || ThrowUserError("chart_too_large");
    }
    
    # These shenanigans are necessary to make sure that both vertical and 
    # horizontal 1D tables convert to the correct dimension when you ask to
    # display them as some sort of chart.
    if (defined $cgi->param('format') && $cgi->param('format') eq "table") {
        if ($col_field && !$row_field) {    
            # 1D *tables* should be displayed vertically (with a row_field only)
            $row_field = $col_field;
            $col_field = '';
        }
    }
    else {
        if ($row_field && !$col_field) {
            # 1D *charts* should be displayed horizontally (with an col_field only)
            $col_field = $row_field;
            $row_field = '';
        }
    }
    
    my %columns;
    if ($type eq 'case'){
        $columns{'case_status'}     = "map_case_status.name";        
        $columns{'priority'}        = "map_priority.value";
        $columns{'product'}         = "map_case_product.name";
        $columns{'component'}       = "map_case_components.name";
        $columns{'category'}        = "map_categories.name";
        $columns{'isautomated'}     = "test_cases.isautomated";
        $columns{'tags'}            = "map_case_tags.tag_name";
        $columns{'requirement'}     = "test_cases.requirement";
        $columns{'author'}          = "map_case_author.login_name";
        $columns{'default_tester'}  = "map_default_tester.login_name";
    }
    elsif ($type eq 'run'){
        $columns{'run_status'}      = "test_runs.close_date";        
        $columns{'product'}         = "map_run_product.name";
        $columns{'build'}           = "map_run_build.name";
        $columns{'milestone'}       = "map_run_milestone.milestone";
        $columns{'environment'}     = "map_run_environment.name";
        $columns{'tags'}            = "map_run_tags.tag_name";
        $columns{'manager'}         = "map_run_manager.login_name";
        $columns{'default_product_version'} = "test_runs.product_version";
    }
    elsif ($type eq 'plan'){
        $columns{'plan_type'}       = "map_plan_type.name";        
        $columns{'product'}         = "map_plan_product.name";
        $columns{'archived'}        = "test_plans.isactive";
        $columns{'tags'}            = "map_plan_tags.tag_name";
        $columns{'author'}          = "map_plan_author.login_name";
        $columns{'default_product_version'} = "test_plans.default_product_version";        
    }
    elsif ($type eq 'caserun'){
        $columns{'build'}           = "map_caserun_build.name";        
        $columns{'case'}            = "map_caserun_case.summary";
        $columns{'run'}             = "map_caserun_run.summary";
        $columns{'environment'}     = "map_caserun_environment.name";
        $columns{'assignee'}        = "map_caserun_assignee.login_name";
        $columns{'testedby'}        = "map_caserun_testedby.login_name";
        $columns{'status'}          = "map_caserun_status.name";
        $columns{'milestone'}       = "map_caserun_milestone.milestone";
        $columns{'case_tags'}       = "map_caserun_case_tags.tag_name";
        $columns{'run_tags'}        = "map_caserun_run_tags.tag_name";
        $columns{'requirement'}     = "map_caserun_cases.requirement";
        $columns{'priority'}        = "map_caserun_priority.value";
        $columns{'default_tester'}  = "map_caserun_default_tester.login_name";
        $columns{'category'}        = "map_caserun_category.name";
        $columns{'componnet'}       = "map_caserun_components.name";
    }
    # One which means "nothing". Any number would do, really. It just gets SELECTed
    # so that we always select 3 items in the query.
    $columns{''}                 = "42217354";
    
    # Validate the values in the axis fields or throw an error.
    !$row_field 
      || ($columns{$row_field} && trick_taint($row_field))
      || ThrowCodeError("report_axis_invalid", {fld => "x", val => $row_field});
    !$col_field 
      || ($columns{$col_field} && trick_taint($col_field))
      || ThrowCodeError("report_axis_invalid", {fld => "y", val => $col_field});
    !$tbl_field 
      || ($columns{$tbl_field} && trick_taint($tbl_field))
      || ThrowCodeError("report_axis_invalid", {fld => "z", val => $tbl_field});
    
    my @axis_fields = ($row_field, $col_field, $tbl_field);
    my @selectnames = map($columns{$_}, @axis_fields);
    $self->{'axis_fields'} = \@axis_fields;
    $self->{'selectnames'} = \@selectnames;
    $cgi->param('viewall', 1);
    
    my $dbh = Bugzilla->switch_to_shadow_db;
    my $search = Bugzilla::Testopia::Search->new($cgi, \@selectnames);
    my $results = $dbh->selectall_arrayref($search->query);
    Bugzilla->switch_to_main_db;
    
    # We have a hash of hashes for the data itself, and a hash to hold the 
    # row/col/table names.
    my %data;
    my %names;
    
    # Read the bug data and count the bugs for each possible value of row, column
    # and table.
    #
    # We detect a numerical field, and sort appropriately, if all the values are
    # numeric.
    my $col_isnumeric = 1;
    my $row_isnumeric = 1;
    my $tbl_isnumeric = 1;
    
    foreach my $result (@$results) {
        my ($row, $col, $tbl) = @$result;
    
        # handle empty dimension member names
        $row = ' ' if ($row eq '');
        $col = ' ' if ($col eq '');
        $tbl = ' ' if ($tbl eq '');
    
        $row = "" if ($row eq $columns{''});
        $col = "" if ($col eq $columns{''});
        $tbl = "" if ($tbl eq $columns{''});
        
        # account for the fact that names may start with '_' or '.'.  Change this 
        # so the template doesn't hide hash elements with those keys
        $row =~ s/^([._])/ $1/;
        $col =~ s/^([._])/ $1/;
        $tbl =~ s/^([._])/ $1/;
    
        $data{$tbl}{$col}{$row}++;
        $names{"col"}{$col}++;
        $names{"row"}{$row}++;
        $names{"tbl"}{$tbl}++;
        
        $col_isnumeric &&= ($col =~ /^-?\d+(\.\d+)?$/o);
        $row_isnumeric &&= ($row =~ /^-?\d+(\.\d+)?$/o);
        $tbl_isnumeric &&= ($tbl =~ /^-?\d+(\.\d+)?$/o);
    }
    
    my @col_names = @{get_names($names{"col"}, $col_isnumeric, $col_field)};
    my @row_names = @{get_names($names{"row"}, $row_isnumeric, $row_field)};
    my @tbl_names = @{get_names($names{"tbl"}, $tbl_isnumeric, $tbl_field)};

    # The GD::Graph package requires a particular format of data, so once we've
    # gathered everything into the hashes and made sure we know the size of the
    # data, we reformat it into an array of arrays of arrays of data.
    push(@tbl_names, "-total-") if (scalar(@tbl_names) > 1);
        
    my @image_data;
    foreach my $tbl (@tbl_names) {
        my @tbl_data;
        push(@tbl_data, \@col_names);
        foreach my $row (@row_names) {
            my @col_data;
            foreach my $col (@col_names) {
                $data{$tbl}{$col}{$row} = $data{$tbl}{$col}{$row} || 0;
                push(@col_data, $data{$tbl}{$col}{$row});
                if ($tbl ne "-total-") {
                    # This is a bit sneaky. We spend every loop except the last
                    # building up the -total- data, and then last time round,
                    # we process it as another tbl, and push() the total values 
                    # into the image_data array.
                    $data{"-total-"}{$col}{$row} += $data{$tbl}{$col}{$row};
                }
            }
    
            push(@tbl_data, \@col_data);
        }
        
        unshift(@image_data, \@tbl_data);
    }
    $self->{'col_field'} = $col_field;
    $self->{'row_field'} = $row_field;
    $self->{'tbl_field'} = $tbl_field;
    
    my @time = localtime(time());
    my $date = sprintf "%04d-%02d-%02d", 1900+$time[5],$time[4]+1,$time[3];
    $self->{'date'} = $date;
    $self->{'format'} = $cgi->param('format');
    
    $self->{'col_names'} = \@col_names;
    $self->{'row_names'} = \@row_names;
    $self->{'tbl_names'} = \@tbl_names;
                
    # Below a certain width, we don't see any bars, so there needs to be a minimum.
    if ($width && $cgi->param('format') eq "bar") {
        my $min_width = (scalar(@col_names) || 1) * 20;
    
        if (!$cgi->param('cumulate')) {
            $min_width *= (scalar(@row_names) || 1);
        }
    
        $self->{'min_width'} = $min_width;
    }
    
    $self->{'width'} = $width if $width;
    $self->{'height'} = $height if $height;
    
    $self->{'query'} = $search->query;
    $self->{'debug'} = $cgi->param('debug');
    
    $self->{'data'} = \%data;
    $self->{'image_data'} = \@image_data;
    $self->{'report_loc'} = "tr_" . $type . "_reports.cgi";
    if ($cgi->param('debug')) {
        print $cgi->header;
        require Data::Dumper;
        print "<pre>data hash:\n";
        print Data::Dumper::Dumper(%data) . "\n\n";
        print "data array:\n";
        print Data::Dumper::Dumper(@image_data) . "\n\n</pre>";
    }

    return $self;
}

sub get_names {
    my ($names, $isnumeric, $field) = @_;
  
    my @sorted;
    
    if ($isnumeric) {
        # It's not a field we are preserving the order of, so sort it 
        # numerically...
        sub numerically { $a <=> $b }
        @sorted = sort numerically keys(%{$names});
    } else {
        # ...or alphabetically, as appropriate.
        @sorted = sort(keys(%{$names}));
    }
  
    return \@sorted;
}
sub listbase{
    my $self = shift;
    my $cgi = $self->{'cgi'};
    $self->{'listbase'} = $cgi->canonicalise_query(
                                 "x_axis_field", "y_axis_field", "z_axis_field",
                               "ctype", "format", "query_format", "report_action",  @{$self->{'axis_fields'}});
    return $self->{'listbase'};
}

sub imagebase {
    my $self = shift;
    my $cgi = $self->{'cgi'};
    $self->{'imagebase'}   = $cgi->canonicalise_query( 
                    $self->{'tbl_field'}, "report_action", "ctype", "format", "width", "height");
    return $self->{'imagebase'};
}

sub switchbase {
    my $self = shift;
    my $cgi = $self->{'cgi'};
    $self->{'switchbase'}  = $cgi->canonicalise_query( 
                "query_format", "report_action", "ctype", "format", "width", "height");
    return $self->{'switchbase'};
}

1;