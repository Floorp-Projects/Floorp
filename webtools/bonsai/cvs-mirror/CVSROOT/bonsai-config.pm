#!/usr/local/bin/perl -w
#use diagnostics;
use strict;
$::debug = 0;
$::debug_level =  5;
$::default_db = "development";
$::default_column = "value";
$::default_key = "id";
%::db = (
	"production" => {
		 "dsn" => "dbi:mysql:database=bonsai;host=bonsai2",
		"user" => "rw_bonsai",
		"pass" => "password",
	},
	"development" => {
		 "dsn" => "dbi:mysql:database=bonsai_dev;host=bonsai2",
		"user" => "rw_bonsai_dev",
		"pass" => "password",
	},
);
$::msg = {
	"denied" => {
		"generic" => "CHECKIN ACCESS DENIED",
		    "eol" => "BRANCH (#-branch-#) IS NO LONGER ACTIVE",
		 "closed" => "BRANCH (#-branch-#) IS TEMPORARILY CLOSED",
		 "access" => "INSUFFICIENT PERMISSION TO COMMIT",
	 },
};
@::mirrored_checkin_reqex = (
	"^mirrored checkin from \\S+: ", 
	"^mirrored add from \\S+: ",
	"^mirrored remove from \\S+: ",
	" \\(mirrored checkin from \\S+\\)\$",
	" \\(mirrored add from \\S+\\)\$",
	" \\(mirrored remove from \\S+\\)\$",
);
return 1;
