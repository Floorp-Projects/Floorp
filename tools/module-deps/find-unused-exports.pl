#!/bin/env perl

use strict;

my $export_file;
my @import_files;

my $show_class_usage = 0;
my $show_function_usage = 0;
my $infrequent_use_threshold = 1;

# first argument is the file that's exporting
# all further arguments are the input files
my $arg;

foreach $arg (@ARGV) {
  if ($arg =~ /^-(.*)/) {
    $show_class_usage=1 if ($1 eq "c");
    $show_function_usage=1 if ($1 eq "f");
  }
  elsif (!$export_file) {
    $export_file = $arg;
  } else {
    push @import_files, $arg;
  }
}

if (!$export_file || $#import_files == -1) {
  print "$export_file, $#import_files\n";
  print "Usage: $ARGV[0] <exporter>.dll [ importer [ importer ..]]\n";
  print "  Builds up a list of exporters from <exporter>.dll,\n";
  print "  and then lists all exports that are rarely or never used.\n";
  exit;
}

open EXPORTS, "dumpbin /exports $export_file|";

# $demangle{foo} is the demangled name of "foo"
my %demangle;

# $import_count{foo} is the number of exports of "foo"
my %import_count;
my %last_consumer_of;

my $found_exports;
while (<EXPORTS>) {
  # format is
  # ordinal hint RVA? name
  if ($found_exports) {
    # break up the words
    my @words = split /\s+/;
    next if ($#words != 4);

    my $ordinal = $words[1];
    my $name = $words[4];

    $import_count{$name} = 0;
    
  } elsif (/ordinal\s+hint\s+RVA/) {
    $found_exports = 1;
  }
}

close EXPORTS;

#
# now read in all imports, and ++ the imports from the exported file
#
my $filename;
foreach $filename (@import_files) {

  print STDERR "gathering data for $filename\n";
  open IMPORTS, "dumpbin /imports $filename|";
  my $current_import_dll;
  my $awaiting_import_dll;
  my $importing_from;
  my $awaiting_import_header;
  while (<IMPORTS>) {
    chop;
    if (/Dump of file ([^\s]+)/) {
      $awaiting_import_dll = 0;
      $current_import_dll = $1;
      #print STDERR "Found imports for $current_import_dll\n";

    } elsif (/Section contains the following imports/) {
      $awaiting_import_dll = 1;

    } elsif ($awaiting_import_dll && /^\s+([\w\.]+)$/) {
      $importing_from = $1;
      $awaiting_import_dll = 0;
      $awaiting_import_header = 1;
      # print STDERR "$current_import_dll: Importing from $importing_from\n";
    } elsif ($awaiting_import_header) {
      if ($_ eq "") {
        $awaiting_import_header = 0;
      }
      # print STDERR "...Skipped past header, starting imports\n" if (!$awaiting_import_header);

    } else {
      if ($_ eq "") {
        $awaiting_import_dll = 1;
        next;
      }
      if ($export_file eq $importing_from) {
        # must be an import line
        my @imports = split /\s+/;
        next if ($#imports != 2);
        my $func = $imports[2];
        #print STDERR "... importing $func from $export_file\n";
        $import_count{$func}++;
        $last_consumer_of{$func} = $current_import_dll;

        #      print STDERR "Got import $importing_from\[$imports[2]\]\n";
      }
    }
  }
  close IMPORTS;
}

my $func;
my @funcs_to_demangle;

print STDERR "finding infrequently imported symbols..\n";
foreach $func (sort keys %import_count) {
  # we need to demangle all symbols if we're showing unused classes
  # otherwise, we only need to demangle infrequently-used symbols
  if ($show_class_usage ||
      $import_count{$func} <= $infrequent_use_threshold) {
    push @funcs_to_demangle, $func;
  } else {
    #print STDERR "Found $import_count{$func} consumers of $func\n";
  }
}

print STDERR "Demangling..$#funcs_to_demangle unused symbols\n";

my $max_per_run = 20;

my $demangle_queue;
my $count = 0;
while ($func = pop @funcs_to_demangle) {

  # skip functions that are used often
  #next if ($import_count{$func} > 1);
  
  $count++;
  $demangle_queue .= " $func";
  
  if ($count == $max_per_run || ($#funcs_to_demangle == 0)) {
    # set up the initial slot where the demangled name goes
    open UNDNAME, "undname -f $demangle_queue|";
    while (<UNDNAME>) {
      chop;

      #      print STDERR "Got result $_\n";
      if (/\s*(\S+)\s*==\s*(.*)$/) {
        #        print STDERR"successfully demangled $1 to $2\n";
        $demangle{$1} = $2;
      } else {
        $demangle{$1} = $1;
      }
    }
    close UNDNAME;
    $demangle_queue = "";
    $count = 0;
  }
}

print STDERR "Done.\n";

my %class_methods;              # number of methods
my %class_method_unused;        # number of unused methods in this class
my %class_method_infreq;        # number of infrequently-used methods
my %class_method_used;          # number of used method
my %multiple_consumers;         # if a class is in here, it has multiple consumers
my %last_consumer_of_class;     # last DLL to use this class

foreach $func (sort keys %demangle) {

  # skip vtables, they don't matter
  next if ($demangle{$func} =~ /vftable/);
  next if ($demangle{$func} =~ /vbtable/);

  my ($class) = ($demangle{$func} =~ /(\S+)::\S+/);

  # gather general class info, no matter what the usage
  if ($class) {
    $class_methods{$class}++;
    $multiple_consumers{$class} = 1 if ($import_count{$func} > 1);
  }

  # never used
  if ($import_count{$func} == 0) {
    $class_method_unused{$class}++ if ($class);
    print "$demangle{$func}: No importers\n" if ($show_function_usage);
  }

  # used, but only by a few consumers
  elsif ($import_count{$func} <= $infrequent_use_threshold) {

    print "$demangle{$func}: $import_count{$func} consumer(s) ($last_consumer_of{$func})\n" if ($show_function_usage);
    &gather_class_info($class, $func) if ($class);
  }

  # used by more than one consumer
  else {
    $class_method_used{$class}++ if ($class);
  }


}

&show_class_usage() if ($show_class_usage);


sub show_class_usage() {

  print "Classes that might be worth trimming:\n";
  my $class;
  foreach $class (sort keys %class_methods) {
    # no methods are used
    if ($class_method_unused{$class} == $class_methods{$class}) {
      print "$class is never used! ($class_methods{$class} methods)\n";
    }

    # all methods are used
    elsif ($class_method_used{$class} == $class_methods{$class}) {
      # print "$class is well-used\n";
    }

    # only report classes that less than 50% used
    elsif ($class_method_unused{$class}*2 > $class_methods{$class}) {
      print "$class: $class_methods{$class} methods, mostly unused.\n";
      print "  Used:   $class_method_used{$class}";

      print " ($class_method_infreq{$class} are used by few consumers)"
        if ($class_method_infreq{$class});

      print "\n";

      print "  Unused: $class_method_unused{$class}\n";
      if (!$multiple_consumers{$class}) {
        print "  Suggest moving $class into $last_consumer_of_class{$class} (the only consumer)\n";
      }
    }
  }

}

sub gather_class_info() {
  my ($class, $func) = @_;

  $class_method_infreq{$class}++;
  $class_method_used{$class}++;
  # we rely on $infrequent_use_threshold being >= 1
  if ($import_count{$func} == 1) {

    # check to see if this class has multiple consumers
    if (!$multiple_consumers{$class} && !$last_consumer_of_class{$class}) {
      $last_consumer_of_class{$class} = $last_consumer_of{$func};
      # $multiple_consumers{$class} = 0;
    }

    elsif ($last_consumer_of_class{$class} ne $last_consumer_of{$func}) {
      # flag this to never match
      $multiple_consumers{$class} = 1;
    }
  }
}
