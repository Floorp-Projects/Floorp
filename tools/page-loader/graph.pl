#!/usr/bin/perl
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla page-loader test, released Aug 5, 2001.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   John Morrison <jrgm@netscape.com>, original author
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
use CGI::Carp qw(fatalsToBrowser);
use CGI::Request;
use URLTimingDataSet;
use URLTimingGraph;

my $request = new CGI::Request;

my $id  = $request->param('id'); #XXX need to check for valid parameter id
my $id2 = $request->param('id2') || undef; # possible comparison test

# set up the data for the first graph
my $rs = URLTimingDataSet->new($id);
my @data = ();
push @data,  [ map($_->[1], @{$rs->{sorted}}) ];  # URL 
push @data,  [ map($_->[4], @{$rs->{sorted}}) ];  # median
# '7' is the first slot for individual test run data
for (my $idx = 7; $idx < (7+$rs->{count}); $idx++) { 
    push @data,  [ map($_->[$idx], @{$rs->{sorted}}) ];
}


# set up the data for the second graph, if requested a second id
# need to sort according to the first chart's ordering
my $rs2;
if ($id2) {
    $rs2 = URLTimingDataSet->new($id2);
    my @order = map($_->[0], @{$rs->{sorted}});  # get the first chart's order
    my @resort = ();
    for my $i (@order) {
        for (@{$rs2->{sorted}}) {
            if ($i == $_->[0]) {
                push @resort, $_;
                last;
            }
        }
    }
    push @data,  [ map($_->[4], @resort) ];  # median
    for (my $idx = 7; $idx < (7+$rs2->{count}); $idx++) { 
        push @data,  [ map($_->[$idx], @resort) ];
    }
}

# and now convert 'NaN' to undef, if they exist in the data.
for (@data) { for (@$_) { $_ = undef if $_ eq "NaN"; } }

# set up the chart parameters
my $args = {};
$args->{cgimode} = 1;
$args->{title}   = "id=$id";

# need to draw first visit as dotted with points
my $types = ['lines','lines']; for (1..$rs->{count}-1) { push @$types, undef;  } 
my $dclrs = [];        for (0..$rs->{count}) { push @$dclrs, 'lred'; } 
my $legend = [$id];    for (1..$rs->{count}) { push @$legend, undef;  } 
if ($id2) {
    push @$types, 'lines'; for (1..$rs2->{count}) { push @$types, undef;  } 
    for (0..$rs2->{count}) { push @$dclrs, 'lblue'; } 
    push @$legend, $id2;  for (1..$rs2->{count}) { push @$legend, undef; } 
}
$args->{types}   = $types; 
$args->{dclrs}   = $dclrs;
$args->{legend}  = $legend;

#XXX set min to zero, and round max to 1000
$args->{y_max_value} = maxDataOrCap();
## nope $args->{y_min_value} = 1000;
$args->{width}   = 800;
$args->{height}  = 720;

my $g = URLTimingGraph->new(\@data, $args);
$g->plot();

exit;


sub maxDataOrCap {
    my $max;
    warn $rs->{maximum};
    if ($rs2 && ($rs->{maximum} < $rs2->{maximum})) {
        $max = $rs2->{maximum}; 
    } else {
        $max = $rs->{maximum}; 
    }
    warn $max;
    #return $max > 10000 ? 10000 : 1000*int($max/1000)+1000;
    # just return whatever, rounded to 1000
    return 1000*int($max/1000)+1000;
}
