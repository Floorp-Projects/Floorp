package DB::Select;

use DBI;
use strict;
use Sys::Hostname;

sub mirrors {
	my $sth = $::dbh->prepare("
		SELECT
			m.id, m.checkin_id, b.value, r.value, o.value
		FROM
			checkin c, mirror m, branch b, cvsroot r, offset o, status s
		WHERE
				c.id = m.checkin_id
			AND b.id = m.branch_id
			AND r.id = m.cvsroot_id 
			AND o.id = m.offset_id 
			AND s.id = m.status_id
			AND c.time < ? - ?
			AND s.value = ?
			AND r.value RLIKE ?
		ORDER BY
			c.time, checkin_id, m.id
	");
	my $arrayref = $::dbh->selectall_arrayref(
		$sth,
		undef,
		time,
		$::mirror_delay,
		shift,
		'^' . Sys::Hostname::hostname() . ':.*$'
	);
	$sth->finish();
	return $arrayref;
}

sub checkin {
	my $sth = $::dbh->prepare("
		SELECT
			u.value as user, d.value as directory, l.value as log, r.value as cvsroot
		FROM
			checkin c, user u, directory d, log l, cvsroot r
		WHERE
				u.id = c.user_id
			AND d.id = c.directory_id
			AND l.id = c.log_id
			AND r.id = c.cvsroot_id
			AND c.id = ?
		LIMIT 1
	");
	$sth->execute(shift);
	my $hashref = $sth->fetchrow_hashref();
	$sth->finish();
	return $hashref;
}

sub change {
	my $sth = $::dbh->prepare("
		SELECT
			f.value as file, ch.oldrev, ch.newrev, b.value as branch
		FROM
			`change` ch, file f, branch b
		WHERE
				 f.id = ch.file_id
			AND  b.id = ch.branch_id
			AND ch.id = ?
		LIMIT 1
	");
	$sth->execute(shift);
	my $hashref = $sth->fetchrow_hashref();
	$sth->finish();
	return $hashref;
}

sub mirror_changes {
	my $sth = $::dbh->prepare("
		SELECT
			mcm.change_id, t.value as type
		FROM
			mirror_change_map mcm, type t, status s
		WHERE
				t.id = mcm.type_id
			AND s.id = mcm.status_id
			AND s.value = ?
			AND mcm.mirror_id = ?
	");
	my $arrayref = $::dbh->selectall_arrayref(
		$sth,
		undef,
		@_
	);
	$sth->finish();
	return $arrayref;
}

sub runtime {
	my $sth = $::dbh->prepare("
		SELECT
			c.value as command,
			ri.mirror_delay,
			ri.min_scan_time,
			ri.throttle_time,
			ri.max_addcheckins,
			ri.last_update,
			ri.mh_command_response as response,
			ri.id
		FROM
			mh_runtime_info ri, mh_command c
		WHERE
				ri.mh_hostname_id = ?
			AND ri.mh_command_id = c.id
		ORDER BY
			ri.id DESC,
			ri.time DESC
		LIMIT 1
	");
	$sth->execute(shift);
	my $hashref = $sth->fetchrow_hashref();
	$sth->finish();
	return $hashref;
}

sub branch_eol {
	my ($r, @ba) = @_;
	return $::dbh->selectcol_arrayref("
		SELECT
			b.value
		FROM
			`cvsroot_branch_map_eol` m, `cvsroot` r, `branch` b
        WHERE
				r.id = m.cvsroot_id
			AND b.id = m.branch_id
			AND (b.value = ?" . (" OR b.value = ?" x $#ba) . ")
		",
        undef,
		@ba
	);
}

return 1;

__END__
