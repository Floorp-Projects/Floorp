#!/usr/bonsaitools/bin/perl --
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


require 'lloydcgi.pl';
require 'timelocal.pl';
require 'cvsquery.pl';

$| = 1;

print "Content-type: text/html

<HTML>";

$CHECKIN_DATA_FILE = 'data/checkinlog_m_src';
$CHECKIN_INDEX_FILE = 'data/index_m_src';

#
# build a module map
#
$query_module = $form{'module'};

@query_dirs = split(/[;, \t]+/, $form{'dir'});

$query_date_type = $form{'date'};
$query_date_min = time-(24*60*60*15);
$query_who ='' ;
$query_branch = $form{'branch'};


print "<h1>Running Query, this may take a while...</h1>";

$result= &query_checkins( $mod_map );

#
# Test code to print the results
#

    if( 0 ) {
    if( $form{"sortby"} eq "Who" ){
        $result = [sort {
                   $a->[$CI_WHO] cmp $b->[$CI_WHO]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
            } @{$result}] ;
        $head_who = $SORT_HEAD;
    }
    elsif( $form{"sortby"} eq "File" ){
        $result = [sort {
                   $a->[$CI_FILE] cmp $b->[$CI_FILE]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
                || $a->[$CI_DIRECTORY] cmp $b->[$CI_DIRECTORY]
            } @{$result}] ;
        $head_file = $SORT_HEAD;
    }
    elsif( $form{"sortby"} eq "Directory" ){
        $result = [sort {
                   $a->[$CI_DIRECTORY] cmp $b->[$CI_DIRECTORY]
                || $a->[$CI_FILE] cmp $b->[$CI_FILE]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
            } @{$result}] ;
        $head_directory = $SORT_HEAD;
    }
    elsif( $form{"sortby"} eq "Change Size" ){
        $result = [sort {

                        ($b->[$CI_LINES_ADDED]- $b->[$CI_LINES_REMOVED])  
                    <=> ($a->[$CI_LINES_ADDED]- $a->[$CI_LINES_REMOVED])
                #|| $b->[$CI_DATE] <=> $a->[$CI_DATE]
            } @{$result}] ;
        $head_delta = $SORT_HEAD;
    }
    else{
        $result = [sort {$b->[$CI_DATE] <=> $a->[$CI_DATE]} @{$result}] ;
        $head_date = $SORT_HEAD;
    }
    }

print "<pre>";
for $ci (@$result) {
    $ci->[$CI_LOG] = '';
    $s = join("|",@$ci);
    print "$s\n";
}

