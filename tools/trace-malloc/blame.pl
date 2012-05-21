#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# Process output of TraceMallocDumpAllocations() to produce a table
# that attributes memory to the allocators using call stack.
#

use 5.004;
use strict;

# A table of all ancestors. Key is function name, value is an
# array of ancestors, each attributed with a number of calls and
# the amount of memory allocated.
my %Ancestors;

# Ibid, for descendants.
my %Descendants;

# A table that keeps the total amount of memory allocated by each
# function
my %Totals;
$Totals{".root"} = { "#memory#" => 0, "#calls#" => 0 };

# A table that maps the long ugly function name to a unique number so
# that the HTML we generate isn't too fat
my %Ids;
my $NextId = 0;

$Ids{".root"} = ++$NextId;


LINE: while (<>) {
    # The line'll look like:
    #
    #  0x4000a008     16  PR_Malloc+16; nsMemoryImpl::Alloc(unsigned int)+12; ...

    # Ignore any lines that don't start with an address
    next LINE unless /^0x/;

    # Parse it
    my ($address, $size, $rest) = /^(0x\S*)\s*(\d+)\s*(.*)$/;
    my @stack = reverse(split /; /, $rest);

    # Accumulate at the root
    $Totals{".root"}->{"#memory#"} += $size;
    ++$Totals{".root"}->{"#calls#"};

    my $caller = ".root";
    foreach my $callee (@stack) {
        # Strip the offset from the callsite information. I don't
        # think we care.
        $callee =~ s/\+\d+$//g;

        # Accumulate the total for the callee
        if (! $Totals{$callee}) {
            $Totals{$callee} = { "#memory#" => 0, "#calls#" => 0 };
        }

        $Totals{$callee}->{"#memory#"} += $size;
        ++$Totals{$callee}->{"#calls#"};

        # Descendants
        my $descendants = $Descendants{$caller};
        if (! $descendants) {
            $descendants = $Descendants{$caller} = [ ];
        }

        # Manage the list of descendants
        {
            my $wasInserted = 0;
          DESCENDANT: foreach my $item (@$descendants) {
                if ($item->{"#name#"} eq $callee) {
                    $item->{"#memory#"} += $size;
                    ++$item->{"#calls#"};
                    $wasInserted = 1;
                    last DESCENDANT;
                }
            }

            if (! $wasInserted) {
                $descendants->[@$descendants] = {
                    "#name#"   => $callee,
                    "#memory#" => $size,
                    "#calls#"  => 1
                };
            }
        }

        # Ancestors
        my $ancestors = $Ancestors{$callee};
        if (! $ancestors) {
            $ancestors = $Ancestors{$callee} = [ ];
        }

        # Manage the list of ancestors
        {
            my $wasInserted = 0;
          ANCESTOR: foreach my $item (@$ancestors) {
                if ($item->{"#name#"} eq $caller) {
                    $item->{"#memory#"} += $size;
                    ++$item->{"#calls#"};
                    $wasInserted = 1;
                    last ANCESTOR;
                }
            }

            if (! $wasInserted) {
                $ancestors->[@$ancestors] = {
                    "#name#"   => $caller,
                    "#memory#" => $size,
                    "#calls#"  => 1
                };
            }
        }

        # Make a new "id", if necessary
        if (! $Ids{$callee}) {
            $Ids{$callee} = ++$NextId;
        }

        # On to the next one...
        $caller = $callee;
    }
}


# Change the manky looking callsite into a pretty function; strip argument
# types and offset information.
sub pretty($) {
    $_ = $_[0];
    s/&/&amp;/g;
    s/</&lt;/g;
    s/>/&gt;/g;

    if (/([^\(]*)(\(.*\))/) {
        return $1 . "()";
    }
    else {
        return $_[0];
    }
}

# Dump a web page!
print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\">\n";
print "<html><head>\n";
print "<title>Live Bloat Blame</title>\n";
print "<link rel=\"stylesheet\" type=\"text/css\" href=\"blame.css\">\n";
print "</head>\n";
print "<body>\n";

# At most 100 rows per table so as not to kill the browser.
my $maxrows = 100;

print "<table>\n";
print "<thead><tr><td>Function</td><td>Ancestors</td><td>Descendants</td></tr></thead>\n";

foreach my $node (sort(keys(%Ids))) {
    print "<tr>\n";

    # Print the current node
    {
        my ($memory, $calls) =
            ($Totals{$node}->{"#memory#"},
             $Totals{$node}->{"#calls#"});

        my $pretty = pretty($node);
        print "  <td><a name=\"$Ids{$node}\">$pretty&nbsp;$memory&nbsp;($calls)</a></td>\n";
    }

    # Ancestors, sorted descending by amount of memory allocated
    print "  <td>\n";
    my $ancestors = $Ancestors{$node};
    if ($ancestors) {
        foreach my $ancestor (sort { $b->{"#memory#"} <=> $a->{"#memory#"} } @$ancestors) {
            my ($name, $memory, $calls) =
                ($ancestor->{"#name#"},
                 $ancestor->{"#memory#"},
                 $ancestor->{"#calls#"});

            my $pretty = pretty($name);

            print "    <a href=\"#$Ids{$name}\">$pretty</a>&nbsp;$memory&nbsp;($calls)<br>\n";
        }
    }

    print "  </td>\n";

    # Descendants, sorted descending by amount of memory allocated
    print "  <td>\n";
    my $descendants = $Descendants{$node};
    if ($descendants) {
        foreach my $descendant (sort { $b->{"#memory#"} <=> $a->{"#memory#"} } @$descendants) {
            my ($name, $memory, $calls) =
                ($descendant->{"#name#"},
                 $descendant->{"#memory#"},
                 $descendant->{"#calls#"});

            my $pretty = pretty($name);

            print "    <a href=\"#$Ids{$name}\">$pretty</a>&nbsp;$memory&nbsp;($calls)<br>\n";
        }
    }
    print "  </td></tr>\n";

    if (--$maxrows == 0) {
        print "</table>\n";
        print "<table>\n";
        print "<thead><tr><td>Function</td><td>Ancestors</td><td>Descendants</td></tr></thead>\n";
        $maxrows = 100;
    }
}

# Footer
print "</table>\n";
print "</body></html>\n";
