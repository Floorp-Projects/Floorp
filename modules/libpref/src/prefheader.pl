#!/usr/local/bin/perl
# usually you just run $(PERL) this file though

while (<ARGV>) {
  if (/(config|pref|localDefPref)\s*\(\"([^\"]*)\"/) {
    # $2 now contains the pref name
    $define = $pref_name = $2;
    $define =~ tr/a-z/A-Z/;
    $define =~ s/\./_/g;

    # uniqify the list by dumping everything into a hash
    $preftable{"PREF_$define"} = "\"$pref_name\""
      
  }


}

# print out the final table
foreach $pref (sort keys %preftable) {
  print "#define $pref 	$preftable{$pref}\n";
}
