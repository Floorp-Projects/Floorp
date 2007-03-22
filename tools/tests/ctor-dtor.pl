#!/usr/bin/perl

use Cwd;

$curdir = cwd();

open( LOG, ">ctor-dtor-report" ) || print "can't open $?\n";

@path_fields = split(/\//,$curdir);

$path = join ("/",@path_fields);

print "Searching in " . $path . "\n";
open (REPORT, "find $path -name \"*.o\" -print | xargs nm -Bno | egrep \"_GLOBAL_\.[ID]|_tcf\" 2>&1 |" ) || die "open: $! \n";

while (<REPORT>) {
    print $_;
    print LOG $_;
}
close(REPORT);

if (-s "./ctor-dtor-report" > 0) {
    print "Global Constructors\/Destructors Found" . "\n";
}
