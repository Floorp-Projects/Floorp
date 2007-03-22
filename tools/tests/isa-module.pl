#!/usr/bin/perl -w

use Cwd;

$curdir = cwd();

@path_fields = split(/\//,$curdir);

$path = join ("/",@path_fields);

print "Searching in " . $path . "\n";

open (REPORT, "find $path -name \"*.so\" -print | xargs nm -Bno | egrep NSGetModule 2>&1 |" ) || die "open: $! \n";

print "Modules:\n";
while (<REPORT>) {
    $module = $_;
    $module =~ s/:.*//;
    print $module;
}
close(REPORT);

open (REPORT, "find $path -name \"*.so\" -print | xargs nm -Bno | egrep NSGetFactory 2>&1 |" ) || die "open: $! \n";

print "\nComponents:\n";
while (<REPORT>) {
    $module = $_;
    $module =~ s/:.*//;
    print $module;
}
close(REPORT);
