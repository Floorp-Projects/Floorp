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
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

use DBD::mysql;

require 'header.pl';

$lockcount = 0;

1;


sub Lock {
    if ($lockcount <= 0) {
        $lockcount = 0;
        if (!open(LOCKFID, ">>data/lockfile")) {
            mkdir "data", 0777;
            chmod 0777, "data";
            open(LOCKFID, ">>data/lockfile") || die "Can't open lockfile.";
        }
        my $val = flock(LOCKFID,2);
        if (!$val) { # '2' is magic 'exclusive lock' const.
            print "Lock failed: $val\n";
        }
        chmod 0666, "data/lockfile";
    }
    $lockcount++;
}

sub Unlock {
    $lockcount--;
    if ($lockcount <= 0) {
        flock(LOCKFID,8);       # '8' is magic 'unlock' const.
        close LOCKFID;
    }
}


sub loadConfigData {
    if (@treelist > 0) {return;}
    local($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
     $atime,$pmtime,$ctime,$blksize,$blocks) = stat("data/configdata.pl");
    local $tmtime;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
     $atime,$tmtime,$ctime,$blksize,$blocks) = stat("data/configdata");

    if ($pmtime eq "" || $pmtime < $tmtime) {
        system "./perlifyconfig.tcl";
    }

    open(CONFIGDATA, "<data/configdata.pl") || die "Can't open configdata.pl";
    while (<CONFIGDATA>) {
        eval;
    }
    close CONFIGDATA;
}


sub pickDefaultRepository {
    loadConfigData();
    return $treeinfo{$treelist[0]}->{'repository'};
}


sub getRepositoryList {
    loadConfigData();
    my @result = ();
    TREELOOP: foreach my $i (@treelist) {
        my $r = $treeinfo{$i}->{'repository'};
        foreach my $j (@result) {
            if ($j eq $r) {
                next TREELOOP;
            }
        }
        push @result, $r;
    }
    return @result;
}


sub validateRepository {
    my ($root) = @_;
    my @list = getRepositoryList();
    foreach my $r (@list) {
        if ($r eq $root) {
            return;
        }
    }
    print "Invalid repository $root selected.  Send mail to terry\@netscape.com if you think this should have worked.\n";
    exit;
}

sub ConnectToDatabase {
    if ($dbh == "") {
        $dbh = DBI->connect("bonsai","bonsai","","mysql") || die "Can't connect to database server -- $DBD::mysql::db_errstr";
    }
    return $dbh;
}

    
sub formatSqlTime {
    my $when = @_[0];
    my($sec,$minute,$hour,$mday,$mon,$year) = localtime( $when );
    return sprintf("%04d-%02d-%02d %02d:%02d:%02d",
                   $year + 1900, $mon + 1, $mday,
                   $hour, $minute, $sec);
}


sub SqlQuote {
    $_ = @_[0];
    s/'/''/g;
    s/\\/\\\\/g;
    return $_;
}


# Returns true if the given directory or filename is one of the hidden ones
# that we don't want to show users.

sub IsHidden {
    my ($name) = (@_);
    $name =~ s:///*:/:g;        # Remove any multiple slashes.
    if (!defined @hidelist) {
        if (open(HIDE, "<data/hidelist")) {
            while (<HIDE>) {
                chop;
                s/^\s*//g;      # Strip leading whitespace
                s/\s*$//g;      # Strip trailing whitespace
                if ( /^#/ || /^$/) {
                    next;
                }
                
                push(@hidelist, $_);
            }
            close HIDE;
        } else {
            @hidelist = ();
        }
    }
    foreach my $item (@hidelist) {
        if ($name =~ m/$item/) {
            return 1;
        }
    }
    return 0;
}

sub CheckHidden {
    my ($name) = (@_);
    if (IsHidden($name)) {
        $| = 1;
        print "";
        die "Security violation; not allowed to access $name.";
    }
}
    
        
