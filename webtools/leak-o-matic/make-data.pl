#!/usr/bin/perl -w
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Leak-o-Matic.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corp.  Portions created by Netscape Communucations
# Corp. are Copyright (C) 1999 Netscape Communications Corp.  All
# Rights Reserved.
# 
# Contributor(s):
# Chris Waterson <waterson@netscape.com>
# 
# $Id: make-data.pl,v 1.3 1999/11/17 19:15:05 waterson%netscape.com Exp $
#

use 5.004;
use strict;
use Getopt::Long;
use POSIX "sys_wait_h";

$::opt_dir = ".";
$::opt_app = "mozilla-bin -f bloaturls.txt";

GetOptions("dir=s", "app=s");

sub ForkAndWait($$$) {
    my ($dir, $app, $timeout) = @_;
    my $pid = fork;

    if ($pid == 0) {
        open(STDOUT, ">/dev/null");
        open(STDERR, ">/dev/null");
        chdir($::opt_dir);
        exec("$app");
        # bye!
    }

    if ($timeout > 0) {
        while ($timeout--) {
            sleep 1;
            my $status = POSIX::waitpid($pid, WNOHANG());
            return 0 if $pid == $status;
            return -1 if $status < 0;
        }

        kill("TERM", $pid);
        return -1;
    }
    else {
        POSIX::waitpid($pid, 0);
    }

    my $status = $? / 256;
    if ($status != 0) {
        die "'$app' terminated abnormally, status == $status";
    }

    return 0;
}

# First, just run the browser with the bloat log turned on. From that,
# we'll capture all of the leaky classes.

my $MasterBloatLog = $ENV{"PWD"} . "/master-bloat.log";

printf("generating top-level class list\n");
$ENV{"XPCOM_MEM_BLOAT_LOG"} = $MasterBloatLog;

ForkAndWait($::opt_dir, $::opt_app, 0);

# Now parse the bloat log.
my @leakyclasses;

{
    open(BLOATLOG, $MasterBloatLog);

    LINE: while (<BLOATLOG>) {
        s/^ +//;
        next LINE unless /^[0-9]/;

        my ($num, $class, $bytesPerInst, $bytesLeaked,
            $totalObjects, $remainingObjects)
            = split(/ +/);

        next LINE unless ($num > 0 && $remainingObjects > 0);

        $leakyclasses[++$#leakyclasses] = $class;
    }
}

# Iterate through each class that leaked, and find out what objects leaked

my $BloatLogFile = "/tmp/leak-report-bloat.log";
$ENV{"XPCOM_MEM_BLOAT_LOG"} = $BloatLogFile;

my $class;
foreach $class (@leakyclasses) {
    printf("+ $class\n");

    delete $ENV{"XPCOM_MEM_REFCNT_LOG"};
    delete $ENV{"XPCOM_MEM_LOG_OBJECTS"};
    $ENV{"XPCOM_MEM_LOG_CLASSES"} = $class;

    ForkAndWait($::opt_dir, $::opt_app, 0);

    open(BLOATLOG, $BloatLogFile);

    my @leakedobjects;
    my $serialNumbersHaveStarted = 0;

    LINE: while (<BLOATLOG>) {
        $serialNumbersHaveStarted = 1
            if /^Serial Numbers of Leaked Objects:/;

        next LINE unless ($serialNumbersHaveStarted && /^[0-9]/);

        chomp;
        $leakedobjects[++$#leakedobjects] = $_;
    }

    my $object;
    foreach $object (@leakedobjects) {
        my $refcntlogfile = $ENV{"PWD"} . "/refcnt-" . $class . "-" . $object . ".log";

        print "|- $refcntlogfile\n";

        $ENV{"XPCOM_MEM_REFCNT_LOG"} = $refcntlogfile;
        $ENV{"XPCOM_MEM_LOG_OBJECTS"} = $object;

        if (ForkAndWait($::opt_dir, $::opt_app, 60) < 0) {
            print "  * Timed out; discarding.\n";
            unlink $refcntlogfile;
        }
    }
}

# Now zip up all of the datafiles into YYYYMMDD.zip
{
    my $zipfile = POSIX::strftime("%Y%m%d.zip", localtime(time));
    system("zip -m $zipfile *.log");
}


