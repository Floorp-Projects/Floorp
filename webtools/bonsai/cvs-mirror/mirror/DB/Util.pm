package DB::Util;

use DBI;
use strict;

sub connect {
	unless ($::dbh->{'Active'}) {
		$::dbh = DBI->connect(
			$::db{$default::db}{'dsn'},
			$::db{$default::db}{'user'},
			$::db{$default::db}{'pass'},
			{
				PrintError => 1,
				RaiseError => 1
			}
		);
	}
}

sub disconnect {
	$::dbh->disconnect() if ($::dbh->{'Active'});
}

sub id {
#use Data::Dumper;
#print Dumper(\@_);
	my ($table, $value, $hashref) = @_;
	my ($column, $key, $ro, $id);
	unless ($column = ${$hashref}{"column"}) { $column = $default::column }
	unless ($key = ${$hashref}{"key"}) { $key = $default::key }
	unless ($ro = ${$hashref}{"read_only"}) { $ro = 0 }
	unless ($id = $::dbh->selectrow_array("SELECT $key FROM `$table` WHERE $column = ?", undef, $value)) {
		unless ($ro) {
			$::dbh->do("INSERT INTO `$table` SET $column = ?", undef, $value);
			$id = $::dbh->selectrow_array("SELECT LAST_INSERT_ID()");
		} else {
			die "\nThe value \"$value\" was not found in column \"$column\" of table \"$table\" during a read-only ID lookup operation.\n\n";
		}
	}
#-debug-# print "\n$id\n\n";
	return $id;
}

sub retrieve {
    my ($table, $where_ref, $hashref) = @_;
    my ($column, $value, $where, @bind);
    unless ($column = ${$hashref}{"column"}) { $column = $default::column }
    while (my ($col, $val) = each %$where_ref) {
        $where .= $col ." = ? AND ";
        if ($col =~ /.*_id$/) {
            $col =~ s/_id$//;
            push @bind, &id($col, $val);
        } else {
            push @bind, $val;
        }
    }
    $where .= "1";
    $value = $::dbh->selectrow_array("SELECT $column FROM `$table` WHERE $where ORDER BY id DESC LIMIT 1", undef, @bind);
#print "SELECT $column FROM $table WHERE $where ORDER BY id DESC LIMIT 1", @bind;
#for my $i (@bind) { print "$i\n" }
    return $value;
}

return 1;

__END__
