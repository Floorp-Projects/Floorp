#!/usr/bin/perl -w
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is leak-soup.pl, released Oct 1, 2000.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    Chris Waterson <waterson@netscape.com>
#
#
# A perl version of Patrick Beard's ``Leak Soup'', which processes the
# stack crawls from the Boehm GC into a graph.
#

use 5.004;
use strict;
use Getopt::Long;
use FileHandle;
use IPC::Open2;

# Collect program options
$::opt_help = 0;
$::opt_format = "boehm";
$::opt_nostacks = 0;
$::opt_nochildstacks = 0;
$::opt_depth = 9999;
$::opt_noentrained = 0;
$::opt_noslop = 0;

GetOptions("help", "format=s", "nostacks", "nochildstacks", "depth=i", "noentrained", "noslop");

if ($::opt_help) {
    die "usage: leak-soup.pl [options] <leakfile>
  --help          Display this message
  --format=[boehm*|trace-malloc]
                  Parse input as if from boehm (default) or trace-malloc
  --nostacks      Do not compute stack traces
  --nochildstacks Do not compute stack traces for entrained objects
  --depth=<max>   Only compute stack traces to depth of <max>
  --noentrained   Do not compute amount of memory entrained by root objects
  --noslop        Don't ignore low bits when searching for pointers";
}

# This is the table that keeps a graph of objects. It's indexed by the
# object's address (as an integer), and refers to a simple hash that
# has information about the object's type, size, slots, and allocation
# stack.
$::Objects = { };


#----------------------------------------------------------------------
#
# Read in the output from the Boehm GC. 
#
sub read_boehm() {
  OBJECT: while (<>) {
      # e.g., 0x0832FBD0 <void*> (80)
      next OBJECT unless /^0x(\S+) <(.*)> \((\d+)\)/;
      my ($addr, $type, $size) = (hex $1, $2, $3);

      my $object = $::Objects{$addr};
      if (! $object) {
          # Found a new object entry. Record its type and size
          $::Objects{$addr} =
              $object =
              { 'type' => $type, 'size' => $size };
      }

      # Record the object's slots
      my @slots;

    SLOT: while (<>) {
        # e.g.,      0x00000000
        last SLOT unless /^\t0x(\S+)/;
        my $value = hex $1;

        # Ignore low bits, unless they've specified --noslop
        $value &= ~0x7 unless $::opt_noslop;

        $slots[$#slots + 1] = $value;
    }

      $object->{'slots'} = \@slots;

      if (! $::opt_nostacks) {
          # Record the stack by which the object was allocated
          my @stack;

        FRAME: while (<>) {
            # e.g., _dl_debug_message[/lib/ld-linux.so.2 +0x0000B858]
            last FRAME unless /^(.*)\[(.*) \+0x(\S+)\]$/;
            my ($func, $lib, $off) = ($1, $2, hex $3);

            chomp;
            $stack[$#stack + 1] = $_;
        }

          $object->{'stack'} = \@stack;
      }

      # Gotta check EOF explicitly...
      last OBJECT if eof;
  }
}


#----------------------------------------------------------------------
#
# Read output from trace-malloc
#
sub read_trace_malloc() {
  OBJECT: while (<>) {
      # One record per line, data separated by `;'
      my @data = split ';';

      my $addr = hex shift @data;
      my $type = shift @data;
      my $size = shift @data;

      my $object = $::Objects{$addr};
      if (! $object) {
          # Found a new object entry. Record its type and size
          $::Objects{$addr} =
              $object =
              { 'type' => $type, 'size' => $size };
      }

      # Record the object's slots
      my @slots;

    SLOT: while (1) {
        my $value = shift @data;

        # The slots are terminated by a field with character `@'
        last SLOT if $value eq '@';

        $value = hex $value;

        # Ignore low bits, unless they've specified --noslop
        $value &= ~0x7 unless $::opt_noslop;

        $slots[$#slots + 1] = $value;
    }

      $object->{'slots'} = \@slots;

      # Record the stack by which the object was allocated
      if (! $::opt_nostacks) {
          my @stack;

        FRAME: while (1) {
            my $frame = shift @data;
            last FRAME unless $frame;
            $stack[$#stack + 1] = $frame;
        }

          $object->{'stack'} = \@stack;
      }
  }
}


#----------------------------------------------------------------------
#
# Read input
#
if ($::opt_format eq "boehm") {
    read_boehm;
}
elsif ($::opt_format eq "trace-malloc") {
    read_trace_malloc;
}
else {
    die "unknown format ``$::opt_format''.";
}


#----------------------------------------------------------------------
#
# Now thread the parents and children together by looking through the
# slots for each object.
#
foreach my $parent (keys %::Objects) {
    # We'll collect a list of this parent object's children
    # by iterating through its slots.
    my @children;

    my $slots = $::Objects{$parent}->{'slots'};
    SLOT: foreach my $child (@$slots) {
        # We only care about pointers that refer to other objects
        next SLOT unless $::Objects{$child};

        # Add the parent to the child's list of parents
        my $parents = $::Objects{$child}->{'parents'};
        if (! $parents) {
            $parents = $::Objects{$child}->{'parents'} = [];
        }

        $parents->[scalar(@$parents)] = $parent;

        # Add the child to the parent's list of children
        $children[$#children + 1] = $child;
    }

    $::Objects{$parent}->{'children'} = \@children;
}


#----------------------------------------------------------------------
#
# Determine objects that entrain equivalent sets, using the strongly
# connected component algorithm from Cormen, Leiserson, and Rivest,
# ``An Introduction to Algorithms'', MIT Press 1990, pp. 488-493.
#
sub compute_post_order($$$) {
# This routine produces a post-order of the call graph (what CLR call
# ``ordering the nodes by f[u]'')
    my ($parent, $visited, $finish) = @_;

    # Bail if we've already seen this node
    return if $visited->{$parent};

    # We have now!
    $visited->{$parent} = 1;

    # Walk the children
    my $children = $::Objects{$parent}->{'children'};

    foreach my $child (@$children) {
        compute_post_order($child, $visited, $finish);
    }

    # Now that we've walked all the kids, we can append the parent to
    # the post-order
    @$finish[scalar(@$finish)] = $parent;
}

sub compute_equivalencies($$$) {
# This routine recursively computes equivalencies by walking the
# transpose of the callgraph.
    my ($child, $table, $equivalencies) = @_;

    # Bail if we've already seen this node
    return if $table->{$child};

    # Otherwise, append ourself to the list of equivalencies...
    @$equivalencies[scalar(@$equivalencies)] = $child;

    # ...and note our other equivalents in the table
    $table->{$child} = $equivalencies;

    my $parents = $::Objects{$child}->{'parents'};

    foreach my $parent (@$parents) {
        compute_equivalencies($parent, $table, $equivalencies);
    }
}

sub compute_equivalents() {
# Here's the strongly connected components algorithm. (Step 2 has been
# done implictly by our object graph construction.)
    my %visited;
    my @finish;

    # Step 1. Compute a post-ordering of the object graph
    foreach my $parent (keys %::Objects) {
        compute_post_order($parent, \%visited, \@finish);
    }

    # Step 3. Traverse the transpose of the object graph in reverse
    # post-order, collecting vertices into %equivalents
    my %equivalents;
    foreach my $child (reverse @finish) {
        compute_equivalencies($child, \%equivalents, []);
    }

    # Now, we'll trim the %equivalents table, arbitrarily removing
    # ``redundant'' entries.
  EQUIVALENT: foreach my $node (keys %equivalents) {
      my $equivalencies = $equivalents{$node};
      next EQUIVALENT unless $equivalencies;

      foreach my $equivalent (@$equivalencies) {
          delete $equivalents{$equivalent} unless $equivalent == $node;
      }
  }

     # Note the equivalent objects in a way that will yield the most
     # interesting order as we do depth-first traversal later to
     # output them.
  ROOT: foreach my $equivalent (reverse @finish) {
      next ROOT unless $equivalents{$equivalent};
      $::Equivalents[$#::Equivalents + 1] = $equivalent;

      # XXX Lame! Should figure out function refs.
      $::Objects{$equivalent}->{'entrained-size'} = 0;
  }
}

# Do it!
compute_equivalents();


#----------------------------------------------------------------------
#
# Compute the size of each node's transitive closure.
#
sub compute_entrained($$) {
    my ($parent, $visited) = @_;

    $visited->{$parent} = 1;

    $::Objects{$parent}->{'entrained-size'} = $::Objects{$parent}->{'size'};

    my $children = $::Objects{$parent}->{'children'};
    CHILD: foreach my $child (@$children) {
        next CHILD if $visited->{$child};

        compute_entrained($child, $visited);
        $::Objects{$parent}->{'entrained-size'} += $::Objects{$child}->{'entrained-size'};
    }
}

if (! $::opt_noentrained) {
    my %visited;

  PARENT: foreach my $parent (@::Equivalents) {
      next PARENT if $visited{$parent};
      compute_entrained($parent, \%visited);
  }
}


#----------------------------------------------------------------------
#
# Converts a shared library and an address into a file and line number
# using a bunch of addr2line processes.
#
sub addr2line($$) {
    my ($dso, $addr) = @_;

    # $::Addr2Lines is a global table that maps a DSO's name to a pair
    # of filehandles that are talking to an addr2line process.
    my $fhs = $::Addr2Lines{$dso};
    if (! $fhs) {
        my ($in, $out) = (new FileHandle, new FileHandle);
        open2($in, $out, "addr2line --exe=$dso") || die "unable to open addr2line --exe=$dso";
        $::Addr2Lines{$dso} = $fhs = { 'in' => $in, 'out' => $out };
    }

    # addr2line takes a hex address as input...
    $fhs->{'out'}->print($addr . "\n");

    # ...and'll return file:lineno as output
    if ($fhs->{'in'}->getline() =~ /([^:]+):(.+)/) {
        return { 'file' => $1, 'line' => $2 };
    }
    else {
        return { 'dso' => $dso, 'addr' => $addr };
    }
}


#----------------------------------------------------------------------
#
# Dump the objects, using a depth-first traversal.
#
sub dump_objects($$$) {
    my ($parent, $visited, $depth) = @_;
    
    # Have we already seen this?
    my $already_visited = $visited->{$parent};
    return if ($depth == 0 && $already_visited);

    if (! $already_visited) {
        $visited->{$parent} = 1;
        $::Total += $::Objects{$parent}->{'size'};
    }

    my $parententry = $::Objects{$parent};

    # Make an ``object'' div, which'll contain an ``object'' span, two
    # ``toggle'' spans, an invisible ``stack'' div, and the invisible
    # ``children'' div.
    print "<div class='object'>";

    if ($already_visited) {
        print "<a href='#$parent'>";
    }
    else {
        print "<span id='$parent' class='object";
        print " root" if $depth == 0;
        print "'>";
    }

    printf "0x%x&lt;%s&gt;[%d]", $parent, $parententry->{'type'}, $parententry->{'size'};

    if ($already_visited) {
        print "</a>";
        goto DONE;
    }
        
    if ($depth == 0) {
        print "($parententry->{'entrained-size'})"
            if $parententry->{'entrained-size'};

        print "&nbsp;<span class='toggle' onclick='toggleDisplay(this.parentNode.nextSibling.nextSibling);'>Children</span>"
            if @{$parententry->{'children'}} > 0;
    }

    if (($depth == 0 || !$::opt_nochildstacks) && !$::opt_nostacks) {
        print "&nbsp;<span class='toggle' onclick='toggleDisplay(this.parentNode.nextSibling);'>Stack</span>";
    }

    print "</span>";

    # Print stack traces
    print "<div class='stack'>\n";

    if (($depth == 0 || !$::opt_nochildstacks) && !$::opt_nostacks) {
        my $depth = $::opt_depth;

      FRAME: foreach my $frame (@{$parententry->{'stack'}}) {
          # Only go as deep as they've asked us to.
          last FRAME unless --$depth >= 0;

          # Stack frames look like ``mangled_name[dso address]''
          $frame =~ /([^\]]+)\[(.*) \+0x([0-9A-Fa-f]+)\]/;

          # Convert address to file and line number
          my $mangled = $1;
          my $result = addr2line($2, $3);

          if ($result->{'file'}) {
              if ($result->{'file'} =~ s/.*\/mozilla/http:\/\/lxr.mozilla.org\/mozilla\/source/) {
                  # It's mozilla source! Clean up refs to dist/include
                  $result->{'file'} =~ s/..\/..\/dist\/include\///;
                  print "<a href=\"$result->{'file'}#$result->{'line'}\">$mangled</a><br>\n";
              }
              else {
                  print "$mangled ($result->{'file'}, line $result->{'line'})<br>\n";
              }
          }
          else {
              print "$result->{'dso'} ($result->{'addr'})<br>\n";
          }
      }

    }

    print "</div>";

    # Recurse to children
    if (@{$parententry->{'children'}} >= 0) {
        print "<div class='children'>\n" if $depth == 0;

        foreach my $child (@{$parententry->{'children'}}) {
            dump_objects($child, $visited, $depth + 1);
        }

        print "</div>" if $depth == 0;
    }

  DONE:
    print "</div>\n";
}


#----------------------------------------------------------------------
#
# Do the output.
#

# Force flush on STDOUT. We get funky output unless we do this.
$| = 1;

# Header
print "<html>
<head>
<title>Object Graph</title>
<style type='text/css'>
    body { font: small monospace; background-color: white; }

    /* give nested div's some margins to make it look like a tree */
    div.children > div.object { margin-left: 1em; }
    div.object > div.object { margin-left: 1em; }

    /* Indent stacks, too */
    div.object > div.stack { margin-left: 3em; }

    /* apply font decorations to special ``object'' spans */
    span.object { font-weight: bold; color: darkgrey; }
    span.object.root { color: black; }

    /* hide ``stack'' divs by default; JS will show them */
    div.stack { display: none; }

    /* hide ``children'' divs by default; JS will show them */
    div.children { display: none; }

    /* make ``toggle'' spans look like links */
    span.toggle { color: blue; text-decoration: underline; }
    span.toggle:active { color: red; }
</style>
<script language='JavaScript'>
function toggleDisplay(element)
{
    element.style.display = (element.style.display == 'block') ? 'none' : 'block';
}
</script>
</head>
<body>
";

{
# Body. Display ``roots'', sorted by the amount of memory they
# entrain. Because of the way we've sorted @::Equivalents, we should
# get a nice ordering that sorts things with a lot of kids early
# on. This should yield a fairly "deep" depth-first traversal, with
# most of the objects appearing as children.
#
# XXX I sure hope that Perl implements a stable sort!
    my %visited;

    foreach my $parent (sort { $::Objects{$b}->{'entrained-size'}
                               <=> $::Objects{$a}->{'entrained-size'} }
                        @::Equivalents) {
        dump_objects($parent, \%visited, 0);
        print "\n";
    }
}

# Footer
print "<br> $::Total total bytes\n" if $::Total;
print "</body>
</html>
";

