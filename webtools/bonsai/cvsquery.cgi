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

require 'CGI.pl';
require 'cvsquery.pl';

$CVS_ROOT = $::FORM{'cvsroot'};
$CVS_ROOT = pickDefaultRepository() unless $CVS_ROOT;
$::TreeID = $::FORM{'module'} 
     if (!exists($::FORM{'treeid'}) && 
         exists($::FORM{'module'}) &&
         exists($::TreeInfo{$::FORM{'module'}}{'repository'}));
$::TreeID = 'default'
     if (!exists($::TreeInfo{$::TreeID}{'repository'}) ||
         exists($::TreeInfo{$::TreeID}{'nobonsai'}));

LoadTreeConfig();

my $userdomain = Param('userdomain');
$| = 1;

$sm_font_tag = "<font face='Arial,Helvetica' size=-2>";

my $generateBackoutCVSCommands = 0;
if (defined $::FORM{'generateBackoutCVSCommands'}) {
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
Log("Query [$ENV{'REMOTE_ADDR'}]: $ENV{'QUERY_STRING'}");

#
# build a module map
#
$query_module = $::FORM{'module'};

#
# allow ?file=/a/b/c/foo.c to be synonymous with ?dir=/a/b/c&file=foo.c
#
if ( $::FORM{'dir'} eq '' ) {
    $::FORM{'file'} = Fix_BonsaiLink($::FORM{'file'});
    if ($::FORM{'file'} =~ m@(.*?/)([^/]*)$@) {
        $::FORM{'dir'} = $1;
        $::FORM{'file'} = $2;
    }
}

#
# build a directory map
#
@query_dirs = split(/[;, \t]+/, $::FORM{'dir'});

$query_file = $::FORM{'file'};
$query_filetype = $::FORM{'filetype'};
$query_logexpr = $::FORM{'logexpr'};

#
# date
#
$query_date_type = $::FORM{'date'};
if( $query_date_type eq 'hours' ){
    $query_date_min = time - $::FORM{'hours'}*60*60;
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
    if ($::FORM{'mindate'} ne "") {
        $query_date_min = parse_date($::FORM{'mindate'});
    }

    if ($::FORM{'maxdate'} ne "") {
        $query_date_max = parse_date($::FORM{'maxdate'});
    }
}
else {
    $query_date_min = time-60*60*2;
}

#
# who
#
$query_who = $::FORM{'who'};
$query_whotype = $::FORM{'whotype'};


$show_raw = 0;
$show_raw = $::FORM{'raw'} ne ''
     if $::FORM{'raw'};

#
# branch
#
$query_branch = $::FORM{'branch'};
if (!defined $query_branch) {
    $query_branch = 'HEAD';
}
$query_branchtype = $::FORM{'branchtype'};


#
# tags
#
$query_begin_tag = $::FORM{'begin_tag'};
$query_end_tag = $::FORM{'end_tag'};


#
# Get the query in english and print it.
#
$t = $e = &query_to_english;
$t =~ s/<[^>]*>//g;

$query_debug = $::FORM{'debug'};

my %mod_map = ();
$result= &query_checkins( %mod_map );

for $i (@{$result}) {
    $w{"$i->[$CI_WHO]\@$userdomain"} = 1;
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

if (defined $::FORM{'generateBackoutCVSCommands'}) {
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


PutsHeader($t, "CVS Checkins", "$menu");

#
# Test code to print the results
#

$|=1;

$head_who = '';
$head_file = '';
$head_directory = '';
$head_delta = '';
$head_date = '';

if( !$show_raw ) {

    if( $::FORM{"sortby"} eq "Who" ){
        $result = [sort {
                   $a->[$CI_WHO] cmp $b->[$CI_WHO]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
            } @{$result}] ;
        $head_who = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "File" ){
        $result = [sort {
                   $a->[$CI_FILE] cmp $b->[$CI_FILE]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
                || $a->[$CI_DIRECTORY] cmp $b->[$CI_DIRECTORY]
            } @{$result}] ;
        $head_file = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "Directory" ){
        $result = [sort {
                   $a->[$CI_DIRECTORY] cmp $b->[$CI_DIRECTORY]
                || $a->[$CI_FILE] cmp $b->[$CI_FILE]
                || $b->[$CI_DATE] <=> $a->[$CI_DATE]
            } @{$result}] ;
        $head_directory = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "Change Size" ){
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
    $t = sprintf("%02d/%02d/%04d&nbsp;%02d:%02d",$mon+1,$mday,$year+1900,$hour,$minute);

    $log = &html_log($ci->[$CI_LOG]);
    $rev = $ci->[$CI_REV];

    print "<tr>\n";
    print "<TD width=2%>${sm_font_tag}$t</font>";
    print "<TD width=2%><a href='../registry/who.cgi?email=$ci->[$CI_WHO]' "
          . "onClick=\"return js_who_menu('$ci->[$CI_WHO]','',event);\" >"
          . "$ci->[$CI_WHO]</a>\n";
    print "<TD width=45%><a href='cvsview2.cgi?subdir=$ci->[$CI_DIR]&files=$ci->[$CI_FILE]\&command=DIRECTORY&branch=$query_branch&root=$CVS_ROOT'\n"
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
        print "<TD width=2%>${sm_font_tag}<a href='cvsview2.cgi?diff_mode=".
              "context\&whitespace_mode=show\&subdir=".
              $ci->[$CI_DIR] . "\&command=DIFF_FRAMESET\&file=" .
              $ci->[$CI_FILE] . "\&rev1=$prevrev&rev2=$rev&root=$CVS_ROOT'>$rev</a></font>\n";
    }
    else {
        print "<TD width=2%>\&nbsp;\n";
    }

    if( !$query_branch_head ){
        print "<TD width=2%><TT><FONT SIZE=-1>$ci->[$CI_BRANCH]&nbsp</FONT></TT>\n";
    }

    print "<TD width=2%>${sm_font_tag}$ci->[$CI_LINES_ADDED]/$ci->[$CI_LINES_REMOVED]</font>&nbsp\n";
    if( $log ne '' ){
        $log = MarkUpText($log);
                    # Makes numbers into links to bugsplat.

        $log =~ s/\n/<BR>/g;
                    # Makes newlines into <BR>'s

        if( $span > 1 ){
            print "<TD WIDTH=$descwidth% VALIGN=TOP ROWSPAN=$span>$log\n";
        }
        else {
            print "<TD WIDTH=$descwidth% VALIGN=TOP>$log\n";
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

print "<TABLE border cellspacing=2>\n";
print "<b><TR ALIGN=LEFT>\n";
print "<TH width=2% $head_date>$anchor>When</a>\n";
print "<TH width=2% $head_who>${anchor}&sortby=Who>Who</a>\n";
print "<TH width=45% $head_file>${anchor}&sortby=File>File</a>\n";
print "<TH width=2%>Rev\n";

$descwidth = 47;
if( !$query_branch_head ){
    print "<TH width=2%>Branch\n";
    $descwidth -= 2;
}

print "<TH width=2% $head_delta>${anchor}&sortby=Change+Size>+/-</a>\n";
print "<TH WIDTH=$descwidth%>Description\n";
print "</TR></b>\n";
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
    elsif( $::FORM{dir} ne "" ) {
        my $word = "directory";
        if (@query_dirs > 1) {
            $word = "directories";
        }
        $english .= "to $word <i>$::FORM{dir}</i> ";
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

    $query_date_type = $::FORM{'date'};
    if( $query_date_type eq 'hours' ){
        $english .="in the last $::FORM{hours} hours";
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
        if ( $::FORM{mindate} ne "" && $::FORM{maxdate} ne "" ) {
            $w1 = "between";
            $w2 = "and" ;
        }
        else {
            $w1 = "since";
            $w2 = "before";
        }

        if( $::FORM{'mindate'} ne "" ){
            $dd = &parse_date($::FORM{'mindate'});
            ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            $t = sprintf("%02d/%02d/%04d&nbsp;%02d:%02d",$mon+1,$mday,$year+1900,$hour,$minute);
            $english .= "$w1 <i>$t</i> ";
        }

        if( $::FORM{'maxdate'} ne "" ){
            $dd = &parse_date($::FORM{'maxdate'});
            ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            $t = sprintf("%02d/%02d/%04d&nbsp;%02d:%02d",$mon+1,$mday,$year+1900,$hour,$minute);
            $english .= "$w2 <i>$t</i> ";
        }
    }
    return $english . ":";
}

PutsTrailer();
