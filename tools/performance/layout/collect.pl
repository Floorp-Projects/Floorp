#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


%::Sites = ();

@::Categories = (
    Content,
    Reflow,
    Frame,
    Style,
    Parse,
    DTD,
    Tokenize,
    Total
    );

$::CurrentSite = "";

LINE: while (<>) {
    if (/^\*\*\* Timing layout processes on url: '(.*)'/) {
        # This will generate a URL that looks something like:
        #
        #   file:///foo/bar/website/index.html
        #
        # So, we'll parse out the ``website'' part...
        my @parts = split('/', $1);
        $::CurrentSite = $parts[$#parts - 1];

        if (! $::Sites{$::CurrentSite}) {
            $::Sites{$::CurrentSite} = {};
        }
        next LINE;
    }

    next LINE unless $::CurrentSite;

    CATEGORY: foreach $category (@::Categories) {
        next CATEGORY unless (/$category/);

        # The "real time" is indicated in HH:MM:SS.mmmm format
        my ($h,$m,$s,$ms) = /Real time (..):(..):(..)\.(....)/;
        my $real = $ms;
        $real += $s * 1000;
        $real += $m * 1000 * 60;
        $real += $h * 1000 * 60 * 60;

        # The "CPU time" is indicated in m.uuu format
        my ($cpu) = /CP time (.*\....)/;

        my $site = $::Sites{$::CurrentSite};
        if (! $site->{$category}) {
            $site->{$category} = { Count => 0, Real => 0, CPU => 0, Real2 => 0, CPU2 => 0 };
        }

        $site->{$category}->{Count} += 1;
        $site->{$category}->{Real}  += $real;
        $site->{$category}->{CPU}   += $cpu;
        $site->{$category}->{Real2} += $real * $real;
        $site->{$category}->{CPU2}  += $cpu * $cpu;
    }
}

my $bgcolor0 = '#999999';
my $bgcolor1 = '#777777';

my $bgcolor;

print "<table border='0' cellpadding='2' cellspacing='0'>\n";

print "<tr>\n";
print "  <td rowspan='2' valign='bottom'>Site</td>\n";

$bgcolor = $bgcolor0;
foreach $category (@::Categories) {
    print "  <td bgcolor='$bgcolor' align='center' colspan='4'>$category</td>\n";
    $bgcolor = ($bgcolor eq $bgcolor0) ? $bgcolor1 : $bgcolor0;
}
print "</tr>\n";

print "<tr>\n";

$bgcolor = $bgcolor0;
foreach $category (@::Categories) {
    print "  <td bgcolor='$bgcolor' align='center' colspan='2'>CPU</td> <td bgcolor='$bgcolor' align='center' colspan='2'>Real</td>\n";
    $bgcolor = ($bgcolor eq $bgcolor0) ? $bgcolor1 : $bgcolor0;
}
print "</tr>\n";

foreach $sitename (sort(keys(%::Sites))) {
    print "<tr>\n";

    my $site = $::Sites{$sitename};
    print "  <td>$sitename</td>\n";

    $bgcolor = $bgcolor0;
    foreach $category (@::Categories) {
        my $count = $site->{$category}->{Count};
        my $real  = $site->{$category}->{Real};
        my $cpu   = $site->{$category}->{CPU};
        my $real2 = $site->{$category}->{Real2};
        my $cpu2  = $site->{$category}->{CPU2};

        my $realdev = 0;
        my $cpudev = 0;

        if ($count) {
            if ($count > 1) {
                $realdev = sqrt( ( $real2 * $count - $real * $real ) / ( $count * ( $count - 1 ) ) );
                $cpudev  = sqrt( ( $cpu2 * $count - $cpu * $cpu ) / ( $count * ( $count - 1 ) ) );
            }

            $real /= $count;
            $cpu /= $count;
        }
        else {
            $count = 0;
            $real = 0;
            $cpu = 0;
        }

        printf "  <td bgcolor='$bgcolor' align='right'>%0.2lf</td><td bgcolor='$bgcolor' align='left'>&plusmn;%0.2lf</td>\n", $cpu, $cpudev;
        printf "  <td bgcolor='$bgcolor' align='right'>%0.2lf</td><td bgcolor='$bgcolor' align='left'>&plusmn;%0.2lf</td>", $real, $realdev;

        $bgcolor = ($bgcolor eq $bgcolor0) ? $bgcolor1 : $bgcolor0;
    }

    print "</tr>\n";
}

print "</table>\n";

