#!/usr/local/bin/perl -w                  
#use diagnostics;
use strict;
$::mirrorconfig = [
{
	'from' => {
		'branch' => 'B1',
		'cvsroot' => 'neutron:/var/cvs',
	},
	'to' => [
		{
			'branch' => 'T1',
			'cvsroot' => 'neutron:/var/cvs',
			'offset' => '',
		},
	],
	'mirror' => {
		'directory' => [ 'mirror-test' ],
	},
},
{
	'from' => {
		'branch' => 'T1',
		'cvsroot' => 'neutron:/var/cvs',
	},
	'to' => [
		{
			'branch' => 'TRUNK',
			'cvsroot' => 'neutron:/var/cvs',
			'offset' => 'mirror-test/|modules/mirror-test/foo/',
		},
	],
	'mirror' => {
		'directory' => [ 'mirror-test' ],
	},
},
{
	'from' => {
		'branch' => 'TRUNK',
		'cvsroot' => 'neutron:/var/cvs',
	},
	'to' => [
		{
			'branch' => 'T2',
			'cvsroot' => 'neutron:/var/cvs',
			'offset' => '',
		},
	],
	'mirror' => {
		'directory' => [ 'modules2' ],
	},
},
{
	'from' => {
		'branch' => 'TRUNK',
		'cvsroot' => 'neutron:/var/cvs',
	},
	'to' => [
		{
			'branch' => 'TRUNK',
			'cvsroot' => 'neutron:/var/cvs',
			'offset' => 'modules/|modules2/',
		},
	],
	'mirror' => {
		'directory' => [ 'modules/mirror-test' ],
	},
},
#{
#	'from' => {
#		'branch' => 'BMS_REL_3_0_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'BMS_REL_3_1_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		},
#		{
#			'branch' => 'BMS_REL_3_2_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		},
#		{
#			'branch' => 'BMS_REL_4_0_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		},
#		{
#			'branch' => 'TRUNK',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'directory' => [
#			'tools/config'
#		]
#	},
#	'exclude' => {
#		'file' => [
#			'tools/config/classpath_solaris',
#			'tools/config/classpath_nt'
#		]
#	 },
#},
#{
#	'from' => {
#			'branch' => 'BMS_REL_3_1_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'BMS_REL_3_2_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools'
#		]
#	},
#},
#{
#	'from' => {
#		'branch' => 'BMS_REL_4_0_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'BMS_REL_4_0_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools'
#		]
#	},
#	'exclude' => {
#		'directory' => [
#			'demo',
#			'projects/config',
#			'bmsrc/packages/com/bluemartini/automation',
#			'projects/automation',
#		],
#	},
#},
#{
#	'from' => {
#		'branch' => 'BMS_REL_4_1_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'TRUNK',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'directory' => [
#			'translation'
#		]
#	},
#	'exclude' => {
#		'directory' => [ 'bmsrc/apps/ams' ],
#	},
#},
#{
#	'from' => {
#		'branch' => 'BMS_REL_4_1_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'Marvin_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools'
#		]
#	},
#	'exclude' => {
#		'directory' => [ 'bmsrc/apps/ams' ],
#	},
#},
#{
#	'from' => {
#		'branch' => 'BMS_REL_4_1_M_1_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'Marvin_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools',
#			'BMInstall'
#		]
#	},
#},
#{
#	'from' => {
#		'branch' => 'BMS_REL_4_1_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'TRUNK',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools',
#			'BMInstall'
#		]
#	},
#	'exclude' => {
#		'directory' => [
#			'demo',
#			'bmsrc/packages/com/bluemartini/automation',
#			'projects/automation',
#			'bmsrc/apps/ams'
#		]
#	},
#},
#{
#	'from' => {
#		'branch' => 'Stanford_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'TRUNK',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools',
#			'BMInstall'
#		]
#	},
#	'exclude' => {
#		'directory' => [
#			'demo',
#			'bmsrc/packages/com/bluemartini/automation',
#			'projects/automation'
#		]
#	},
#},
#{
#	'from' => {
#		'branch' => 'TRUNK',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'db2_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools',
#			'BMInstall'
#		],
##		'file' => [
##			'test/foo.sh'
##		],
##		'directory' => [
##			'CVSROOT',
##		]
#	},
#	'exclude' => {
#		'directory' => [
#			'demo'
#		],
##		'file' => [
##			'makefile'
##		],
#	},
#},
##{
##	'from' => {
##		'branch' => 'TRUNK',
##		'cvsroot' => 'neutron:/var/cvs',
##	},
##	'to' => [
##		{
##			'branch' => 'incognitus_Dev_BRANCH',
##			'cvsroot' => 'neutron:/var/cvs',
##			'offset' => '',
##		}
##	],
##	'mirror' => {
##		'module' => [
##			'Vermouth',
##			'BMTools',
##			'BMInstall'
##		]
##	},
##	'exclude' => {
##		'directory' => [
##			'demo'
##		]
##	},
##},
#{
#	'from' => {
#		'branch' => 'db2_Dev_BRANCH',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'incognitus_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'Vermouth',
#			'BMTools',
#			'BMInstall'
#		],
##		'directory' => [
##			'CVSROOT',
##		]
#	},
#	'exclude' => {
#		'directory' => [
#			'demo'
#		],
#		'file' => [
#			'makefile'
#		]
#	},
#},
##
## testing foo below
##
#{
#	'from' => {
#		'branch' => 'TRUNK',
#		'cvsroot' => 'neutron:/var/cvs',
#	},
#	'to' => [
#		{
#			'branch' => 'Test2_Dev_BRANCH',
#			'cvsroot' => 'neutron:/var/cvs',
#			'offset' => '',
#		}
#	],
#	'mirror' => {
#		'module' => [
#			'thj'
#		],
#		'directory' => [
#			'test'
#		],
#		'file' => [
#			'test/foo.sh',
#		],
#	},
#	'exclude' => {
##		'directory' => [
##			'demo'
##		],
#		'file' => [
#			'test/foo.sh',
#		],
#	},
#}
];
return 1;
