package DB::Insert;

use DBI;
use strict;

sub exec_log {
#use Data::Dumper;
#print Dumper(\@_);
	$::dbh->do("
		INSERT INTO
			`exec_log`
		SET
			time = UNIX_TIMESTAMP(),
			command = ?,
			stdout = ?,
			stderr = ?,
			exit_value = ?,
			signal_num = ?,
			dumped_core = ?
	", undef, @_);
	return $::dbh->selectrow_array("SELECT LAST_INSERT_ID()");
}

sub mirror_change_exec_map {
#use Data::Dumper;
#print Dumper(\@_);
	$::dbh->do("
		INSERT INTO
			`mirror_change_exec_map`
		SET
			mirror_id = ?,
			change_id = ?,
			exec_log_id = ?
	", undef, @_);
}

return 1;

__END__
