#!/usr/bin/perl -w
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# A perl version of Patrick Beard's ``Leak Soup'', which processes the
# stack crawls from the Boehm GC into a graph.

use 5.004;
use strict;
use Getopt::Long;
use FileHandle;
use IPC::Open2;

# Collect program options
$::opt_help = 0;
$::opt_detail = 0;
$::opt_fragment = 1.0; # Default to no fragment analysis
$::opt_nostacks = 0;
$::opt_nochildstacks = 0;
$::opt_depth = 9999;
$::opt_noentrained = 0;
$::opt_noslop = 0;
$::opt_showtype = -1; # default to listing all types
$::opt_stackrefine = "C";
@::opt_stackretype = ();
@::opt_stackskipclass = ();
@::opt_stackskipfunc = ();
@::opt_typedivide = ();

GetOptions("help", "detail", "format=s", "fragment=f", "nostacks", 
	   "nochildstacks", "depth=i", "noentrained", "noslop", "showtype=i", 
	   "stackrefine=s", "stackretype=s@", "stackskipclass=s@", "stackskipfunc=s@",
	   "typedivide=s@"
	   );

if ($::opt_help) {
    die "usage: leak-soup.pl [options] <leakfile>
  --help                 Display this message
  --detail               Provide details of memory sweeping from child to parents
  --fragment=ratio       Histogram bucket ratio for fragmentation analysis
#  --nostacks            Do not compute stack traces
#  --nochildstacks       Do not compute stack traces for entrained objects
#  --depth=<max>         Only compute stack traces to depth of <max>
#  --noentrained         Do not compute amount of memory entrained by root objects
  --noslop               Don't ignore low bits when searching for pointers
  --showtype=<i>         Show memory usage histogram for most-significant <i> types
  --stackrefine={F|C}    During stack based refinement, use 'F'ull name name or just 'C'lass
  --stackretype=type     Use allocation stack to refine vague types like void*
  --stackskipclass=class When refining types, ignore stack frames from 'class'
  --stackskipfunc=func   When refining types, ignore stack frames for 'func'
  --typedivide=type      Subdivide 'type' based on objects pointing to each instance
";
}

# This is the table that keeps a graph of objects. It's indexed by the
# object's address (as an integer), and refers to a simple hash that
# has information about the object's type, size, slots, and allocation
# stack.
%::Objects = %{0};

# This will be a list of keys to (addresses in) Objects, that is sorted
# It gets used to evaluate overlaps, calculate fragmentation, and chase 
# parent->child (interior) pointers.
@::SortedAddresses = [];

# This is the table that keeps track of memory usage on a per-type basis.
# It is indexed by the type name (string), and keeps a tally of the 
# total number of such objects, and the memory usage of such objects.
%::Types = %{0};
$::TotalSize = 0; # sum of sizes of all objects included $::Types{}

# This is an array of leaf node addresses.  A leaf node has no children
# with memory allocations. We traverse them sweeping memory
# tallies into parents.  Note that after all children have
# been swept into a parent, that parent may also become a leaf node.
@::Leafs = @{0};




#----------------------------------------------------------------------
#
# Decode arguments to override default values for doing call-stack-based 
# refinement of typename based on contents of the stack at allocation time.
#

# List the types that we need to refine (if any) based on allocation stack
$::VagueType = {
    'void*' => 1,
};

# With regard to the stack, ignore stack frames in the following
# overly vague classes.
$::VagueClasses = {
#    'nsStr' => 1,
    'nsVoidArray' => 1,
};

# With regard to stack, ignore stack frames with the following vague
# function names
$::VagueFunctions = {
    'PL_ArenaAllocate' => 1,
    'PL_HashTableFinalize(PLHashTable *)' => 1,
    'PL_HashTableInit__FP11PLHashTableUiPFPCv_UiPFPCvPCv_iT3PC14PLHashAllocOpsPv' => 1,
    'PL_HashTableRawAdd' => 1,
    '__builtin_vec_new' => 1,
    '_init' => 1,
    'il_get_container(_IL_GroupContext *, ImgCachePolicy, char const *, _NI_IRGB *, IL_DitherMode, int, int, int)' => 1,
    'nsCStringKey::Clone(void) const' => 1,
    'nsCppSharedAllocator<unsigned short>::allocate(unsigned int, void const *)' => 1,
    'nsHashtable::Put(nsHashKey *, void *)' => 1,
    'nsHashtable::nsHashtable(unsigned int, int)' => 1,
    'nsMemory::Alloc(unsigned int)' => 1,
    'nsMemoryImpl::Alloc(unsigned int)' => 1,
};

sub init_stack_based_type_refinement() {
    # Move across stackretype options, or use default values
    if ($#::opt_stackretype < 0) {
	print "Default --stackretype options will be used (since none were specified)\n";
	print "  use --stackretype='nothing' to disable re-typing activity\n";
    } else {
	foreach my $type (keys %{$::VagueType}) {
	    delete ($::VagueType->{$type});
	}
	if ($#::opt_stackretype == 0 && $::opt_stackretype[0] eq 'nothing') {
	    print "Types will not be refined based on call stack\n";
	} else {
	    foreach my $type (@::opt_stackretype) {
		$::VagueType->{$type} = 1;
	    }
	}
    }


    if (keys %{$::VagueType}) {
	print "The following type(s) will be refined based on call stacks:\n";
	foreach my $type (sort keys %{$::VagueType}) {
	    print "     $type\n";
	}
	print "Equivalent command line argument(s):\n";
	foreach my $type (sort keys %{$::VagueType}) {
	    print " --stackretype='$type'";
	}
	print "\n\n";

	if ($#::opt_stackskipclass < 0) {
	    print "Default --stackskipclass options will be used (since none were specified)\n";
	    print "  use --stackskipclass='nothing' to disable skipping stack frames based on class names\n";
	} else {
	    foreach my $type (keys %{$::VagueClasses}) {
		delete ($::VagueClasses->{$type});
	    }
	    if ($#::opt_stackskipclass == 0 && $::opt_stackskipclass[0] eq 'nothing') {
		print "Types will not be refined based on call stack\n";
	    } else {
		foreach my $type (@::opt_stackskipclass) {
		    $::VagueClasses->{$type} = 1;
		}
	    }
	}

	if (keys %{$::VagueClasses}) {
	    print "Stack frames from the following class(es) will not be used to refine types:\n";
	    foreach my $class (sort keys %{$::VagueClasses}) {
		print "     $class\n";
	    }
	    print "Equivalent command line argument(s):\n";
	    foreach my $class (sort keys %{$::VagueClasses}) {
		print " --stackskipclass='$class'";
	    }
	    print "\n\n";
	}


	if ($#::opt_stackskipfunc < 0) {
	    print "Default --stackskipfunc options will be used (since none were specified)\n";
	    print "  use --stackskipfunc='nothing' to disable skipping stack frames based on function names\n";
	} else {
	    foreach my $type (keys %{$::VagueFunctions}) {
		delete ($::VagueFunctions->{$type});
	    }
	    if ($#::opt_stackskipfunc == 0 && $::opt_stackskipfunc[0] eq 'nothing') {
		print "Types will not be refined based on call stack\n";
	    } else {
		foreach my $type (@::opt_stackskipfunc) {
		    $::VagueFunctions->{$type} = 1;
		}
	    }
	}

	if (keys %{$::VagueFunctions}) {
	    print "Stack frames from the following function(s) will not be used to refine types:\n";
	    foreach my $func (sort keys %{$::VagueFunctions}) {
		print "     $func\n";
	    }
	    print "Equivalent command line argument(s):\n";
	    foreach my $func (sort keys %{$::VagueFunctions}) {
		print " --stackskipfunc='$func'";
	    }
	    print "\n\n";
	}
    }
}


#----------------------------------------------------------------------
#
# Read in the output from the Boehm GC or Trace-malloc. 
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
      } else {
	  print "Duplicate address $addr contains $object->{'type'} and $type\n";
	  $object->{'dup_addr_count'}++;
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

      if (@::opt_stackretype && (defined $::VagueType->{$type})) {
	  # Change the value of type of the object based on stack
	  # if we can find an interesting calling function
        VAGUEFRAME: while (<>) {
            # e.g., _dl_debug_message[/lib/ld-linux.so.2 +0x0000B858]
            last VAGUEFRAMEFRAME unless /^(.*)\[(.*) \+0x(\S+)\]$/;
            my ($func, $lib, $off) = ($1, $2, hex $3);
            chomp;

	    my ($class,,$fname) = split(/:/, $func);
	    next VAGUEFRAME if (defined $::VagueFunctions->{$func} || 
				defined $::VagueClasses->{$class});

	    # Refine typename and exit stack scan
	    $object->{'type'} = $type . ":" . 
		(('C' eq $::opt_stackrefine) ?
		 $class :
		 $func);
	    last VAGUEFRAME;
	} 
      } else {
	  # Save all stack info if requested
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
      }

      # Gotta check EOF explicitly...
      last OBJECT if eof;
  }
}


#----------------------------------------------------------------------
#
# Read input
#
init_stack_based_type_refinement();
read_boehm;



#----------------------------------------------------------------------
#
# Do basic initialization of the type hash table.  Accumulate
# total counts, and basic memory usage (not including children)
sub load_type_table() {
    # Reset global counter and hash table
    $::TotalSize = 0;
    %::Types = %{0};

    OBJECT: foreach my $addr (keys %::Objects) {
	my $obj = $::Objects{$addr};
	my ($type, $size, $swept_in, $overlap_count, $dup_addr_count) = 
	    ($obj->{'type'}, $obj->{'size'}, 
	     $obj->{'swept_in'}, 
	     $obj->{'overlap_count'},$obj->{'dup_addr_count'});

	my $type_data = $::Types{$type};
	if (! defined $type_data) {
	    $::Types{$type} =
		$type_data = {'count' => 0, 'size' => 0, 
			      'max' => $size, 'min' => $size,
			      'swept_in' => 0, 'swept' => 0,
			      'overlap_count' => 0,
			      'dup_addr_count' => 0};
	}

	if (!$size) {
	    $type_data->{'swept'}++;
	    next OBJECT;
	}
	$::TotalSize += $size;

	$type_data->{'count'}++;
	$type_data->{'size'} += $size;
	if (defined $swept_in) {
	    $type_data->{'swept_in'} += $swept_in;

	    if ($::opt_detail) {
		my $type_detail_sizes = $type_data->{'sweep_details_size'};
		my $type_detail_counts;
		if (!defined $type_detail_sizes) {
		    $type_detail_sizes = $type_data->{'sweep_details_size'} = {};
		    $type_detail_counts = $type_data->{'sweep_details_count'} = {};
		} else {
		    $type_detail_counts = $type_data->{'sweep_details_count'};
		}

		my $sweep_details = $obj->{'sweep_details'};
		for my $swept_addr (keys (%{$sweep_details})) {
		    my $swept_obj = $::Objects{$swept_addr};
		    my $swept_type = $swept_obj->{'type'};
		    $type_detail_sizes->{$swept_type} += $sweep_details->{$swept_addr};
		    $type_detail_counts->{$swept_type}++;
		}
	    }
	}
	if (defined $overlap_count) {
	    $type_data->{'overlap_count'} += $overlap_count;
	}

	if (defined $dup_addr_count) {
	    $type_data->{'dup_addr_count'} += $dup_addr_count;
	}

	if ($type_data->{'max'} < $size) {
	    $type_data->{'max'} = $size;
	}
	# Watch out for case where min is produced by a swept object
	if (!$type_data->{'min'} || $type_data->{'min'} > $size) {
	    $type_data->{'min'} = $size;
	}
    }
}


#----------------------------------------------------------------------
sub print_type_table(){
    if (!$::opt_showtype) {
	return;
    }
    my $line_count = 0;
    my $bytes_printed_tally = 0;

    # Display type summary information
    my @sorted_types = keys (%::Types);
    print "There are ", 1 + $#sorted_types, " types containing ", $::TotalSize, " bytes\n";
    @sorted_types = sort {$::Types{$b}->{'size'}
			  <=> $::Types{$a}->{'size'} } @sorted_types;

    foreach my $type (@sorted_types) {
	last if ($line_count++ == $::opt_showtype);

	my $type_data = $::Types{$type};
	$bytes_printed_tally += $type_data->{'size'};

	if ($type_data->{'count'}) {
	    printf "%.2f%% ", $type_data->{'size'} * 100.0/$::TotalSize;
	    print $type_data->{'size'}, 
	    "\t(", 
	    $type_data->{'min'}, "/", 
	    int($type_data->{'size'} / $type_data->{'count'}),"/", 
	    $type_data->{'max'}, ")";
	    print "\t", $type_data->{'count'}, 
	    " x ";
	}
	print $type;

	if ($type_data->{'swept_in'}) {	    
	    print ", $type_data->{'swept_in'} sub-objs absorbed";
	}
	if ($type_data->{'swept'}) {
	    print ", $type_data->{'swept'} swept away";
	}
	if ($type_data->{'overlap_count'}) {	    
	    print ", $type_data->{'overlap_count'} range overlaps";
	}
	if ($type_data->{'dup_addr_count'}) {	    
	    print ", $type_data->{'dup_addr_count'} duplicated addresses";
	}

	print "\n" ;
	if (defined $type_data->{'sweep_details_size'}) {
	    my $sizes = $type_data->{'sweep_details_size'};
	    my $counts = $type_data->{'sweep_details_count'};
	    my @swept_types = sort {$sizes->{$b} <=> $sizes->{$a}} keys (%{$sizes});
	    
	    for my $type (@swept_types) {
		printf "    %.2f%% ", $sizes->{$type} * 100.0/$::TotalSize;
		print "$sizes->{$type}     (", int($sizes->{$type}/$counts->{$type}) , ")   $counts->{$type} x $type\n";
	    }
	    print "    ---------------\n";
	}
    }
    if ($bytes_printed_tally != $::TotalSize) {
	printf "%.2f%% ", ($::TotalSize- $bytes_printed_tally) * 100.0/$::TotalSize;
	print $::TotalSize - $bytes_printed_tally, "\t not shown due to truncation of type list\n";
	print "Currently only data on $::opt_showtype types are displayed, due to command \n",
	    "line argument '--showtype=$::opt_showtype'\n\n";
    }

}

#----------------------------------------------------------------------
#
# Check for duplicate address ranges is Objects table, and 
# create list of sorted addresses for doing pointer-chasing

sub validate_address_ranges() {
    # Build sorted list of address for validating interior pointers
    @::SortedAddresses = sort {$a <=> $b} keys %::Objects;

    # Validate non-overlap of memory
    my $prev_addr_end = -1;
    my $prev_addr = -1;
    my $index = 0;
    my $overlap_tally = 0; # overlapping object memory
    my $unused_tally = 0;  # unused memory between blocks
    while ($index <= $#::SortedAddresses) {
	my $address = $::SortedAddresses[$index];
	if ($prev_addr_end > $address) {
	    print "Object overlap from $::Objects{$prev_addr}->{'type'}:$prev_addr-$prev_addr_end into";
	    my $test_index = $index;
	    my $prev_addr_overlap_tally = 0;

	    while ($test_index <=  $#::SortedAddresses) {
		my $test_address = $::SortedAddresses[$test_index];
		last if ($prev_addr_end < $test_address);
		print " $::Objects{$test_address}->{'type'}:$test_address";

		$::Objects{$prev_addr}->{'overlap_count'}++;
		$::Objects{$test_address}->{'overlap_count'}++;
		my $overlap = $prev_addr_end - $test_address;
		if ($overlap > $::Objects{$test_address}->{'size'}) {
		    $overlap = $::Objects{$test_address}->{'size'};
		}
		print "($overlap bytes)";
		$prev_addr_overlap_tally += $overlap;

		$test_index++;
	    }
	    print " [total $prev_addr_overlap_tally bytes]";
	    $overlap_tally += $prev_addr_overlap_tally;
	    print "\n";
	} 

	$prev_addr = $address;
	$prev_addr_end = $prev_addr + $::Objects{$prev_addr}->{'size'} - 1;
	$index++;
    } #end while
    if ($overlap_tally) {
	print "Total overlap of $overlap_tally bytes\n";
    }
}

#----------------------------------------------------------------------
#
# Evaluate sizes of interobject spacing (fragmentation loss?)
# Gather the sizes into histograms for analysis
# This function assumes a sorted list of addresses is present globally

sub generate_and_print_unused_memory_histogram() {
    print "\nInterobject spacing (fragmentation waste) Statistics\n";
    if ($::opt_fragment <= 1) {
	print "Statistics are not being gathered.  Use '--fragment=10' to get stats\n";
	return;
    }
    print "Ratio of histogram buckets will be a factor of $::opt_fragment\n";

    my $prev_addr_end = -1;
    my $prev_addr = -1;
    my $index = 0;

    my @fragment_count;
    my @fragment_tally;
    my $power;
    my $bucket_size;

    my $max_power = 0;

    my $tally_sizes = 0;

    while ($index <= $#::SortedAddresses) {
	my $address = $::SortedAddresses[$index];

	my $unused = $address - $prev_addr_end;

	# handle overlaps gracefully
	if ($unused < 0) {
	    $unused = 0;
	}

	$power = 0;
	$bucket_size = 1;
	while ($bucket_size < $unused) {
	    $bucket_size *= $::opt_fragment;
	    $power++;
	}
	$fragment_count[$power]++;
	$fragment_tally[$power] += $unused;
	if ($power > $max_power) {
	    $max_power = $power;
	}
	my $size = $::Objects{$address}->{'size'}; 
	$tally_sizes += $size;
	$prev_addr_end = $address + $size - 1;
	$index++;
    }


    $power = 0;
    $bucket_size = 1;
    print "Basic gap histogram is (max_size:count):\n";
    while ($power <= $max_power) {
	if (! defined $fragment_count[$power]) {
	    $fragment_count[$power] = $fragment_tally[$power] = 0;
	}
	printf " %.1f:", $bucket_size;
	print $fragment_count[$power];
	$power++;
	$bucket_size *= $::opt_fragment;
    }
    print "\n";

    print "Summary gap analysis:\n";

    $power = 0;
    $bucket_size = 1;
    my $tally = 0;
    my $count = 0;
    while ($power <= $max_power) {
	$count += $fragment_count[$power];
	$tally += $fragment_tally[$power];
	print "$count gaps, totaling $tally bytes, were under ";
	printf "%.1f bytes each", $bucket_size;
	if ($count) {
	    printf ", for an average of %.1f bytes per gap", $tally/$count, ;
	}
	print "\n";
	$power++;
	$bucket_size *= $::opt_fragment;
    }

    print "Total allocation was $tally_sizes bytes, or ";
    printf "%.0f bytes per allocation block\n\n", $tally_sizes/($count+1);
    
}

#----------------------------------------------------------------------
#
# Now thread the parents and children together by looking through the
# slots for each object.
#
sub create_parent_links(){
    my $min_addr = $::SortedAddresses[0];
    my $max_addr = $::SortedAddresses[ $#::SortedAddresses]; #allow one beyond each object
    $max_addr += $::Objects{$max_addr}->{'size'};

    print "Viable addresses range from $min_addr to $max_addr for a total of ", 
    $max_addr-$min_addr, " bytes\n\n";

    # Gather stats as we try to convert slots to children
    my $slot_count = 0;   # total slots examined
    my $fixed_addr_count = 0; # slots into interiors that were adjusted
    my $parent_child_count = 0;  # Number of parent-child links
    my $child_count = 0;   # valid slots, discounting sibling twins
    my $child_dup_count = 0; # number of duplicate child pointers
    my $self_pointer_count = 0; # count of discarded self-pointers

    foreach my $parent (keys %::Objects) {
	# We'll collect a list of this parent object's children
	# by iterating through its slots.
	my @children;
	my %children_hash;
	my $self_pointer = 0;

	my @slots = @{$::Objects{$parent}->{'slots'}};
	$slot_count += $#slots + 1;
	SLOT: foreach my $child (@slots) {

	    # We only care about pointers that refer to other objects
	    if (! defined $::Objects{$child}) {
		# check to see if we are an interior pointer

		# Punt if we are completely out of range
		next SLOT unless ($max_addr >= $child && 
				  $child >= $min_addr);

		# Do binary search to find object below this address
		my ($min_index, $beyond_index) = (0, $#::SortedAddresses + 1);
		my $test_index;
		while ($min_index != 
		       ($test_index = int (($beyond_index+$min_index)/2)))  {
		    if ($child >= $::SortedAddresses[$test_index]) {
			$min_index = $test_index;
		    } else {
			$beyond_index = $test_index;
		    }
		}
		# See if pointer is within extent of this object
		my $address = $::SortedAddresses[$test_index];
		next SLOT unless ($child < 
				  $address + $::Objects{$address}->{'size'});

		# Make adjustment so we point to the actual child precisely
		$child = $address;
		$fixed_addr_count++;
	    }

	    if ($child == $parent) {
		$self_pointer_count++;
		next SLOT; # Discard self-pointers
	    }

	    # Avoid creating duplicate child-parent links
	    if (! defined $children_hash{$child}) {
		$parent_child_count++;
		# Add the parent to the child's list of parents
		my $parents = $::Objects{$child}->{'parents'};
		if (! $parents) {
		    $parents = $::Objects{$child}->{'parents'} = [];
		}

		$parents->[scalar(@$parents)] = $parent;

		# Add the child to the parent's list of children
		$children_hash{$child} = 1;
	    } else {
		$child_dup_count++;
	    }
	}
	@children = keys %children_hash;
	# Track tally of unique children linked
	$child_count += $#children + 1;

	$::Objects{$parent}->{'children'} = \@children;

	if (! @children) {
	    $::Leafs[$#::Leafs + 1] = $parent;
	} 
    }
    print "Scanning $#::SortedAddresses objects, we found $parent_child_count parents-to-child connections by chasing $slot_count pointers.\n",
    "This required $fixed_addr_count interior pointer fixups, skipping $child_dup_count duplicate pointers, ",
    "and $self_pointer_count self pointers\nAlso discarded ", 
    $slot_count - $parent_child_count -$self_pointer_count - $child_dup_count, 
    " out-of-range pointers\n\n";
}


#----------------------------------------------------------------------
# For every leaf, if a leaf has only one parent, then sweep the memory 
# cost into the parent from the leaf
sub sweep_leaf_memory () {
    my $sweep_count = 0;
    my $leaf_counter = 0;
    LEAF: while ($leaf_counter <= $#::Leafs) {
	my $leaf_addr = $::Leafs[$leaf_counter++];
	my $leaf_obj = $::Objects{$leaf_addr};
	my $parents = $leaf_obj->{'parents'};

	next LEAF if (! defined($parents) || 1 != scalar(@$parents));

	# We have only one parent, so we'll try to sweep upwards
	my $parent_addr = @$parents[0];
	my $parent_obj = $::Objects{$parent_addr};

	# watch out for self-pointers
	next LEAF if ($parent_addr == $leaf_addr); 

	if ($::opt_detail) {
	    foreach my $obj ($parent_obj, $leaf_obj) {
		if (!defined $obj->{'original_size'}) {
		    $obj->{'original_size'} = $obj->{'size'};
		}
	    }
	    if (defined $leaf_obj->{'sweep_details'}) {
		if (defined $parent_obj->{'sweep_details'}) { # merge details
		    foreach my $swept_obj (keys (%{$leaf_obj->{'sweep_details'}})) {
			%{$parent_obj->{'sweep_details'}}->{$swept_obj} = 
			    %{$leaf_obj->{'sweep_details'}}->{$swept_obj};
		    }
		} else { # No parent info
		    $parent_obj->{'sweep_details'} = \%{$leaf_obj->{'sweep_details'}};
		}
		delete $leaf_obj->{'sweep_details'};
	    } else { # no leaf detail
		if (!defined $parent_obj->{'sweep_details'}) {
		    $parent_obj->{'sweep_details'} = {};
		}
	    }
	    %{$parent_obj->{'sweep_details'}}->{$leaf_addr} = $leaf_obj->{'original_size'};
	}

	$parent_obj->{'size'} += $leaf_obj->{'size'};
	$leaf_obj->{'size'} = 0;

	if (defined ($leaf_obj->{'swept_in'})) {
	    $parent_obj->{'swept_in'} += $leaf_obj->{'swept_in'};
	    $leaf_obj->{'swept_in'} = 0;  # sweep has been handed off to parent
	} 
	$parent_obj->{'swept_in'} ++;  # tally swept in leaf_obj

	$sweep_count++;

	# See if we created another leaf
	my $consumed_children = $parent_obj->{'consumed'}++;
	my @children = $parent_obj->{'children'};
	if ($consumed_children == $#children) {
	    $::Leafs[$#::Leafs + 1] = @$parents[0];
	}
    }
    print "Processed ", $leaf_counter, " leaves sweeping memory to parents in ", $sweep_count, " objects\n";
}


#----------------------------------------------------------------------
#
# Subdivide the types of objects that are in our "expand" list
# List types that should be sub-divided based on parents, and possibly 
# children
# The argument supplied is a hash table with keys selecting types that
# need to be "refined" by including the types of the parent objects,
# and (when we are desparate) the types of the children objects.

sub expand_type_names($) {
    my %TypeExpand = %{$_[0]};

    my @retype; # array of addrs that get extended type names
    foreach my $child (keys %::Objects) {
	my $child_obj = $::Objects{$child};
	next unless (defined ($TypeExpand{$child_obj->{'type'}}));

	foreach my $relation ('parents','children') {
	    my $relatives = $child_obj->{$relation};
	    next unless defined @$relatives;

	    # Sort out the names of the types of the relatives
	    my %names;
	    foreach my $relative (@$relatives) {
		%names->{$::Objects{$relative}->{'type'}} = 1;
	    }
	    my $related_type_names = join(',' , sort(keys(%names)));


	    $child_obj->{'name' . $relation} = $related_type_names;

	    # Don't bother with children if we have significant parent types 
	    last if (!defined ($TypeExpand{$related_type_names}));
	}
	$retype[$#retype + 1] = $child;
    }

    # Revisit all addresses we've marked
    foreach my $child (@retype) {
	my $child_obj = $::Objects{$child};
	$child_obj->{'type'} = $TypeExpand{$child_obj->{'type'}};
	my $extended_type = $child_obj->{'namechildren'};
	if (defined $extended_type) {
	    $child_obj->{'type'}.= "->(" . $extended_type . ")";
	    delete ($child_obj->{'namechildren'});
	}
	$extended_type = $child_obj->{'nameparents'};
	if (defined $extended_type) {
	    $child_obj->{'type'} = "(" . $extended_type . ")->" . $::Objects{$child}->{'type'};
	    delete ($child_obj->{'nameparents'});
	}
    }
}

#----------------------------------------------------------------------
#
# Print out a type histogram

sub print_type_histogram() {
    load_type_table();
    print_type_table();
    print "\n\n";
}


#----------------------------------------------------------------------
# Provide a nice summary of the types during the process
validate_address_ranges();
create_parent_links();

print "\nBasic memory use histogram is:\n";
print_type_histogram();

generate_and_print_unused_memory_histogram();

sweep_leaf_memory ();
print "After doing basic leaf-sweep processing of instances:\n";
print_type_histogram();

{
    foreach my $typename (@::opt_typedivide) {
	my %expansion_table;
	$expansion_table{$typename} = $typename;
	expand_type_names(\%expansion_table);
	print "After subdividing <$typename> based on inbound (and somtimes outbound) pointers:\n";
	print_type_histogram();
    }
}

exit();  # Don't bother with SCCs yet.


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
        if (!(-r $dso)) {
            # bogus filename (that happens sometimes), so bail
            return { 'dso' => $dso, 'addr' => $addr };
        }
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
              # It's mozilla source! Clean up refs to dist/include
              if (($result->{'file'} =~ s/.*\.\.\/\.\.\/dist\/include\//http:\/\/bonsai.mozilla.org\/cvsguess.cgi\?file=/) ||
                  ($result->{'file'} =~ s/.*\/mozilla/http:\/\/bonsai.mozilla.org\/cvsblame.cgi\?file=mozilla/)) {
                  my $prevline = $result->{'line'} - 10;
                  print "<a target=\"lxr_source\" href=\"$result->{'file'}\&mark=$result->{'line'}#$prevline\">$mangled</a><br>\n";
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
    body { font: medium monospace; background-color: white; }

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
    span.toggle { color: blue; text-decoration: underline; cursor: pointer; }
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

