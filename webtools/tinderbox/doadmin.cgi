#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 


use lib "../bonsai";
require 'lloydcgi.pl';
require 'tbglobals.pl';

umask O666;


$|=1;

tb_check_password();

print "Content-type: text/html\n\n<HTML>\n";

$command = $form{'command'};
$tree= $form{'tree'};

if( $command eq 'create_tree' ){
    &create_tree;
}
elsif( $command eq 'trim_logs' ){
    &trim_logs;
}
elsif( $command eq 'set_status_message' ){
    &set_status_message;
}
elsif( $command eq 'set_rules_message' ){
    &set_rules_message;
}
elsif( $command eq 'set_sheriff' ){
    &set_sheriff;
}
elsif( $command eq 'disable_builds' ){
    &disable_builds;
}
elsif( $command eq 'scrape_builds' ){
    &scrape_builds;
} else {
    print "Unknown command: \"$command\".";
}

sub trim_logs {
    $days = $form{'days'};
    $tree = $form{'tree'};

    print "<h2>Trimming Log files for $tree...</h2>\n<p>";
    
    $min_date = time - (60*60*24 * $days);

    #
    # Nuke the old log files
    #
    $i = 0;
    opendir( D, 'DogbertTip' );
    while( $fn = readdir( D ) ){
        if( $fn =~ /\.gz$/ ){
            ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
                $ctime,$blksize,$blocks) = stat("$tree/$fn");
            if( $mtime && ($mtime < $min_date) ){
                print "$fn\n";
                $tblocks += $blocks;
                unlink( "$tree/$fn" );
                $i++;
            }
        }
    }
    closedir( D );
    $k = $tblocks*512/1024;
    print "<br><b>$i Logfiles ( $k K bytes ) removed</b><br>\n";

    #
    # Trim build.dat
    #
    $builds_removed = 0;
    open(BD, "<$tree/build.dat");
    open(NBD, ">$tree/build.dat.new");
    while( <BD> ){
        ($mailtime,$buildtime,$buildname) = split( /\|/ );
        if( $buildtime >= $min_date ){
            print NBD $_;
        }
        else {
            $builds_removed++;
        }
    }
    close( BD );
    close( NBD );

    rename( "$tree/build.dat", "$tree/build.dat.old" );
    rename( "$tree/build.dat.new", "$tree/build.dat" );

    print "<h2>$builds_removed Builds removed from build.dat</h2>\n";
}

sub create_tree {
    $treename = $form{'treename'};
    my $repository = $form{'repository'};
    $modulename = $form{'modulename'};
    $branchname = $form{'branchname'};
    $bonsaitreename = $form{'bonsaitreename'};

    if( -r $treename ){
        chmod 0777, $treename;
    }
    else {
        mkdir( $treename, 0777 ) || die "<h1> Cannot mkdir $treename</h1>";
    }
    open( F, ">$treename/treedata.pl" );
    print F "\$cvs_module='$modulename';\n";
    print F "\$cvs_branch='$branchname';\n";
    print F "\$bonsai_tree='$bonsaitreename';\n";
    if ($repository ne "") {
        print F "\$cvs_root='$repository';\n";
    }
    close( F );

    open( F, ">$treename/build.dat" );
    close( F );
    
    open( F, ">$treename/who.dat" );
    close( F );

    open( F, ">$treename/notes.txt" );
    close( F );

    chmod 0777, "$treename/build.dat", "$treename/who.dat", "$treename/notes.txt",
                "$treename/treedata.pl";

    print "<h2><a href=showbuilds.cgi?tree=$treename>Tree created or modified</a></h2>\n";
}


sub disable_builds {
    my $i,%buildnames;

    # Read build.dat
    open(BD, "<$tree/build.dat");
    while( <BD> ){
        ($mailtime,$buildtime,$bname) = split( /\|/ );
        $buildnames{$bname} = 0;
    }
    close( BD );

    for $i (keys %form) {
        if ($i =~ /^build_/ ){
            $i =~ s/^build_//;
            $buildnames{$i} = 1;
        }
    }

    open(IGNORE, ">$tree/ignorebuilds.pl");
    print IGNORE '$ignore_builds = {' . "\n";
    for $i ( sort keys %buildnames ){
        if( $buildnames{$i} == 0 ){
            print IGNORE "\t\t'$i' => 1,\n";
        }
    }
    print IGNORE "\t};\n";

    chmod( 0777, "$tree/ignorebuilds.pl");
    print "<h2><a href=showbuilds.cgi?tree=$tree>Build state Changed</a></h2>\n";
}


sub scrape_builds {
    my $i,%buildnames;

    # Read build.dat
    open(BD, "<$tree/build.dat");
    while( <BD> ){
        ($mailtime,$buildtime,$bname) = split( /\|/ );
        $buildnames{$bname} = 1;
    }
    close( BD );

    for $i (keys %form) {
        if ($i =~ /^build_/ ){
            $i =~ s/^build_//;
            $buildnames{$i} = 0;
        }
    }

    open(SCRAPE, ">$tree/scrapebuilds.pl");
    print SCRAPE '$scrape_builds = {' . "\n";
    for $i ( sort keys %buildnames ){
        if( $buildnames{$i} == 0 ){
            print SCRAPE "\t\t'$i' => 1,\n";
        }
    }
    print SCRAPE "\t};\n";

    chmod( 0777, "$tree/scrapebuilds.pl");
    print "<h2><a href=showbuilds.cgi?tree=$tree>Build state Changed</a></h2>\n";
}


sub set_sheriff {
    $m = $form{'sheriff'};
    $m =~ s/\'/\\\'/g;
    open(SHERIFF, ">$tree/sheriff.pl");
    print SHERIFF "\$current_sheriff = '$m';\n1;";
    close(SHERIFF);
    chmod( 0777, "$tree/sheriff.pl");
    print "<h2><a href=showbuilds.cgi?tree=$tree>
            Sheriff Changed.</a><br></h2>\n";
}

sub set_status_message {
    $m = $form{'status'};
    $m =~ s/\'/\\\'/g;
    open(TREESTATUS, ">$tree/status.pl");
    print TREESTATUS "\$status_message = \'$m\'\;\n1;";
    close(TREESTATUS);
    chmod( 0777, "$tree/status.pl");
    print "<h2><a href=showbuilds.cgi?tree=$tree>
            Status message changed.</a><br></h2>\n";
}

sub set_rules_message {
    $m = $form{'rules'};
    $m =~ s/\'/\\\'/g;
    open(RULES, ">$tree/rules.pl");
    print RULES "\$rules_message = \'$m\';\n1;";
    close(RULES);
    chmod( 0777, "$tree/rules.pl");
    print "<h2><a href=showbuilds.cgi?tree=$tree>
            Rule message changed.</a><br></h2>\n";
}

