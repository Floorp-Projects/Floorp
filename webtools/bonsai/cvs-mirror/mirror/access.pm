package access;
use strict;
use Data::Dumper;


sub allowed {
    my ($file, $type_hashref) = @_;
    my %x = ('exclude' => 0, 'include' => 1);
#   for my $st ("module", "directory", "file") {
    for my $st ("file", "directory", "module") {
        if (defined $type_hashref->{$st}) {
#           print "=== $st", Dumper($type_hashref->{$st});
            for my $type ("file", "directory") {
                for my $clude ("exclude", "include") {
                    if (defined $type_hashref->{$st}->{$clude."_".$type}) {
                        return $x{$clude} if &match_array($file, $type_hashref->{$st}->{$clude."_".$type}) ;
                    }
                }
            }
        }
    }
    return 0;
}

sub match_array {
    my ($file, $regex_arrayref) = @_;
#    $file = $directory."/".$file if $directory;
    for my $r (@$regex_arrayref) {
##
#       print "#####".$file."#####".$r."#####";
#       if ($file =~ /$r/) {
#           print " MATCH\n";
#           return 1;
#       } else {
#           print " NO-MATCH\n";
#       }
##
        return 1 if $file =~ /$r/ ;
    }
    return 0;
}

sub rule_applies {
    my ($ah, $cvsroot, $branch, $file) = @_;
    my $return = 0;
	if (($cvsroot eq $ah->{'cvsroot'} || $ah->{'cvsroot'} eq "#-all-#") &&
		($branch  eq $ah->{'branch'}  || $ah->{'branch'}  eq "#-all-#")) {
		return &allowed($file, $ah->{'location'});
	}
	return 0;
}
#next if &access::closed{$accessconfig, $to_branch, $directory, $file);
sub closed {
	my ($accessconfig, $cvsroot, $branch, $directory, $file) = @_;
	$file = $directory."/".$file if $directory;
	for my $access_rule (@$accessconfig) {
		if (&rule_applies($access_rule, $cvsroot, $branch, $file)) {
			return 1 if ( $access_rule->{'close'} ); # { print "\nclosed\n\n" }
		}
	}
	return 0;
}

return 1;

