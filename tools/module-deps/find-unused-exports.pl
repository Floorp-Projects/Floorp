#!/bin/env perl

my $export_file;
my @import_files;

# first argument is the file that's exporting
# all further arguments are the input files

foreach $arg (@ARGV) {
  if (!$export_file) {
    $export_file = $arg;
  } else {
    push @import_files, $arg;
  }
}

open EXPORTS, "dumpbin /exports $export_file|";

my %exports;

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

    $exports{$name} = 0;
    
  } elsif (/ordinal\s+hint\s+RVA/) {
    $found_exports = 1;
  }
}

close EXPORTS;

#
# now read in all imports, and ++ the imports from the exported file
#
foreach $filename (@import_files) {

  print STDERR "gathering data for $filename\n";
  open IMPORTS, "dumpbin /imports $filename|";
  my $current_import_dll, $awaiting_import_dll, $importing_from;
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
        @imports = split /\s+/;
        next if ($#imports != 2);
        my $name = $imports[2];
        #print STDERR "... importing $name from $export_file\n";
        $exports{$name}++;
        $last_consumer_of{$name} = $current_import_dll;

        #      print STDERR "Got import $importing_from\[$imports[2]\]\n";
      }
    }
  }
  close IMPORTS;
}

print STDERR "finding infrequently imported symbols..\n";
foreach $func (sort keys %exports) {
  if ($exports{$func} < 2) {
    push @funcs_to_demangle, $func;
  } else {
    #print STDERR "Found $exports{$func} consumers of $func\n";
  }
}

print STDERR "Demangling..$#funcs_to_demangle unused symbols\n";

my $max_per_run = 20;

while ($func = pop @funcs_to_demangle) {
  $count++;
  $funcs_to_demangle .= " $func";
  
  if ($count == $max_per_run || ($#funcs_to_demangle == 0)) {
    # set up the initial slot where the demangled name goes
    open UNDNAME, "undname -f $funcs_to_demangle|";
    while (<UNDNAME>) {
      chop;

      #      print STDERR "Got result $_\n";
      if (/\s*(\S+)\s*==\s*(.*)$/) {
        #        print STDERR"succesfully demangled $1 to $2\n";
        $demangle{$1} = $2;
      } else {
        $demangle{$1} = $1;
      }
    }
    close UNDNAME;
    $funcs_to_demangle = "";
    $count = 0;
  }
}

print STDERR "Done.\n";

foreach $func (sort keys %demangle ) {

  next if ($demangle{$func} =~ /vftable/);
  if ($exports{$func} == 0) {
    print "$demangle{$func}: No importers\n";
  } elsif ($exports{$func} == 1) {
    print "$demangle{$func}: One consumer: $last_consumer_of{$func}\n";
  }
}
