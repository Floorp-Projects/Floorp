package default;
use strict;
#
# addresses to which to complain
#
$default::admin_address = 'release-eng@bluemartini.com';
$default::pager_address = 'vajonez@yahoo.com';
#
# default times (seconds)
#
	# how long to wait after a checkin to start mirroring
	# this is mostly to give folks a time to nomirror
$default::mirror_delay    = 15 * 60;
	# let's be kind and not hammer the network/database.
	# minimum time between checks of the database
$default::min_scan_time   = 10;
	# the old bonsai code uses a really inefficient means
	# of getting checkin info into the database. each
	# addcheckin.pl process consumes ~8MB of memory and
	# take several seconds to run. The following number
	# is the number of addcheckins that we'd like to see
	# running at any one time. If the number of addcheckin.pl's
	# exceeds the number below, wait throttle_time seconds
	# and try again.
$default::max_addcheckins = 20;
$default::throttle_time   = 5;
#
# Database stuff (pick the correct one!)
#
$default::db = "development";
$default::column = "value";
$default::key = "id";
%::db = (
	"production" => {
		 "dsn" => "dbi:mysql:database=bonsai;host=bonsai2",
		"user" => "bonsai_mh",
		"pass" => "password",
	},
	"development" => {
		 "dsn" => "dbi:mysql:database=bonsai_dev;host=bonsai2",
		"user" => "bonsai_dev_mh",
		"pass" => "password",
	},
);
return 1;
