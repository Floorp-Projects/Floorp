# -*- Mode: perl; indent-tabs-mode: nil -*-
#

# This file interfaces PVCS into the original Bonsai code.  The
# resulting Bonsai hack will allow users to query the PVCS database
# using SQL.  Many of the original bonsai features are not applicable
# in this environment and have been removed.

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

# complete rewrite by Ken Estes:
#	 kestes@staff.mail.com Old work.
#	 kestes@reefedge.com New work.
#	 kestes@walrus.com Home.
# Contributor(s): 


# $Revision: 1.1 $ 
# $Date: 2003/12/24 12:16:36 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/bonsai_pvcs/pvcs_query_checkins.pl,v $ 
# $Name:  $ 

require Time::Local;
require Data::Dumper;



$sql_user = 'tinderbox/tinderbox';
$sep = '___///___';

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
    return $sep;
}

sub oraselect {
    my ($sqltable) = @_;
     
    my $tmpfile       = "/tmp/oracle.$$.sql";
    my $SQLCMD        = 'sqlplus';
    $OSNAME = $^O;

    if ($OSNAME =~ /MSWin32/) {
        $SQLCMD='plus80';
        $tmpfile='c:\TEMP\oracle.$$.sql';
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

    set colsep '$sep'
    alter session set NLS_DATE_FORMAT='yyyy/mm/dd/hh24/mi/ss';

EOSQL
    ;

    open TEMPFH, ">$tmpfile";
    print TEMPFH "$sqlheader\n";
    print TEMPFH "$sqltable\n";
    print TEMPFH "exit;\n";
    close TEMPFH;

    my $executestring;
    if ($OSNAME =~ /MSWin32/) {
        $executestring = "$SQLCMD -s '$sql_user' \@$tmpfile 2>&1";
    } else {
        $executestring =
            "source /etc/profile; ".
            "$SQLCMD -s \'$sql_user\' \@$tmpfile 2>&1";
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
}



sub create_where_stmt {
    
    my (
        $query_module,
        $query_branch,          
        $query_branch_head,
        $query_branchtype,       
        $query_who,
        $query_whotype,
        
        $query_date_max,         
        $query_date_min,        
        $query_logexpr,       
        
        $query_file,       
        $query_filetype,      
        @query_dirs,     
        ) = @_;
    
    if (
        ( $query_module eq 'allrepositories' ) ||
        ( $query_module eq 'all' ) ||
        ( $query_module eq 'All' ) ||
        0){
        1;
    } else {
        my $q = SqlQuote($query_module);
        $qstring1 .= "PCMS_ITEM_DATA.\"PRODUCT_ID\" = $q AND ";
    }

    if (
        ($query_branch_head) ||
        ($query_branch eq 'head') ||
        ($query_branch eq 'HEAD') ||
        0) {
        1;
    } elsif ($query_branch ne '') {
        my $q = SqlQuote($query_branch);
        if ($query_branchtype eq 'regexp') {
            $qstring1 .=
                " PCMS_WORKSET_INFO.\"WORKSET_NAME\" LIKE $q AND ";
        } elsif ($query_branchtype eq 'notregexp') {
            $qstring1 .=
                " PCMS_WORKSET_INFO.\"WORKSET_NAME\" NOT LIKE $q AND ";
        } else {
            $qstring1 .=
                " PCMS_WORKSET_INFO.\"WORKSET_NAME\" = $q AND ";
        }
    }

    if ($query_date_min) {
        my $t =  time2pvcsFormat($query_date_min);
        $qstring1 .= "PCMS_ITEM_DATA.\"CREATE_DATE\" >= '$t' AND ";
    }
    if ($query_date_max) {
        my $t =  time2pvcsFormat($query_date_max);
        $qstring1 .= "PCMS_ITEM_DATA.\"CREATE_DATE\" <= '$t' AND ";
    }

    if (0 < @query_dirs) {
        my @list;
        foreach my $i (@query_dirs) {
            my $l = "PCMS_ITEM_DATA.\"LIB_FILENAME\" LIKE " . SqlQuote("$i%");
            push(@list, $l);
        }
        $qstring1 .= " (" . join(" or ", @list) . ") AND ";
    }

    if (defined $query_file && $query_file ne '') {
        my $q = SqlQuote($query_file);
        $query_filetype ||= "exact";
        if ($query_filetype eq 'regexp') {
            $qstring1 .= "PCMS_ITEM_DATA.\"LIB_FILENAME\" LIKE $q AND ";
		} elsif ($query_filetype eq 'notregexp') {
            $qstring1 .= "PCMS_ITEM_DATA.\"LIB_FILENAME\" NOT LIKE $q AND ";
        } else {
            $qstring1 .= "PCMS_ITEM_DATA.\"LIB_FILENAME\" = $q AND ";
        }
    }
     

    if (defined $query_who && $query_who ne '') {
        my $q = SqlQuote($query_who);
        $query_whotype ||= "exact";
        if ($query_whotype eq 'regexp') {
            $qstring1 .= "PCMS_ITEM_DATA.\"ORIGINATOR\" LIKE $q AND ";
        }
        elsif ($query_whotype eq 'notregexp') {
            $qstring1 .= "PCMS_ITEM_DATA.\"ORIGINATOR\" NOT LIKE $q AND ";

        } else {
            $qstring1 .= "PCMS_ITEM_DATA.\"ORIGINATOR\" = $q AND ";
        }
    }

    if (defined($query_logexpr) && $query_logexpr ne '') {
        my $q = SqlQuote($query_logexpr);
        $qstring1 .= " PCMS_CHDOC_DATA.\"TITLE\" = $q AND ";
    }
    
 
    $qstring1 .= "PCMS_ITEM_DATA.\"PRODUCT_ID\" = PCMS_WORKSET_INFO.\"PRODUCT_ID\" AND ";

    $qstring1 .= "PCMS_CHDOC_RELATED_ITEMS.\"TO_ITEM_UID\" = PCMS_ITEM_DATA.\"ITEM_UID\" AND ";

    $qstring1 .= "PCMS_CHDOC_RELATED_ITEMS.\"FROM_CH_UID\" = PCMS_CHDOC_DATA.\"CH_UID\" ";

    return $qstring1;
}


# a drop in replacement for the bonsai 
# function sub query_checkins found in cvsquery.pl

sub query_checkins {

# the function takes these global variables as arguments:

# $::query_module
# $::query_branch
# $::query_branchtype
# $::query_who
# $::query_whotype

# $::query_date_max
# $::query_date_min
# $::query_logexpr

# $::query_file
# $::query_filetype
# @::query_dirs
# $::query_module
# $::query_branch
# $::query_branchtype
# $::query_who
# $::query_whotype

# $::query_date_max
# $::query_date_min
# $::query_logexpr

# $::query_file
# $::query_filetype
# @::query_dirs
    
#    print "query_module: $::query_module, query_branch: $::query_branch<br>\n";

    $where_stmt = create_where_stmt (
                                     $::query_module,
                                     $::query_branch,
                                     $::query_branch_head,
                                     $::query_branchtype,
                                     $::query_who,
                                     $::query_whotype,
                                     
                                     $::query_date_max,
                                     $::query_date_min,
                                     $::query_logexpr,
                                     
                                     $::query_file,
                                     $::query_filetype,
                                     @::query_dirs,
                                     );
    
#   print"$where_stmt<br>\n";

    my $sqltable = <<"EOSQL";

  SELECT 
      PCMS_ITEM_DATA."PRODUCT_ID", 
      PCMS_ITEM_DATA."CREATE_DATE", 
      PCMS_ITEM_DATA."ORIGINATOR", 
      PCMS_ITEM_DATA."LIB_FILENAME", 
      PCMS_ITEM_DATA."REVISION",  
      PCMS_ITEM_DATA."STATUS",
      PCMS_WORKSET_INFO."WORKSET_NAME",
      PCMS_CHDOC_DATA."CH_DOC_ID", 
      PCMS_CHDOC_DATA."TITLE" 
  FROM 
      "PCMS"."PCMS_ITEM_DATA" PCMS_ITEM_DATA, 
      "PCMS"."PCMS_WORKSET_INFO",
      "PCMS"."PCMS_CHDOC_DATA" PCMS_CHDOC_DATA,
      "PCMS"."PCMS_CHDOC_RELATED_ITEMS" PCMS_CHDOC_RELATED_ITEMS
  WHERE 
      $where_stmt
  ORDER BY 
      PCMS_CHDOC_DATA."CH_DOC_ID" DESC;

  exit;
 
EOSQL
    ;
  
    $sqlout = oraselect($sqltable);

#    print $sqltable. "<br><br>\n\n";
#    print $sqlout. "<br><br>\n\n";

    my @out;

    # we get back these values from PVCS.

    my (
        $product_id,
        $create_date,
        $originator,
        $lib_filename,
        $revision,
        $status,
        $workset_name,
        $ch_doc_id,
        $title,
        );

    # Fudge some bonsai arguments which we do not have data for or do
    # not care about.

    my $rectype = 'M';
    my $lines_added = 0;
    my $lines_removed = 0;
    my $dir = '<ignored>';
    my $sticky = '<ignored>';
    my $repository = '<ignored>';
    
    foreach $line (@{ $sqlout }) {

        chomp $line;
        ($line) || next;

        (
	 $product_id,
	 $create_date,
	 $originator,
	 $lib_filename,
	 $revision,
	 $status,
         $workset_name,
	 $ch_doc_id,
	 $title,
	 ) = split (/$sep/ , $line);
        
        $product_id = pvcs_trim_whitespace($product_id);
        $lib_filename = pvcs_trim_whitespace($lib_filename);
        $originator = pvcs_trim_whitespace($originator);
        $create_date  = pvcs_trim_whitespace($create_date);
        $revision = pvcs_trim_whitespace($revision);
        $status = pvcs_trim_whitespace($status);
        $workset_name = pvcs_trim_whitespace($workset_name);
        $ch_doc_id = pvcs_trim_whitespace($ch_doc_id);
        $status = pvcs_trim_whitespace($status);
        
        my $time = pvcs_date_str2time($create_date);

        @row = (
                $rectype, $time, $originator, $repository,
                $dir, $lib_filename, 
                $revision, $sticky, $workset_name,
                $lines_added, $lines_removed, "$ch_doc_id: $title", 
                );

        # This the format that Bonsai expects to get back is a LoL 
        # a list (rows from the database) of 
        # lists (columns for each line, these columns must be in the 
        #        Bonsai expected order).

        push @out, [ @row ];
    }

    my $result = \@out;

    return $result;
}

sub load_product_id_into_modules {

    $module_cache_file = './data/modules';

    # Tell Bonsai what the possible Product_ID's are so that it can
    # create a pick list at the top of the SQL query page.  We cache
    # this information and only perform the SQL call once a day.

    my $sqltable = <<"EOSQL";

  SELECT DISTINCT
      PCMS_WORKSET_INFO."PRODUCT_ID" 
  FROM 
      "PCMS"."PCMS_WORKSET_INFO" PCMS_WORKSET_INFO
  ORDER BY
      PCMS_WORKSET_INFO."PRODUCT_ID";

    exit;

EOSQL
    ;

    if (-M $module_cache_file < 1) {

        require($module_cache_file) ||
            die("Could not eval filename: $module_cache_file: $!\n");

        return ;
    }

    $sqlout = oraselect($sqltable);

    foreach $line (@{ $sqlout }) {

        chomp $line;
        ($line) || next;
        
        my $product_id = pvcs_trim_whitespace($line);
        
        # Bonsai expects this information in a global variable.
        $::modules->{$product_id} = 1;
    }

    my (@out) = ( 
                Data::Dumper->Dump([$::modules], ["\$::modules"],).
                  "1;\n"
                  );
    
    open(NEW, "> $module_cache_file") ||
        die "Couldn't create `$module_cache_file': $!";
    print NEW "@out\n";
    close(NEW);

    return ;
}

1;
