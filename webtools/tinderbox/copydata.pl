#!/tools/ns/bin/perl5
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

#
# to run this script execute 'nohup copydata.pl &' from the tinderbox directory
#

$start_dir = `pwd`;
chop($start_dir);
$scp_cmd = "scp  -o 'User snapshot' -o'Port 22' -o 'UserKnownHostsFile /u/shaver/snapshot/known_hosts' -o 'IdentityFile /u/shaver/snapshot/identity'";


$last_time = 0;

$min_cycle_time = 3 * 60;  # 3 minutes

print "starting dir is :$start_dir\n";

while( 1 ){
    chdir("$start_dir");
    if( time - $last_time < $min_cycle_time ){
        $sleep_time = $min_cycle_time - (time - $last_time);
        print "\n\nSleeping $sleep_time seconds ...\n";
        sleep( $sleep_time );
    }

    &copy_data("Mozilla");
    &copy_data("raptor");

    chdir( "$start_dir");
    print  "$scp_cmd * cvs1.mozilla.org:/e/webtools/tinderbox\n";
    system "$scp_cmd * cvs1.mozilla.org:/e/webtools/tinderbox";

    chdir( "$start_dir/../bonsai");
    print  "$scp_cmd * cvs1.mozilla.org:/e/webtools/bonsai\n";
    system "$scp_cmd * cvs1.mozilla.org:/e/webtools/bonsai";


    $last_time = time;
}

1;


sub copy_data {
    local($data_dir) = @_;
    local($zips,$qry);

chdir $data_dir || die "couldn't chdir to $data_dir";

system "echo hello >lastup.new";

if( -r 'lastup' ) {
    $qry = '-newer lastup.old';
    rename 'lastup', 'lastup.old'
}
rename 'lastup.new', 'lastup';


open( FINDER, "find . $qry -name \"*.gz\" -print|" );
while(<FINDER>){
    print;
    chop;
    $zips .= "$_ ";
}
close( FINDER );

unlink 'lastup.old';

print "$scp_cmd *.txt $zips *.dat cvs1.mozilla.org:/e/webtools/tinderbox/$data_dir\n";
system "$scp_cmd *.txt $zips *.dat cvs1.mozilla.org:/e/webtools/tinderbox/$data_dir";


chdir $start_dir || die "couldn't chdir to $start_dir";
}
