# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# Isolate the calls to oracle for PVCS we may need to reuse these with
# other oracle products some day.

# The contents of this file are subject to the Mozilla Public
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

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


# $Revision: 1.2 $ 
# $Date: 2005/11/25 19:47:47 $ 
# $Author: timeless%mozdev.org $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/lib/PVCSGetData.pm,v $ 
# $Name:  $ 


# Standard perl libraries

package PVCSGetData;

use Time::Local;


$SQL_USER = $TinderConfig::VC_PVCS_USER || 'tinderbox/tinderbox';
$SEP = $TinderConfig::VC_PVCS_SEPARATOR || '___///___';

sub time2pvcsFormat {
  # convert time() format to the format which appears in perforce output
  my ($time) = @_;

  my ($sec,$min,$hour,$mday,$mon,
      $year,$wday,$yday,$isdst) =
        localtime($time);

  $year += 1900;
  $mon++;

  my $date_str = sprintf("%04u/%02u/%02u/%02u/%02u/%02u", 
                         $year, $mon, $mday, $hour, $min, $sec);

  return ($date_str);
}

sub pvcs_date_str2time {      
    my ($pvcs_date_str) = @_;
    
    my ($year, $mon, $mday, $hour, $min, $sec) = 
        split('/', $pvcs_date_str);

    $mon--;

    my ($time) = timelocal($sec,$min,$hour,$mday,$mon,$year);    
    
    return $time;
}


sub pvcs_trim_whitespace {
    my ($line) = (@_);

    $line =~ s{\t+}{}g;
    $line =~ s{\n+}{}g;
    $line =~ s{ +}{}g;

    return $line;    
}

# Adapted from oraselect()
# code by Ulrich Herbst <Ulrich.Herbst@gmx.de> 
# found at
# http://servdoc.sourceforge.net/docs/pod/ServDoc_0310oracle.pod.html

sub get_oraselect_separator {
    return $SEP;
}

sub oraselect {
    my ($sqltable) = @_;
     
    my $tmpfile       = "/tmp/oracle.$$.sql";
    my $SQLCMD        = 'sqlplus';
    $OSNAME = $^O;

    if ($OSNAME =~ /MSWin32/) {
        $SQLCMD='plus80';
        $tmpfile='c:\TEMP\oracle.$$.sql';
        $ADDTOPATH="$ENV{ORACLE_HOME}\\bin;";
    }

    # We don't use pipes to input sqlplus because that doesn't work
    # for windows.

    my $sqlheader = <<"EOSQL";

    set pages 0
    set pagesize 10000
    set linesize 10000

    set heading off
    set feedback off
    set serveroutput on

    set colsep '$SEP'
    alter session set NLS_DATE_FORMAT='yyyy/mm/dd/hh24/mi/ss';

EOSQL
    ;

    open TEMPFH, ">$tmpfile";
    print TEMPFH "$sqlheader\n";
    print TEMPFH "$sqltable\n";
    print TEMPFH "$select; \n";
    print TEMPFH "exit;\n";
    close TEMPFH;

    my $executestring;
    if ($OSNAME =~ /MSWin32/) {
        $executestring = "$SQLCMD -s '$SQL_USER' \@$tmpfile 2>&1";
    } else {
        $executestring =
            "source /etc/profile; ".
            "$SQLCMD -s \'$SQL_USER\' \@$tmpfile 2>&1";
    }

     my @sqlout=`$executestring`;

    # I do not thin sqlplus ever returns non zero, but I wish to be
    # complete to I check it. Real errors are harder to discribe, we
    # use pattern matching to find strings like:

    #      SP2-0042: unknown command "asdf" - rest of line ignored.
    #      ORA-00923: FROM keyword not found where expected

     my $rc = $?; 
    if ( 
         ($rc) ||
         ("@sqlout" =~ m/\nERROR at line /) ||
         ("@sqlout" =~ m/\n[A-Z0-9]+\-\d+\:/) ||
         0) {
	 die(@sqlout);
     }

    unlink ($tmpfile);
    
     
    return \@sqlout;
} # oraselect

1;
