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
require 'utils.pl';
use Date::Parse;

loadConfigData();
$CVS_ROOT = $form{"cvsroot"};

require 'timelocal.pl';
require 'cvsquery.pl';

$| = 1;

$sm_font_tag = "<font face='Arial,Helvetica' size=-2>";

my $generateBackoutCVSCommands = 0;
if (defined $form{'generateBackoutCVSCommands'}) {
    $generateBackoutCVSCommands = 1;
}

if (!$generateBackoutCVSCommands) {
    print "Content-type: text/html

";

    $script_str='';
    &setup_script;

    print "$script_str";
}

#print "<pre>";


$CVS_REPOS_SUFIX = $CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;

$CHECKIN_DATA_FILE = "data/checkinlog${CVS_REPOS_SUFIX}";
$CHECKIN_INDEX_FILE = "data/index${CVS_REPOS_SUFIX}";


$SORT_HEAD="bgcolor=\"#DDDDDD\"";

#
# Log the query
open( LOG, ">>data/querylog.txt");
$t = time;
print LOG "$ENV{'REMOTE_ADDR'} $t $ENV{'QUERY_STRING'}\n";
close(LOG);

#
# build a module map
#
$query_module = $form{'module'};

#
# allow ?file=/a/b/c/foo.c to be synonymous with ?dir=/a/b/c&file=foo.c
#
if ( $form{'dir'} eq '' ) {
    if ($form{'file'} =~ m@(.*?/)([^/]*)$@) {
        $form{'dir'} = $1;
        $form{'file'} = $2;
    }
}

#
# build a directory map
#
@query_dirs = split(/[;, \t]+/, $form{'dir'});

$query_file = $form{'file'};
$query_filetype = $form{'filetype'};
$query_logexpr = $form{'logexpr'};

#
# date
#
$query_date_type = $form{'date'};
if( $query_date_type eq 'hours' ){
    $query_date_min = time - $form{'hours'}*60*60;
} 
elsif( $query_date_type eq 'day' ){
    $query_date_min = time - 24*60*60;
}
elsif( $query_date_type eq 'week' ){
    $query_date_min = time - 7*24*60*60;
}
elsif( $query_date_type eq 'month' ){
    $query_date_min = time - 30*24*60*60;
}
elsif( $query_date_type eq 'all' ){
    $query_date_min = 0;
}
elsif( $query_date_type eq 'explicit' ){
    if ($form{'mindate'} ne "") {
        $query_date_min = parse_date($form{'mindate'});
    }

    if ($form{'maxdate'} ne "") {
        $query_date_max = parse_date($form{'maxdate'});
    }
}
else {
    $query_date_min = time-60*60*2;
}

#
# who
#
$query_who = $form{'who'};
$query_whotype = $form{'whotype'};


$show_raw = $form{'raw'} ne '';

#
# branch
#
$query_branch = $form{'branch'};
if (!defined $query_branch) {
    $query_branch = 'HEAD';
}
$query_branchtype = $form{'branchtype'};


#
# tags
#
$query_begin_tag = $form{'begin_tag'};
$query_end_tag = $form{'end_tag'};


#
# Get the query in english and print it.
#
$t = $e = &query_to_english;
$t =~ s/<[^>]*>//g;

$query_debug = $form{'debug'};

$result= &query_checkins( $mod_map );

for $i (@{$result}) {
    $w{"$i->[$CI_WHO]\@netscape.com"} = 1;
}

@p = sort keys %w;
$pCount = @p;
$s = join(",%20", @p);

$e =~ s/Checkins in/In/;

my $menu = "
<p align=center>$e
<p align=left>
<a href=cvsqueryform.cgi?$ENV{QUERY_STRING}>Modify Query</a>
<br><a href=mailto:$s>Mail everyone on this page</a>
<NOBR>($pCount people)</NOBR>
<br><a href=cvsquery.cgi?$ENV{QUERY_STRING}&generateBackoutCVSCommands=1>I want to back out these changes</a>
";

if (defined $form{'generateBackoutCVSCommands'}) {
    print "Content-type: text/plain

# This page can be saved as a shell script and executed.  It should be 
# run at the top of your CVS work area.  It will update your workarea to 
# backout the changes selected by your query.

";
    foreach my $ci (@{$result}) {
        if ($ci->[$CI_REV] eq "") {
            print "echo 'Changes made to $ci->[$CI_DIR]/$ci->[$CI_FILE] need to be backed out by hand'\n";
            next;
        }
        my $prev_revision = PrevRev($ci->[$CI_REV]);
        print "cvs update -j$ci->[$CI_REV] -j$prev_revision $ci->[$CI_DIR]/$ci->[$CI_FILE]\n";
    }
    exit;
}
        

EmitHtmlTitleAndHeader($t, "CVS Checkins", "$menu");

#
# Test code to print the results
#

$|=1;

if( !$show_raw ) {

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

    &print_result($result);
}
else {
    print "<pre>";
    for $ci (@$result) {
        $ci->[$CI_LOG] = '';
        $s = join("|",@$ci);
        print "$s\n";
    }
}


#
# code to debug the modules_map
#
#print "<PRE>\n";
#while( ($k,$v) = each(%{$mod_map})) {
#    print "$k=$v\n";
#}

#
#
#
sub print_tcl {
    local($result) = @_;
    local($t, $count,$first,$i, $k, $files);

    $t = time;
    print TCLOUT "set treeopen 0\n" .
                 "set lastgoodtimestamp $t\n" .
                 "set closetimestamp $t\n";
    $count = 0;
    $first = 0;
    $i = 1;


    $max = @{$result}+1;

    while( $i < $max ){
        $c1 = $result->[$first];
        $c2 = $result->[$i];
        if( $i == $max-1
                || $c1->[$CI_DATE] != $c2->[$CI_DATE]
                || $c1->[$CI_DIR] ne $c2->[$CI_DIR]
                || $c1->[$CI_WHO] ne $c2->[$CI_WHO]
        ) {
            $files = '{';
            $fu = '{';
            $k = $first;
            while( $k < $i ){
                $files .= $result->[$k][$CI_FILE] . " ";
                $fu .= &make_fullinfo( $result->[$k] );
                $k++;
            }
            $files .= '}';
            $fu .= '}';
            print TCLOUT "set ci-${count}(date) $c1->[$CI_DATE]\n" .
                      "set ci-${count}(dir) $c1->[$CI_DIR]\n" .
                      "set ci-${count}(person) $c1->[$CI_WHO]\n" .
                      "set ci-${count}(files) $files\n" .
                      "set ci-${count}(fullinfo) $fu\n" .
                      "set ci-${count}(log) \{$c1->[$CI_LOG]\}\n" .
                      "set ci-${count}(treeopen) 1\n";
            $count++;
            $first = $i;
        }
        $i++;
    }
}

sub make_fullinfo{
    local( $ci ) = @_;
    local( $s );

    $a = &tcl_value( $ci->[$CI_FILE] );
    $b = &tcl_value( $ci->[$CI_REV] );
    $c = &tcl_value( $ci->[$CI_LINES_ADDED] );
    $d = &tcl_value( $ci->[$CI_LINES_REMOVED] );
    $e = &tcl_value( $ci->[$CI_STICKY] );

    return "{$a $b $c $d $e}";
}

sub tcl_value {
    local( $a ) = @_;

    if( $a eq '' ){
        return '{}';
    }
    else {
        return $a;
    }
}

#
#
#
sub print_result {
    local($result) = @_;
    local($ci,$i,$k,$j,$max, $l, $span);

    &print_head;

    $i = 20;
    $k = 0;
    $max = @{$result};

    while( $k < $max ){
        $ci = $result->[$k];
        $span = 1;
        if( ($l = $ci->[$CI_LOG]) ne '' ){
            # 
            # Calculate the number of consequitive logs that are 
            #  the same and nuke them
            #
            $j = $k+1;
            while( $j < $max && $result->[$j]->[$CI_LOG] eq $l ){
                $result->[$j]->[$CI_LOG] = '';
                $j++;
            }

            # 
            # Make sure we don't break over a description block
            #
            $span = $j-$k;
            if( $span-1 > $i ){
                $i = $j-$k;
            }
        }

        &print_ci( $ci, $span );
        

        if( $i <= 0 ){
            $i = 20;
            print "</TABLE><TABLE border cellspacing=2>\n";
        }
        else {
            $i--;
        }
        $k++;
    }

    &print_foot;
}

sub print_ci {
    local($ci, $span) = @_;
    local($sec,$minute,$hour,$mday,$mon,$year,$t);
    local($log);
    
    ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $ci->[$CI_DATE] );
    $t = sprintf("%02d/%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$year,$hour,$minute);

    $log = &html_log($ci->[$CI_LOG]);
    $rev = $ci->[$CI_REV];

    print "<tr>\n";

    print "<TD>${sm_font_tag}$t</font>";
    print "<TD><a href='../registry/who.cgi?email=$ci->[$CI_WHO]' "
          . "onClick=\"return js_who_menu('$ci->[$CI_WHO]','',event);\" >"
          . "$ci->[$CI_WHO]</a>\n";
    print "<TD><a href='cvsview2.cgi?subdir=$ci->[$CI_DIR]&files=$ci->[$CI_FILE]\&command=DIRECTORY&branch=$query_branch&root=$CVS_ROOT'\n"
          . " onclick=\"return js_file_menu('$CVS_ROOT', '$ci->[$CI_DIR]','$ci->[$CI_FILE]','$ci->[$CI_REV]','$query_branch',event)\">\n";
#     if( (length $ci->[$CI_FILE]) + (length $ci->[$CI_DIR])  > 30 ){
#         $d = $ci->[$CI_DIR];
#         if( (length $ci->[$CI_DIR]) > 30 ){
#             $d =~ s/([^\n]*\/)(classes\/)/$1classes\/<br>&nbsp;&nbsp/;
#                               # Insert a <BR> before any directory named
#                               # 'classes.'
#         }
#         print "   $d/<br>&nbsp;&nbsp;$ci->[$CI_FILE]<a>\n";
#     }
#     else{
#         print "   $ci->[$CI_DIR]/$ci->[$CI_FILE]<a>\n";
#     }
    $d = "$ci->[$CI_DIR]/$ci->[$CI_FILE]";
    if( $query_module eq 'allrepositories' ){ $d = "$ci->[$CI_REPOSITORY]/$d"; }
    $d =~ s:/:/ :g;             # Insert a whitespace after any slash, so that
                                # we'll break long names at a reasonable place.
    print "$d\n";

    if( $rev ne '' ){
        $prevrev = &PrevRev( $rev );
        print "<TD>${sm_font_tag}<a href='cvsview2.cgi?diff_mode=".
              "context\&whitespace_mode=show\&subdir=".
              $ci->[$CI_DIR] . "\&command=DIFF_FRAMESET\&file=" .
              $ci->[$CI_FILE] . "\&rev1=$prevrev&rev2=$rev&root=$CVS_ROOT'>$rev</a></font>\n";
    }
    else {
        print "<TD>\&nbsp;\n";
    }

    if( !$query_branch_head ){    
        print "<TD><TT><FONT SIZE=-1>$ci->[$CI_BRANCH]&nbsp</FONT></TT>\n";
    }
    print "<TD>${sm_font_tag}$ci->[$CI_LINES_ADDED]/$ci->[$CI_LINES_REMOVED]</font>&nbsp\n";
    if( $log ne '' ){
        eval ('$log =~ s@\d{4,6}@' . $BUGSYSTEMEXPR . '@g;');
        $log =~ s/([ #\t])([0-9][0-9][0-9][0-9][0-9])([^0-9])/$1<a href='http:\/\/scopus.mcom.com\/bugsplat\/show_bug.cgi?id=$2'>$2<\/a>$3/g;
                    # Makes numbers into links to bugsplat.

        $log =~ s/\n/<BR>/g;
                    # Makes newlines into <BR>'s
                    
        if( $span > 1 ){
            print "<TD VALIGN=TOP ROWSPAN=$span>$log\n";
        }
        else {
            print "<TD VALIGN=TOP>$log\n";
        }
    }
    print "</tr>\n";
}

sub print_head {

if ($versioninfo ne "") {
    print "<FORM action='multidiff.cgi' method=post>";
    print "<INPUT TYPE='HIDDEN' name='allchanges' value = '$versioninfo'>";
    print "<INPUT TYPE='HIDDEN' name='cvsroot' value = '$CVS_ROOT'>";
    print "<INPUT TYPE=SUBMIT VALUE='Show me ALL the Diffs'>";
    print "</FORM>";
    print "<tt>(+$lines_added/$lines_removed)</tt> Lines changed.";
}

$anchor = $ENV{QUERY_STRING};
$anchor =~ s/\&sortby\=[A-Za-z\ \+]*//g;
$anchor = "<a href=cvsquery.cgi?$anchor";

print "    
<TABLE border cellspacing=2>
<b><TR ALIGN=LEFT>
<TH width=2% $head_date>$anchor>When</a>
<TH width=2% $head_who>${anchor}&sortby=Who>Who</a>
<TH width=45% $head_file>${anchor}&sortby=File>File</a>
<TH width=2%>Rev
";

$descwidth = 47;
if( !$query_branch_head ){    
    print "<TH width=2%>Branch\n";
    $descwidth -= 2;
}

print "    
<TH width=2% $head_delta>${anchor}&sortby=Change+Size>+/-</a>
<TH WIDTH=$descwidth%>Description
</TR></b>
";
}


sub print_foot {
    print "</TABLE>";
    print "<br><br>";
}

sub html_log {
    local( $log ) = @_;
    $log =~ s/&/&amp;/g;
    $log =~ s/</&lt;/g;
    return $log;
}

sub PrevRev {
    local( $rev ) = @_;
    local( $i, $j, $ret, @r );

    @r = split( /\./, $rev );

    $i = @r-1;
    
    $r[$i]--;
    if( $r[$i] == 0 ){
        $i -= 2;
    }

    $j = 0;
    while( $j < $i ){
        $ret .= "$r[$j]\.";
        $j++
    }
    $ret .= $r[$i];
}


sub parse_date {
    my($d) = @_;

    my($result) = str2time($d);
    if (defined $result) {
        return $result;
    } elsif ($d  > 7000000) {
        return $d;
    }
    return 0;
}



sub setup_script {

$script_str =<<'ENDJS';
<script>
var event = 0;	// Nav3.0 compatibility

function js_who_menu(n,extra,d) {
    if( parseInt(navigator.appVersion) < 4 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="../registry/who.cgi?email="+n+extra;

    if(d.target.y > window.innerHeight + window.pageYOffset - l.clip.height) { 
         l.top = (window.innerHeight + window.pageYOffset - l.clip.height);
    } else {
         l.top = d.target.y - 6;
    }
    l.left = d.target.x - 6;

    l.visibility="show";
    return false;
}

function js_file_menu(repos,dir,file,rev,branch,d) {
    var fileName="";
    if( parseInt(navigator.appVersion) < 4 ){
        return true;
    }
    for (var i=0;i<d.target.text.length;i++)
    {
        if (d.target.text.charAt(i)!=" ") {
            fileName+=d.target.text.charAt(i);
        }
    }
    l = document.layers['popup'];
    l.src="../registry/file.cgi?cvsroot="+repos+"&file="+file+"&dir="+dir+"&rev="+rev+"&branch="+branch+"&linked_text="+fileName;

    l.top = d.target.y - 6;
    l.left = d.target.x - 6;
    if( l.left + l.clipWidth > window.width ){
        l.left = window.width - l.clipWidth;
    }
    l.visibility="show";
    return false;
}


</script>

<layer name="popup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

ENDJS

}

#
# Actually do the query
#
sub query_to_english {
    my $english = 'Checkins ';

    if( $query_module eq 'allrepositories' ){
        $english .= "to <i>All Repositories</i> ";
    }
    elsif( $query_module ne 'all' && @query_dirs == 0 ){
        $english .= "to module <i>$query_module</i> ";
    }
    elsif( $form{dir} ne "" ) {
        my $word = "directory";
        if (@query_dirs > 1) {
            $word = "directories";
        }
        $english .= "to $word <i>$form{dir}</i> ";
    }

    if ($query_file ne "") {
        if ($english ne 'Checkins ') {
            $english .= "and ";
        }
        $english .= "to file $query_file ";
    }

    if( ! ($query_branch =~ /^[ ]*HEAD[ ]*$/i) ){
        if($query_branch eq '' ){
            $english .= "on all branches ";
        }
        else {
            $english .= "on branch <i>$query_branch</i> ";
        }
    }

    if( $query_who ne '' ){
        $english .= "by $query_who ";
    }

    $query_date_type = $form{'date'};
    if( $query_date_type eq 'hours' ){
        $english .="in the last $form{hours} hours";
    } 
    elsif( $query_date_type eq 'day' ){
        $english .="in the last day";
    }
    elsif( $query_date_type eq 'week' ){
        $english .="in the last week";
    }
    elsif( $query_date_type eq 'month' ){
        $english .="in the last month";
    }
    elsif( $query_date_type eq 'all' ){
        $english .="since the beginning of time";
    }
    elsif( $query_date_type eq 'explicit' ){
        if ( $form{mindate} ne "" && $form{maxdate} ne "" ) {
            $w1 = "between";
            $w2 = "and" ;
        }
        else {
            $w1 = "since";
            $w2 = "before";
        }

        if( $form{'mindate'} ne "" ){
            $dd = &parse_date($form{'mindate'});
            ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            $t = sprintf("%02d/%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$year,$hour,$minute);
            $english .= "$w1 <i>$t</i> ";
        }

        if( $form{'maxdate'} ne "" ){
            $dd = &parse_date($form{'maxdate'});
            ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            $t = sprintf("%02d/%02d/%02d&nbsp;%02d:%02d",$mon+1,$mday,$year,$hour,$minute);
            $english .= "$w2 <i>$t</i> ";
        }
    }
    return $english . ":";
}
