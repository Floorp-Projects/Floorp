#! perl -w

# Usage:
# module-graph.pl [directory [ directory ..] ] > foo.dot
#
# Description:
# Outputs a Graphviz-compatible graph description file for use
# with the utilities dot, sccmap, and so forth.
# Graphviz is available from:
# http://www.research.att.com/sw/tools/graphviz/
#
# Reccomendations:
# View the graphs by creating graphs with dot:
# > dot -Tpng foo.dot -o foo.png
#

# Todo:
# - eliminate arcs implied by transitive dependancies
#   (i.e. in a -> b -> c; a->c;, eliminate a->c;
#   (discovered that "tred" will do this, but isn't super-helpful)
# - group together strongly-connected components, where strongly connected
#   means there exists a cycle, and all dependancies off the cycle.
#   in the graph "a -> b <-> c -> d", b and c are strongly connected, and
#   they depend on d, so b, c, and d should be grouped together.

use strict;

my %clustered;
my %deps;
my %toplevel_modules;

my $makecommand;

if ($^O eq "linux") {
  $makecommand = "make";
} elsif ($^O eq "MSWin32") {
  $makecommand = "nmake /nologo /f makefile.win";
}

use Cwd;
my @dirs;
my $curdir = getcwd();
if (!@ARGV) {
  @dirs = (getcwd());
} else {
  @dirs = @ARGV;
  # XXX does them in reverse order..
  my $arg;
  foreach $arg (@ARGV) {
    push @dirs, "$curdir/$arg";
  }
}

MFILE:
while ($#dirs != -1) {
  my ($current_dirs, $current_module, $current_requires);
  # pop the curdir
  $curdir = pop @dirs;

  print STDERR "Entering $curdir..                 \r";
  chdir "$curdir" || next;
  $current_dirs = "";
  open(MAKEOUT, "$makecommand echo-dirs echo-module echo-requires|") || die "Can't make: $!\n";

  $current_dirs = <MAKEOUT>; $current_dirs && chop $current_dirs;
  $current_module = <MAKEOUT>; $current_module && chop $current_module;
  $current_requires = <MAKEOUT>; $current_requires && chop $current_requires;
  close MAKEOUT;

  if ($current_module) {
    #
    # now keep a list of all dependancies of the module
    #
    my @require_list = split(/\s+/,$current_requires);
    my $req;
    foreach $req (@require_list) {
      $deps{$current_module}{$req}++;
    }

    $toplevel_modules{$current_module}++;
  }
  

  next if !$current_dirs;

  # now push all child directories onto the list
  my @local_dirs = split(/\s+/,$current_dirs);
  for (@local_dirs) {
    push @dirs,"$curdir/$_" if $_;
  }

}
print STDERR "\n";

print "digraph G {\n";
print "    concentrate=true;\n";

# figure out the internal nodes, and place them in a cluster

#print "    subgraph cluster0 {\n";
#print "        color=blue;\n"; # blue outline around cluster

my $module;
# ** new method: just list all modules that came from MODULE=foo
foreach $module (sort keys %toplevel_modules) {
  print "        $module [style=filled];\n"
}

# ** old method: find only internal nodes
# (nodes with both parents and children)

# foreach $module (sort { scalar keys %{$deps{$b}} <=> scalar keys %{$deps{$a}} } keys %deps) {
#   foreach $depmod ( keys %deps ) {
#     # only in cluster if they are a child too
#     if ($deps{$depmod}{$module}) {
#       print "        $module;\n";
#       $clustered{$module}++;
#       last;
#     }
#   }
# }

#print "    };\n";

foreach $module (sort sortby_deps keys %deps) {
  my $req;
  foreach $req ( sort { $deps{$module}{$b} <=> $deps{$module}{$a} }
                 keys %{ $deps{$module} } ) {
#    print "    $module -> $req [weight=$deps{$module}{$req}];\n";
    print "    $module -> $req;\n";
  }
}


print "}";


# we're sorting based on clustering
# order:
#   - unclustered, with dependencies
#   - clustered
#   - unclustered, with no dependencies
# However, the last group will probably never come in $a or $b, because we're
# probably only being called from the keys in $deps
# We'll keep all the logic here, in case we come up with a better scheme later
sub sortby_deps() {

  my $keys_a = scalar keys %{$deps{$a}};
  my $keys_b = scalar keys %{$deps{$b}};
  
  # determine if they are the same or not
  if ($clustered{$a} && $clustered{$b}) {
    # both in "clustered" group
    return $keys_a <=> $keys_b;
  }

  elsif (!$clustered{$a} && !$clustered{$b}) {
    # not clustered. Do they both have dependencies or both
    # have no dependencies?

    if (($keys_a && $keys_b) ||
        (!$keys_a && !$keys_b)) {
      # both unclustered, and either both have dependencies,
      # or both don't have dependencies
      return $keys_a <=> $keys_b;
    }
  }

  # if we get here, then they are in different "groups"
  if ($clustered{$a}) {
    # b must be unclustered
    if ($keys_b) {
      return 1;
    } else {
      return -1;
    }
  } elsif ($clustered{$b}) {
    # a must be unclustered
    if ($keys_a) {
      return -1;
    } else {
      return 1;
    }
  } else {
    # both are unclustered, so the with-dependencies one comes first
    if ($keys_a) {
      return -1;
    } else {
      return 1;
    }
  }
  
}
