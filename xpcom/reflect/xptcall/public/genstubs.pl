#!/usr/local/bin/perl

# This is used to generate stub entry points. We generate a file to
# be included in the declaraion and a file to be used for expanding macros
# to represent the implementation of the stubs.

#
# if "$entry_count" is ever changed and the .inc files regenerated then
# the following issues need to be addressed:
#
# 1) Alpha NT has a .def file that lists exports by symbol. It will need
#    updating.
# 2) The current Linux ARM code has a limitation of only having 256-3 stubs
#
# more dependencies???
#

# 3 entries are already 'used' by the 3 methods of nsISupports.
# 3+247+5=255 This should get us in under the Linux ARM limitation
$entry_count    = 247;
$sentinel_count = 5;

$decl_name = "xptcstubsdecl.inc";
$def_name  = "xptcstubsdef.inc";

##
## Write the declarations include file
##

die "Can't open $decl_name" if !open(OUTFILE, ">$decl_name");

print OUTFILE "/* generated file - DO NOT EDIT */\n\n";
print OUTFILE "/* includes ",$entry_count," stub entries, and ",
              $sentinel_count," sentinel entries */\n\n";
print OUTFILE "/*\n";
print OUTFILE "*  declarations of normal stubs...\n";
print OUTFILE "*  0 is QueryInterface\n";
print OUTFILE "*  1 is AddRef\n";
print OUTFILE "*  2 is Release\n";
print OUTFILE "*/\n";
print OUTFILE "#if !defined(__ia64) || (!defined(__hpux) && !defined(__linux__))\n";
for($i = 0; $i < $entry_count; $i++) {
    print OUTFILE "NS_IMETHOD Stub",$i+3,"();\n";
}
print OUTFILE "#else\n";
for($i = 0; $i < $entry_count; $i++) {
    print OUTFILE "NS_IMETHOD Stub",$i+3,"(PRUint64,\n";
    print OUTFILE " PRUint64,PRUint64,PRUint64,PRUint64,PRUint64,PRUint64);\n";

}
print OUTFILE "#endif\n";

print OUTFILE "\n/* declarations of sentinel stubs */\n";

for($i = 0; $i < $sentinel_count; $i++) {
    print OUTFILE "NS_IMETHOD Sentinel",$i,"();\n";
}
close(OUTFILE);


##
## Write the definitions include file. This assumes a macro will be used to
## expand the entries written...
##

die "Can't open $def_name" if !open(OUTFILE, ">$def_name");

print OUTFILE "/* generated file - DO NOT EDIT */\n\n";
print OUTFILE "/* includes ",$entry_count," stub entries, and ",
              $sentinel_count," sentinel entries */\n\n";

for($i = 0; $i < $entry_count; $i++) {
    print OUTFILE "STUB_ENTRY(",$i+3,")\n";
}

for($i = 0; $i < $sentinel_count; $i++) {
    print OUTFILE "SENTINEL_ENTRY(",$i,")\n";
}
