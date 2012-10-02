#! /usr/local/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


require('coreconf.pl');

#######-- read in variables on command line into %var

$use_jar = 1;
$ZIP     = "$ENV{JAVA_HOME}/bin/jar";

if ( $ENV{JAVA_HOME} eq "" ) {
    $ZIP      = "zip";
    $use_jar  = 0;
}


&parse_argv;

 
######-- Do the packaging of jars.

foreach $jarfile (split(/ /,$var{FILES}) ) {
    print STDERR "---------------------------------------------\n";
    print STDERR "Packaging jar file $jarfile....\n";

    $jarinfo = $var{$jarfile};

    ($jardir,$jaropts) = split(/\|/,$jarinfo);

    if ( $use_jar ) {
        $zipoptions = "-cvf";
    } else {
        $zipoptions = "-T -r";
        if ($jaropts =~ /a/) {
            if ($var{OS_ARCH} eq 'WINNT') {
                $zipoptions .= ' -ll';
            }
        }
    }

# just in case the directory ends in a /, remove it
    if ($jardir =~ /\/$/) {
	chop $jardir;
    }

    $dirdepth --;
    
    print STDERR "jardir = $jardir\n";
    system("ls $jardir");

    if (-d $jardir) {


# count the number of slashes

	$slashes =0;
	
	foreach $i (split(//,$jardir)) {
	    if ($i =~ /\//) {
		$slashes++;
	    }
	}

	$dotdots =0;
	
	foreach $i (split(m|/|,$jardir)) {
	    if ($i eq '..') {
		$dotdots ++;
	    }
	}

	$dirdepth = ($slashes +1) - (2*$dotdots);

	print STDERR "changing dir $jardir\n";
	chdir($jardir);
	print STDERR "making dir META-INF\n";
	mkdir("META-INF",0755);

	$filelist = "";
	opendir(DIR,".");
	while ($_ = readdir(DIR)) {
	    if (! ( ($_ eq '.') || ($_ eq '..'))) {
		if ( $jaropts =~ /i/) {
		    if (! /^include$/) {
			$filelist .= "$_ ";
		    }
		}
		else {
		    $filelist .= "$_ ";
		}
	    }
	}
	closedir(DIR);	

	print STDERR "$ZIP $zipoptions $jarfile $filelist\n";
	system("$ZIP $zipoptions $jarfile $filelist");
	rmdir("META-INF");
	    for $i (1 .. $dirdepth) {
	    chdir("..");
	    print STDERR "chdir ..\n";
	}
    }
    else {
        print STDERR "Directory $jardir doesn't exist\n";
    }

}

