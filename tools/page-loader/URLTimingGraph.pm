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
# The Original Code is Mozilla page-loader test, released Aug 5, 2001
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2001 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    John Morrison <jrgm@netscape.com>, original author
#    
#    
package URLTimingGraph;
use strict;
use GD;
use GD::Graph::linespoints;
use GD::Graph::points;
use GD::Graph::lines;
use GD::Graph::mixed;
use GD::Graph::colour;
use GD::Graph::Data;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    bless ($self, $class);
    $self->{data} = shift || die "No data.";
    my $args  = shift || {};
    $self->{cgimode} = $args->{cgimode} || 0;
    $self->{title}   = $args->{title}   || ""; 
    $self->{types}   = $args->{types}   || ['lines', undef, undef, undef, undef, undef, undef]; 
    $self->{dclrs}   = $args->{dclrs}   || [qw(lred)];
    $self->{legend}  = $args->{legend}  || [qw(undef)];
    $self->{y_max_value} = $args->{y_max_value} || 10000;
    $self->{width}   = $args->{width}   || 800;
    $self->{height}  = $args->{height}  || 720;
    return $self;
}

sub _set_standard_options {
    my $self = shift;
    $self->{graph}->set( 
                 x_label => '',
                 y_label => 'Page Load Time (msec)',
                 default_type => 'points',
                 x_labels_vertical => 1,
                 y_long_ticks => 1,
                 x_tick_length => 8,
                 x_long_ticks => 0,
                 line_width => 2,
                 marker_size => 3,
                 markers => [8],
                 show_values => 0,
                 transparent => 0,
                 interlaced => 1,
                 skip_undef => 1,
                 ) 
         || warn $self->{graph}->error;
    $self->{graph}->set_title_font(GD::Font->Giant);
    $self->{graph}->set_x_label_font(GD::Font->Large);
    $self->{graph}->set_y_label_font(GD::Font->Large);
    $self->{graph}->set_x_axis_font(GD::Font->Large);
    $self->{graph}->set_y_axis_font(GD::Font->Large);
    $self->{graph}->set_legend_font(GD::Font->Giant);
}

sub plot {
    my $self = shift;
    $self->{graph} = new GD::Graph::mixed($self->{width}, 
                                          $self->{height});
    $self->_set_standard_options();

    $self->{graph}->set(title => $self->{title},
                        types => $self->{types},
                        y_max_value => $self->{y_max_value},
                        dclrs => $self->{dclrs},
                        ) 
         || warn $self->{graph}->error;

    $self->{graph}->set_legend( @{$self->{legend}} );

    # draw the graph image
    $self->{graph}->plot($self->{data}) || 
         die $self->{graph}->error;

    # send it back to stdout (or browser)
    print "Content-type: image/png\n\n" if $self->{cgimode};
    binmode STDOUT;
    print $self->{graph}->gd->png();
}


1; #return true
