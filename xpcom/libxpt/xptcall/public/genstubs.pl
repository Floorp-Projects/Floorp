#!/usr/local/bin/perl

# This is used to generate stub entry points. We generate a file to
# be included in the declaraion and a file to be used for expanding macros
# to represent the implementation of the stubs.

$entry_count    = 256;
$sentinel_count = 10;

$decl_name = "xptcstubsdecl.inc";
$def_name  = "xptcstubsdef.inc";

##
## Write the declarations include file
##

die "Can't open $decl_name" if !open(OUTFILE, ">$decl_name");

print OUTFILE "// generated file - DO NOT EDIT \n\n";
print OUTFILE "// includes ",$entry_count," stub entries, and ",
              $sentinel_count," sentinel entries\n\n";
print OUTFILE "// declarations of normal stubs...\n";
print OUTFILE "// 0 is QueryInterface\n";
print OUTFILE "// 1 is AddRef\n";
print OUTFILE "// 2 is Release\n";
for($i = 0; $i < $entry_count; $i++) {
    print OUTFILE "XPTC_EXPORT NS_IMETHOD Stub",$i+3,"();\n";
}

print OUTFILE "\n// declarations of sentinel stubs\n";

for($i = 0; $i < $sentinel_count; $i++) {
    print OUTFILE "XPTC_EXPORT NS_IMETHOD Sentinel",$i,"();\n";
}
close(OUTFILE);


##
## Write the definitions include file. This assumes a macro will be used to
## expand the entries written...
##

die "Can't open $def_name" if !open(OUTFILE, ">$def_name");

print OUTFILE "// generated file - DO NOT EDIT \n\n";
print OUTFILE "// includes ",$entry_count," stub entries, and ",
              $sentinel_count," sentinel entries\n\n";

for($i = 0; $i < $entry_count; $i++) {
    print OUTFILE "STUB_ENTRY(",$i+3,")\n";
}

for($i = 0; $i < $sentinel_count; $i++) {
    print OUTFILE "SENTINEL_ENTRY(",$i,")\n";
}
