#!/usr/bin/perl -w

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
# Note to Linux users: graphviz needs TrueType fonts installed
# http://support.pa.msu.edu/Help/FAQs/Linux/truetype.html
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

# For --option1, --option2, ...
use Getopt::Long;
Getopt::Long::Configure("bundling_override");
Getopt::Long::Configure("auto_abbrev");

sub PrintUsage {
  die <<END_USAGE
  Prints out required modules for specified directories.
  usage: module-graph.pl [--start-module <mod> ] [--force-order <file> ] [--file <file> | <dir1> <dir2> ...] [--list-only] [--skip-tree] [--skip-dep-map] [--skip-list]
END_USAGE
}

my %clustered;        # strongly connected components.
my %deps;
my %toplevel_modules; # visited components.
my %all_modules;
my $debug = 0;

my $makecommand = "make";

use Cwd;
my @dirs;
my $curdir = getcwd();

my $opt_list_only = 0;          # --list-only: only print out module names
my $opt_dont_print_tree = 0;    # --skip-tree: don't print dependency tree
my $opt_dont_print_dep_map = 0; # --skip-dep-map: don't print dependency map
my $opt_dont_print_list = 0;    # --skip-list: don't print dependency list

my $load_file = 0;       # --file
my $opt_start_module;    # --start-module optionally print out dependencies    
                         # for a given module.
my $force_order = 0;     # --force-order gives rules on ordering outside of
                         #   normal means.


# Parse commandline input.
sub parse_args() {
  # Stuff arguments into variables.
  # Print usage if we get an unknown argument.
  PrintUsage() if !GetOptions('list-only' => \$opt_list_only,
							  'skip-dep-map' => \$opt_dont_print_dep_map,
							  'skip-tree' => \$opt_dont_print_tree,
							  'skip-list' => \$opt_dont_print_list,
							  'start-module=s' => \$opt_start_module,
							  'file=s' => \$load_file,
							  'force-order=s' => \$force_order);

  # list-only: don't print tree + don't pring dep map
  if($opt_list_only) {
     $opt_dont_print_tree = 1;
	 $opt_dont_print_dep_map = 1;
  }

  # Last args are directories, if not in load-from-file mode.
  unless($load_file) {
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
  }
}


#  Build up the %deps matrix.
sub build_deps_matrix() {
MFILE:
  while ($#dirs != -1) {
	my ($current_dirs, $current_module, $current_requires);
	# pop the curdir
	$curdir = pop @dirs;
	
	if(!$opt_list_only) {
	  print STDERR "Entering $curdir..                 \r";
	}
	chdir "$curdir" || next;
	if ($^O eq "linux") {
      next if (! -e "$curdir/Makefile");
	} elsif ($^O eq "MSWin32") {
      next if (! -e "$curdir/makefile.win");
	}
	
	$current_dirs = "";
	open(MAKEOUT, "$makecommand echo-dirs echo-module echo-requires|") || die "Can't make: $!\n";
	
	$current_dirs = <MAKEOUT>; $current_dirs && chop $current_dirs;
	$current_module = <MAKEOUT>; $current_module && chop $current_module;
	$current_requires = <MAKEOUT>; $current_requires && chop $current_requires;
	close MAKEOUT;
	
	if ($current_module) {
	  #
	  # now keep a list of all dependencies of the module
	  #
	  my @require_list = split(/\s+/,$current_requires);
	  my $req;
	  foreach $req (@require_list) {
		$deps{$current_module}{$req}++;
	  }
	
	  $toplevel_modules{$current_module}++;
	  $all_modules{$current_module}++;
	}
	
	next if !$current_dirs;
	
	# now push all child directories onto the list
	my @local_dirs = split(/\s+/,$current_dirs);
	for (@local_dirs) {
	  push @dirs,"$curdir/$_" if $_;
	}
  }

  if(!$opt_list_only) {
	print STDERR "\n";
  }
}


#
# 
#
sub build_deps_matrix_from_file {
  my ($filename) = @_;

  open DEPS_FILE, $filename or print "can't open $filename, $?\n";
  my @line;
  while (<DEPS_FILE>) {
	if(/->/) {
	  chomp;
	  s/^\s+//;  # Strip off leading spaces.
	  s/\;//;    # Strip off ';'

	  # Pick off module, and dependency from -> line.
	  @line = split(' -> ', $_);
	  $deps{$line[0]}{$line[1]}++;

	  # Add the module to the list of modules.
	  $toplevel_modules{$line[0]}++;
      } elsif (/style=filled/) {
	  chomp;
	  s/^\s+//;  # Strip off leading spaces.
	  s/\;//;    # Strip off ';'

	  # Pick off module
	  @line = split(' \[', $_);
	  $all_modules{$line[0]}++;
      }
  }
  close DEPS_FILE;

}

# Print out %deps.
sub print_deps_matrix() {
  my $module;
  if(!$opt_dont_print_dep_map) {
	print "digraph G {\n";
	print "    concentrate=true;\n";
	
	# figure out the internal nodes, and place them in a cluster
	
	#print "    subgraph cluster0 {\n";
	#print "        color=blue;\n"; # blue outline around cluster
	
	# ** new method: just list all modules that came from MODULE=foo
	foreach $module (sort keys %toplevel_modules) {
	  print "        $module [style=filled];\n"
	}
  }

  print_dependency_list();

  if(!$opt_dont_print_dep_map) {
	print "}\n";
  }
}

#
#   Here we order an array based on other rules, and return the result.
#   Optional from --force-order.
#
sub possibly_force_order {
  my @modarray = @_;
  
  if ($force_order) {
    open FORCEORDER, $force_order     or die "can't open force order file $force_order\n";
    
    my @mod_orders;
    
    LINE: while(<FORCEORDER>) {
      # skip comments
      next LINE if /^#/;
      
      # Should be two strings, or we skip.
      @mod_orders = split;
      if( $#mod_orders + 1 == 2 ) {
        my $mod_before = $mod_orders[0];
        my $has_before = -1;
        my $mod_after = $mod_orders[1];
        my $has_after = -1;
        my $traverse = 0;
        
        my $mod_item = "";
        foreach $mod_item (@modarray) {
          # strip whitespace.
          for ($mod_item) {
              s/^\s+//;
              s/\s+$//;
          }
          
          if( $mod_item eq $mod_before) {
            $has_before = $traverse;
          }
          if( $mod_item eq $mod_after ) {
            $has_after = $traverse;
          }
          
          $traverse++;
        }
        
        # Must find both strings.
        # Must be out of order to rewrite string.
        if(-1 != $has_before && -1 != $has_after && $has_before > $has_after) {
          # Rewrite the array.
          my $modules_string = "";
          $traverse = 0;
          foreach $mod_item (@modarray) {
            # skip before, it comes with after.
            if( $traverse != $has_before ) {
              # if after, then we add before.
              if( $traverse == $has_after ) {
                $modules_string .= $mod_before;
                $modules_string .= " ";
              }
              
              # add this item to the string.
              $modules_string .= $mod_item;
              $modules_string .= " ";
            }
            $traverse++;
          }
          
          # rewrite return value.
          @modarray = split(' ', $modules_string);
        }
      }
    }
    
    close FORCEORDER;
  }
  
  return @modarray;
}

# ** old method: find only internal nodes
# (nodes with both parents and children)
sub print_internal_nodes() {
  my $module;
  my $depmod;
  foreach $module (sort { scalar keys %{$deps{$b}} <=> scalar keys %{$deps{$a}} } keys %deps) {
	foreach $depmod ( keys %deps ) {
	  # only in cluster if they are a child too
	  if ($deps{$depmod}{$module}) {
		print "        $module;\n";
		$clustered{$module}++;
		last;
	  }
	}
  }
}

# Run over dependency array to generate raw component list.
# This is the "a -> b" lines.
sub print_dependency_list() {
  my @raw_list;
  my @unique_list;
  my $module;

  foreach $module (sort sortby_deps keys %deps) {
	my $req;
	foreach $req ( sort { $deps{$module}{$b} <=> $deps{$module}{$a} }
				   keys %{ $deps{$module} } ) {
	  #    print "    $module -> $req [weight=$deps{$module}{$req}];\n";
	  if(!$opt_dont_print_dep_map) {
		print "    $module -> $req;\n";
	  } else {
		# print "$req ";
		push(@raw_list, $req);
	  }
	}
  }

  # generate unique list, print it out.
  if($opt_list_only) {
	my %saw;
	undef %saw;
	@unique_list = grep(!$saw{$_}++, @raw_list);

	# apply forced order if desired.
	@unique_list = possibly_force_order(@unique_list);

	my $i;
	for ($i=0;$i <= $#unique_list; $i++) {
	  print $unique_list[$i], " ";
	}
	print "\n";
  }
}


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

#
# Recursively traverse the deps matrix.
#
my %visited_nodes;
my @visited_nodes_leaf_first_order; # Store in post-recursion order.

sub walk_module_digraph {
  my ($module, $level) = @_;

  # Remember that we visited this node.
  $visited_nodes{$module}++;

  # Print this node.
  if (!$opt_dont_print_tree) {
	my $i;
	for ($i=0; $i<$level; $i++) {
	  print "  ";
	}
	print "$module\n";
  }

  # If we haven't visited this node, search again
  # from this node.
  my $depmod;
  foreach $depmod ( keys %{ $deps{$module} } ) {
	my $visited = $visited_nodes{$depmod};

	if(!$visited) {    	# test recursion: if($level < 5)
	  walk_module_digraph($depmod, $level + 1);
	}
  }

  # Post-recursion.  Store in array form so we keep the order.
  push(@visited_nodes_leaf_first_order, $module);

  if (!$opt_dont_print_tree) {
	if($level == 1) {
	  print "\n";
	}
  }
}


sub print_module_deps {

    # The "ALL" module requires special handling.
    # Just print out the toplevel module names
    if ($opt_start_module eq "ALL") {
	my $key;
	my @outlist;
	foreach $key (keys %all_modules) {
	    push @outlist, $key;
	}
	if ($opt_list_only) {
	    print "@outlist\n";
	    return;
	}
    }

  # Recursively hunt down dependencies for $opt_start_module
  walk_module_digraph($opt_start_module, 1);

  # apply forced ordering if needed.
  @visited_nodes_leaf_first_order = possibly_force_order(@visited_nodes_leaf_first_order);

  # Post-recursion version.
  unless ($opt_dont_print_list) {
	my $visited_mod;
	foreach $visited_mod (@visited_nodes_leaf_first_order) {
	  print "$visited_mod ";
	}
	print "\n";
  }

  if($debug) {
	my @total_visited = (sort keys %visited_nodes);
	my $total = $#total_visited + 1;
	print "\ntotal = $total\n";
  }
}


sub get_matrix_size {
    my (%matrix) = @_;

  	my $i;
	my $j;

	$i = 0;
    $j = 0;
	foreach $i ( keys %matrix ) {
	  $j++;
	}

	return $j;
}


# main
{
  parse_args();

  if($load_file) {
	build_deps_matrix_from_file($load_file);
  } else {
	build_deps_matrix();
  }

  # Print out deps matrix.
  # --list-only and --start-module together mean to
  # print out the module deps, not the matrix.
  if (not ($opt_list_only)) {
    print_deps_matrix();
  }

  # If we specified a --start-module option, print out
  # the required modules for that module.
  if($opt_start_module) {
	print_module_deps();
  }

  if($debug) {
	print STDERR "----- sizes -----\n";
	print STDERR "            deps: " . get_matrix_size(%deps) . "\n";
	print STDERR "toplevel_modules: " . get_matrix_size(%toplevel_modules) . "\n";
	print STDERR "       clustered: " . get_matrix_size(%clustered) . "\n";
  }
}
