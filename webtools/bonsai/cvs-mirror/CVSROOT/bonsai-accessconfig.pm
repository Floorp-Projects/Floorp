#!/usr/local/bin/perl -w   
#use diagnostics;
use strict;
$::accessconfig = [
#{
#	'cvsroot' => 'neutron:/var/cvs',
#	'branch' => '#-all-#',
#	'location' => {
#		'file' => [ 'test/foo.sh' ],
#	},
#	'close' => 'Hey! You Suck!',
#	'bless' => {
#		'user' => [ 'thj' ],
#	},
#	'permit' => {
#		'unix_group' => [ 'ebuildrel' ],
#	},
#	'deny_msg' => 'buildrel group has been naughty',
#},
#{
#	'cvsroot' => 'neutron:/var/cvs',
#	'branch' => 'TRUNK',
#	'location' => {
#		'file' => [ 'test/foo.sh' ],
#	},
#	'close' => 'BRANCH closed, so go away.',
#	'bless' => {
#		'bonsai_group' => [ 'test-2' ],
#	},
#	'permit' => {
#		'bonsai_group' => [ 'test-2' ],
#	},
#	'deny_msg' => 'i don\'t like people',
#},
#{
#	'cvsroot' => 'neutron:/var/cvs',
#	'branch' => '#-all-#',
#	'location' => {
#		'module' => [ ],
#		'directory' => [ 'test/foo/', 'blah/blah/blah' ],
###		'file' => [ 'test/foo.sh' ],
#	},
#	'close' => 0,
##	'permit' => {
###		'unix_group' => [ ],
###		'bonsai_group' => [ ],
###		'user' => [ ],
##	},
##	'deny' => {
##		'unix_group' => [ ],
##		'bonsai_group' => [ ],
##		'user' => [ ],
##	},
##	'bless' => {
##		'unix_group' => [ ],
##		'bonsai_group' => [ 'blessed' ],
##		'user' => [ ],
##	},
#},
#{
#	'cvsroot' => 'neutron:/var/cvs',
#	'branch' => 'TRUNK',
#	'location' => {
#		'file' => [ 'test/foo.sh' ],
#	},
#	'close' => 'Blah, blah, blah.',
#	'bless' => {
#		'user' => [ 'thj' ],
#	},
#	'permit' => {
#		'user' => [ 'thj' ],
#	},
#},
##{
##	'cvsroot' => '#-all-#',
##	'branch' => '#-all-#',
##	'location' => {
##		'directory' => [ 'CVSROOT' ],
##	},
###	'close' => "TESTING TESTING TESTING",
##	'permit' => {
##		'unix_group' => [ 'buildrel' ],
##	},
##},
##{
##	'cvsroot' => 'neutron:/var/cvs',
##	'branch' => 'XML_Dev_BRANCH',
##	'location' => {
##		'module' => [ ],
##		'directory' => [ ],
##		'file' => [ 'test/foo.sh' ],
##	},
##	'close' => 0,
##	'permit' => {
##		'unix_group' => [ ],
##		'bonsai_group' => [ ],
##		'user' => [ ],
##	},
##	'deny' => {
##		'unix_group' => [ ],
##		'bonsai_group' => [ ],
##		'user' => [ ],
##	},
##	'bless' => {
##		'unix_group' => [ ],
##		'bonsai_group' => [ ],
##		'user' => [ ],
##	},
##},
##{
##	'cvsroot' => 'neutron:/var/cvs2',
##	'branch' => 'FOO',
##	'location' => {
##		'module' => [ 'Vermouth' ],
##	},
##	'close' => 0,
##},
{
	'cvsroot' => 'neutron:/var/cvs',
	'branch' => 'T2',
	'location' => {
#		'module' => [ 'Vermouth' ],
#		'file' => [ 'bmsrc/apps/ams/foo.pl' ],
		'directory' => [ 'mirror-test' ]
	},
#	'permit' => { },
#	'close' => "closed",
},
];
return 1;
