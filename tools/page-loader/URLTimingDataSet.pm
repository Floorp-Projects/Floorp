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
package URLTimingDataSet;
use DBI;
use PageData;       # list of test pages, etc.
use strict;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {
        dataset   => [],
        results   => [],
        sorted    => [],
        average   => undef,
        avgmedian => undef,    # note: average of individual medians
        maximum   => undef,
        minimum   => undef,
    };
    $self->{id}    = shift || die "No id supplied"; 
    $self->{table} = shift || "t" . $self->{id};
    $self->{pages} = PageData->new;
    bless ($self, $class);
    $self->_grok();
    return $self;
}


sub _grok {    
    my $self = shift;
    my ($res);

    # select the dataset from the db
    $self->_select();

    for (my $i=0; $i < $self->{pages}->length; $i++) {
        my $name = $self->{pages}->name($i);
        my $count = 0;
        my @times = (); 
        my $nan = 0;
        foreach my $ref (@{$self->{dataset}}) {
            next if ($name ne $ref->{content});
            $count++;
            if ($ref->{c_part} eq "NaN") { 
                # we bailed out of this page load
                $res = "NaN";
                $nan = 1;
            }
            else {
                my $s_intvl = $ref->{s_intvl};
                my $c_intvl = $ref->{c_intvl};
                my $errval = abs($s_intvl-$c_intvl)/(($s_intvl+$c_intvl)/2);
                if ($errval > 0.08) {       # one of them went wrong and stalled out (see [1] below)
                    $res = ($s_intvl <= $c_intvl) ? $s_intvl : $c_intvl;
                } else {
                    $res = int(($s_intvl + $c_intvl)/2);
                }
            }
            push @times, $res;
        }

        my $avg = int(_avg(@times));
        my $med = _med(@times);
        my $max = $nan ? "NaN" : _max(@times);
        my $min = _min(@times);
        push @{$self->{results}}, [ $i, $name, $count, $avg, $med, $max, $min, @times ];
    }
    
    $self->_get_summary();
    $self->_sort_result_set();
    
}

sub _select {
    my $self = shift;

    my $dbh = DBI->connect("DBI:CSV:f_dir=./db", {RaiseError => 1, AutoCommit => 1})
         or die "Cannot connect: " . $DBI::errstr;

    my $sql = qq{
        SELECT INDEX, S_INTVL, C_INTVL, C_PART, CONTENT, ID 
             FROM $self->{table}
                  WHERE ID = '$self->{id}'
                };
 
    my $sth = $dbh->prepare($sql);
    $sth->execute();

    while (my @data = $sth->fetchrow_array()) {
        push @{$self->{dataset}}, 
        {index   => $data[0],
         s_intvl => $data[1],
         c_intvl => $data[2],
         c_part  => $data[3],
         content => $data[4],
         id      => $data[5]
          };
    }
    $sth->finish();
    $dbh->disconnect();
}

sub _get_summary {
    my $self = shift;
    my (@avg, @med, @max, @min);

    # how many pages were loaded in total ('sampled')
    $self->{samples} = scalar(@{$self->{dataset}}); 

    # how many cycles (should I get this from test parameters instead?)
    $self->{count} = int(_avg( map($_->[2],  @{$self->{results}}) ));
    #warn $self->{count};

    # calculate overall average, average median, maximum, minimum, (RMS Error?)
    for (@{$self->{results}}) {
        push @avg, $_->[3];
        push @med, $_->[4];
        push @max, $_->[5];
        push @min, $_->[6];
    }
    $self->{average}   = int(_avg(@avg));
    $self->{avgmedian} = int(_avg(@med));     # note: averaging individual medians
    $self->{maximum}   = _max(@max);
    $self->{minimum}   = _min(@min);
}

sub _sort_result_set {
    my $self = shift;
    # sort by median load time
    # @{$self->{sorted}} = sort {$a->[4] <=> $b->[4]} @{$self->{results}};
    # might be "NaN", but this is lame of me to be carrying around a string instead of undef
    @{$self->{sorted}} = 
         sort {
             if ($a->[4] eq "NaN" || $b->[4] eq "NaN") {
                 return $a->[4] cmp $b->[4]; 
             } else {
                 return $a->[4] <=> $b->[4];
             }
         } @{$self->{results}};
}

sub as_string {
    my $self = shift;
    return $self->_as_string();
}

sub as_string_sorted {
    my $self = shift;
    return $self->_as_string(@{$self->{sorted}});
}


sub _as_string {
    my $self = shift;
    my @ary = @_ ? @_ : @{$self->{results}};
    my $str;
    for (@ary) {
        my ($index, $path, $count, $avg, $med, $max, $min, @times) = @$_;
        $str .= sprintf "%3s %-26s\t", $index, $path;
        if ($count > 0) {
            $str .= sprintf "%6s %6s %6s %6s ", $avg, $med, $max, $min;
            foreach my $time (@times) {
                $str .= sprintf "%6s ", $time;
            }
        }
        $str .= "\n";
    }
    return $str;
}
    
#
# package internal helper functions
#
sub _num {
    my @array = ();
    for (@_) { push @array, $_ if /^[+-]?\d+\.?\d*$/o; }
    return @array;
}

sub _avg {
    my @array = _num(@_);
    return "NaN" unless scalar(@array);
    my $sum = 0;
    for (@array) { $sum += $_; }
    return $sum/scalar(@array);
}

sub _max {
    my @array = _num(@_);
    return "NaN" unless scalar(@array);
    my $max = $array[0];
    for (@array) { $max = ($max > $_) ? $max : $_; }
    return $max;
}

sub _min {
    my @array = _num(@_);
    return "NaN" unless scalar(@array);
    my $min = $array[0];
    for (@array) { $min = ($min < $_) ? $min : $_; }
    return $min;
}

# returns the floor(N/2) element of a sorted ascending array
sub _med {
    my @array = _num(@_);
    return "NaN" unless scalar(@array);
    my $index = int((scalar(@array)-1)/2);
    @array = sort {$a <=> $b} @array;
    return $array[$index];
}

1; # return true

################################################################################
#
# [1] in looking at the test results, in almost all cases, the
# round-trip time measured by the server logic and the client logic
# would be almost the same value (which is what one would
# expect). However, on occasion, one of the them would be "out of
# whack", and inconsistent with the additional "layout" measure by the
# client.
#
#    i.e., a set of numbers like these:
#      c_part     c_intvl      s_intvl
#      800      1003        997
#      804      1007        1005
#      801      1001        1325             <--
#      803      1318        998              <--
#      799      1002        1007
#      ...
#
# which looks like the server side would stall in doing the accept or
# in running the mod-perl handler (possibly a GC?). (The following
# c_intvl would then be out of whack by a matching amount on the next
# cycle).
#
# At any rate, since it was clear from comparing with the 'c_part'
# measure, which of the times was bogus, I just use an arbitrary error
# measure to determine when to toss out the "bad" value.
#
