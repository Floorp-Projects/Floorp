package DB::Update;

use DBI;
use strict;

use DB::Util;

BEGIN {
}

sub mirror {
#use Data::Dumper;
#print Dumper(\@_);
	my ($id, $status) = @_;
	$::dbh->do("
		UPDATE
			`mirror`
		SET
			status_id = ?
		WHERE
			id = ?
		",
		undef,
		&DB::Util::id('status', $status, {'read_only' => 1}),
		$id
	);
}

sub mirror_change {
#use Data::Dumper;
#print Dumper(\@_);
	my ($mid, $chid, $status) = @_;
	$::dbh->do("
		UPDATE
			`mirror_change_map`
		SET
			status_id = ?
		WHERE
				mirror_id = ?
			AND change_id = ?
		",
		undef,
		&DB::Util::id('status', $status, {'read_only' => 1}),
		$mid,
		$chid
	);
}

sub runtime {
#use Data::Dumper;
#print Dumper(\@_);
	my ($r, $id) = @_;
	$r++;
	$::dbh->do("
		UPDATE
			`mh_runtime_info`
		SET
			mh_command_response = ?
		WHERE
			id = ?
		",
		undef,
		$r,
		$id
	);
}

return 1;

__END__
