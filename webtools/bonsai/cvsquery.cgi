#!/usr/bin/perl -w
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
# The Original Code is the Bonsai CVS tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::CI_BRANCH;
    $zz = $::CI_REPOSITORY;
    $zz = $::lines_added;
    $zz = $::lines_removed;
    $zz = $::query_begin_tag;
    $zz = $::query_branchtype;
    $zz = $::query_dirtype;
    $zz = $::query_date_max;
    $zz = $::query_debug;
    $zz = $::query_end_tag;
    $zz = $::query_filetype;
    $zz = $::query_logexpr;
    $zz = $::query_whotype;
    $zz = $::script_type;
}



require 'CGI.pl';

$::query_debug = (defined($::FORM{'debug'}) ? 1 : 0);
$::query_module = &SanitizeModule($::FORM{'module'});

$::CVS_ROOT = $::FORM{'cvsroot'};
$::CVS_ROOT = &pickDefaultRepository() unless $::CVS_ROOT;
&validateRepository($::CVS_ROOT);

$::TreeID = $::query_module
    if (!exists($::FORM{'treeid'}) && 
        defined($::query_module) &&
        exists($::TreeInfo{$::query_module}{'repository'}));
$::TreeID = 'default'
    if (!exists($::TreeInfo{$::TreeID}{'repository'}) ||
    exists($::TreeInfo{$::TreeID}{'nobonsai'}));

&LoadTreeConfig();

require 'cvsquery.pl';

my $userdomain = Param('userdomain');
my $registryurl = Param('registryurl');
$registryurl =~ s@/$@@;
$| = 1;

my $sm_font_tag = "<font face='Arial,Helvetica' size=-2>";

my $generateBackoutCVSCommands = 0;
if (defined $::FORM{'generateBackoutCVSCommands'}) {
    $generateBackoutCVSCommands = 1;
}

my $show_raw = 0;
if ($::FORM{'raw'}) {
    $show_raw = 1;
}

my $show_xml = 0;
my $xml_nofiles = 0;
if ($::FORM{'xml'}) {
    $show_xml = 1;
    if ($::FORM{'xml_nofiles'}) {
        $xml_nofiles = 1;
    }
}


if( !$show_xml && !$generateBackoutCVSCommands ) {
    print "Content-type: text/html\n\n";
    print setup_script();
} elsif ($show_xml) {
    print "Content-type: text/xml\n\n";
} elsif ($generateBackoutCVSCommands) {
    print "Content-type: text/plain\n\n";
}

#print "<pre>";


my $CVS_REPOS_SUFIX = $::CVS_ROOT;
$CVS_REPOS_SUFIX =~ s/\//_/g;

my $CHECKIN_DATA_FILE = "data/checkinlog${CVS_REPOS_SUFIX}";
my $CHECKIN_INDEX_FILE = "data/index${CVS_REPOS_SUFIX}";


my $SORT_HEAD="bgcolor=\"#DDDDDD\"";

#
# Log the query
Log("Query [$ENV{'REMOTE_ADDR'}]: $ENV{'QUERY_STRING'}");

#
# build a module map
#

#
# allow ?file=/a/b/c/foo.c to be synonymous with ?dir=/a/b/c&file=foo.c
#
$::FORM{'file'} = "" unless defined $::FORM{'file'};
unless ($::FORM{'dir'}) {
    $::FORM{'file'} = Fix_BonsaiLink($::FORM{'file'});
    if ($::FORM{'file'} =~ m@(.*?/)([^/]*)$@) {
        $::FORM{'dir'} = $1;
        $::FORM{'file'} = $2;
    } else {
        $::FORM{'dir'} = "";
    }
}

#
# build a directory map
#
@::query_dirs = split(/[;, \t\000]+/, $::FORM{'dir'});

$::query_file = $::FORM{'file'};
$::query_filetype = &ExpectMatchtype($::FORM{'filetype'});
$::query_logexpr = $::FORM{'logexpr'};

#
# date
#
$::query_date_type = $::FORM{'date'} || 'nothing';
my $query_hours;
if( $::query_date_type eq 'hours' ){
    $query_hours = &ExpectDigit('hours', $::FORM{'hours'});
    $::query_date_min = time - $query_hours*60*60;
}
elsif( $::query_date_type eq 'day' ){
    $::query_date_min = time - 24*60*60;
}
elsif( $::query_date_type eq 'week' ){
    $::query_date_min = time - 7*24*60*60;
}
elsif( $::query_date_type eq 'month' ){
    $::query_date_min = time - 30*24*60*60;
}
elsif( $::query_date_type eq 'all' ){
    $::query_date_min = 0;
}
elsif( $::query_date_type eq 'explicit' ){
    if ($::FORM{'mindate'}) {
        $::query_date_min = parse_date($::FORM{'mindate'});
    }

    if ($::FORM{'maxdate'}) {
        $::query_date_max = parse_date($::FORM{'maxdate'});
    }
}
else {
    $::query_date_min = time-60*60*2;
}

#
# who
#
$::query_who = &SanitizeUsernames($::FORM{'who'});
$::query_whotype = &ExpectMatchtype($::FORM{'whotype'});


#
# branch
#
$::query_branch = $::FORM{'branch'};
$::query_branchtype = &ExpectMatchtype($::FORM{'branchtype'});
$::query_branch = 'HEAD' if !defined($::query_branch);
if ($::query_branchtype eq 'notregexp' || $::query_branchtype eq 'regexp') {
    $::query_branch =~ s/\W*([\w-]+).*/$1/;
} else {
    $::query_branch = &SanitizeRevision($::query_branch);
}
$::query_dirtype = $::FORM{'dirtype'};
$::query_dirtype = 'recurse' if !defined ($::query_dirtype);

if ($::query_branch eq 'HEAD' && $::query_branchtype ne 'notregexp') {
    $::query_branch_head = 1 ;
}

#
# tags
#
$::query_begin_tag = &SanitizeRevision($::FORM{'begin_tag'});
$::query_end_tag = &SanitizeRevision($::FORM{'end_tag'});


#
# Get the query in english and print it.
#
my ($t, $e);
$t = $e = &query_to_english;
$t =~ s/<[^>]*>//g;

my %mod_map = ();
my $result= &query_checkins( %mod_map );

my %w;

for my $i (@{$result}) {
	my $aname=$i->[$::CI_WHO];
# the else is for compatibility w/ something that uses the other format
# the regexp is probably not the best, but I think it might work
	if ($aname =~ /%\w*.\w\w+/) {
		my $tmp = join("@",split("%",$aname));
		$w{"$tmp"} = 1;
	}else{
		$w{"$i->[$::CI_WHO]\@$userdomain"} = 1;
	}
}

my @p = sort keys %w;
my $pCount = @p;
my $s = join(",%20", @p);

$e =~ s/Checkins in/In/;

my $menu = "
<p align=center>$e
<p align=left>
<a href=cvsqueryform.cgi?$ENV{QUERY_STRING}>Modify Query</a>
";
if ($pCount) {
    my $persons = "people";
    $persons = "person" if ($pCount == 1);
    $menu .= "
<br><a href=mailto:$s>Mail everyone on this page</a>
<NOBR>($pCount $persons)</NOBR>
<br><a href=cvsquery.cgi?$ENV{QUERY_STRING}&generateBackoutCVSCommands=1>Show commands which could be used to back out these changes</a>
";
}

if (defined $::FORM{'generateBackoutCVSCommands'}) {
    print "Content-type: text/plain

# This page can be saved as a shell script and executed.  It should be
# run at the top of your CVS work area.  It will update your workarea to
# backout the changes selected by your query.

";
    unless ($pCount) {
        print "
#
# No changes occurred during this interval.
# There is nothing to back out.
#

";
        exit;
    }

    foreach my $ci (reverse @{$result}) {
        if ($ci->[$::CI_REV] eq "") {
            print "echo 'Changes made to $ci->[$::CI_DIR]/$ci->[$::CI_FILE] need to be backed out by hand'\n";
            next;
        }
        my $prev_revision = PrevRev($ci->[$::CI_REV]);
        print "cvs update -j$ci->[$::CI_REV] -j$prev_revision $ci->[$::CI_DIR]/$ci->[$::CI_FILE]\n";
    }
    exit;
}

if( !$show_xml ) {
    PutsHeader($t, "CVS Checkins", "$menu");
}

#
# Test code to print the results
#

$|=1;

my $head_who = '';
my $head_file = '';
my $head_directory = '';
my $head_delta = '';
my $head_date = '';

if( !$show_raw && !$show_xml ) {

    $::FORM{"sortby"} ||= "";

    if( $::FORM{"sortby"} eq "Who" ){
        $result = [sort {
                   $a->[$::CI_WHO] cmp $b->[$::CI_WHO]
                || $b->[$::CI_DATE] <=> $a->[$::CI_DATE]
            } @{$result}] ;
        $head_who = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "File" ){
        $result = [sort {
                   $a->[$::CI_FILE] cmp $b->[$::CI_FILE]
                || $b->[$::CI_DATE] <=> $a->[$::CI_DATE]
                || $a->[$::CI_DIRECTORY] cmp $b->[$::CI_DIRECTORY]
            } @{$result}] ;
        $head_file = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "Directory" ){
        $result = [sort {
                   $a->[$::CI_DIRECTORY] cmp $b->[$::CI_DIRECTORY]
                || $a->[$::CI_FILE] cmp $b->[$::CI_FILE]
                || $b->[$::CI_DATE] <=> $a->[$::CI_DATE]
            } @{$result}] ;
        $head_directory = $SORT_HEAD;
    }
    elsif( $::FORM{"sortby"} eq "Change Size" ){
        $result = [sort {

                        ($b->[$::CI_LINES_ADDED]- $b->[$::CI_LINES_REMOVED])
                    <=> ($a->[$::CI_LINES_ADDED]- $a->[$::CI_LINES_REMOVED])
                #|| $b->[$::CI_DATE] <=> $a->[$::CI_DATE]
            } @{$result}] ;
        $head_delta = $SORT_HEAD;
    }
    else{
        $result = [sort {$b->[$::CI_DATE] <=> $a->[$::CI_DATE]} @{$result}] ;
        $head_date = $SORT_HEAD;
    }

    &print_result($result);
}
elsif( $show_xml ) {
    print "<?xml version=\"1.0\"?>\n";
    print "<queryResults>\n";

    my $max = @{$result};
    my $k = 0;

    while ($k < $max) {
        my $ci = $result->[$k];
        my @ci_files = ($ci->[$::CI_DIR] . "/" . $ci->[$::CI_FILE]);
        my @ci_revs = ($ci->[$::CI_REV]);
        my $ci_log = "";
        my $ci_who = $ci->[$::CI_WHO];
        my $ci_date = $ci->[$::CI_DATE];

        # combine adjacent entries with the same log entry
        my $j = $k+1;
        if (($ci_log = $ci->[$::CI_LOG]) ne '') {
            while ($j < $max && $result->[$j]->[$::CI_LOG] eq $ci_log) {
                push @ci_files, $result->[$j]->[$::CI_DIR]."/".$result->[$j]->[$::CI_FILE];
                push @ci_revs, $result->[$j]->[$::CI_REV];
                $j++;
            }
        }

        $ci_who = &html_log($ci_who);
        $ci_log = &html_log($ci_log);

        print "<ci who=\"$ci_who\" date=\"$ci_date\">\n";
        print " <log>$ci_log</log>\n";
        if (!$xml_nofiles) {
            print " <files>\n";
            for (my $f = 0; $f != $#ci_files; $f++) {
                print "  <f rev=\"" . $ci_revs[$f] . "\">" . &html_log($ci_files[$f]) . "</f>\n";
            }
            print " </files>\n";
        }
        print "</ci>\n";

        $k = $j;
    }

    print "</queryResults>\n\n";

    exit;
}
else {
    print "<pre>";
    for my $ci (reverse @$result) {
        $ci->[$::CI_LOG] = '';
        $s = join("|",@$ci);
        print "$s\n";
    }
}


#
#
#
sub print_result {
    my ($result) = @_;
    my ($ci,$i,$k,$j,$max, $l, $span);

    &print_head;

    $i = 20;
    $k = 0;
    $max = @{$result};

    while( $k < $max ){
        $ci = $result->[$k];
        $span = 1;
        if( ($l = $ci->[$::CI_LOG]) ne '' ){
            #
            # Calculate the number of consecutive logs that are
            #  the same and nuke them
            #
            $j = $k+1;
            while( $j < $max && $result->[$j]->[$::CI_LOG] eq $l ){
                $result->[$j]->[$::CI_LOG] = '';
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

my $descwidth;

sub print_ci {
    my ($ci, $span) = @_;

    my ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $ci->[$::CI_DATE] );
    my $t = sprintf("%04d-%02d-%02d&nbsp;%02d:%02d",$year+1900,$mon+1,$mday,$hour,$minute);

    my $log = &html_log($ci->[$::CI_LOG]);
    my $rev = $ci->[$::CI_REV];
    my $url_who = &url_quote($ci->[$::CI_WHO]);
    my $url_dir = &url_quote($ci->[$::CI_DIR]);
    my $url_file = &url_quote($ci->[$::CI_FILE]);

    print "<tr>\n";
    print "<TD width=2%>${sm_font_tag}$t</font>";
    print "<TD width=2%><a href='$registryurl/who.cgi?email=$url_who'"
          . " onClick=\"return js_who_menu('$url_who','',event);\" >"
          . "$ci->[$::CI_WHO]</a>\n";
    print "<TD width=45%><a href='cvsview2.cgi?subdir=$url_dir" .
        "&files=$url_file\&command=DIRECTORY&branch=$::query_branch" .
        "&root=" . $ci->[$::CI_REPOSITORY] . "'\n" .
        " onclick=\"return js_file_menu('$ci->[$::CI_REPOSITORY]', '$url_dir'," .
        "'$url_file','$ci->[$::CI_REV]','$::query_branch',event)\">\n";
#     if( (length $ci->[$::CI_FILE]) + (length $ci->[$::CI_DIR])  > 30 ){
#         $d = $ci->[$::CI_DIR];
#         if( (length $ci->[$::CI_DIR]) > 30 ){
#             $d =~ s/([^\n]*\/)(classes\/)/$1classes\/<br>&nbsp;&nbsp/;
#                               # Insert a <BR> before any directory named
#                               # 'classes.'
#         }
#         print "   $d/<br>&nbsp;&nbsp;$ci->[$::CI_FILE]<a>\n";
#     }
#     else{
#         print "   $ci->[$::CI_DIR]/$ci->[$::CI_FILE]<a>\n";
#     }
    my $d = "$ci->[$::CI_DIR]/$ci->[$::CI_FILE]";
    if (defined $::query_module && $::query_module eq 'allrepositories') {
        $d = "$ci->[$::CI_REPOSITORY]/$d";
    }
    $d =~ s:/:/<wbr>:g;         # Insert a <wbr> tag after each slash, so that
                                # we'll break long paths at a reasonable place.
    print "$d\n";

    if( $rev ne '' ){
        my $prevrev = &PrevRev( $rev );
        print "<TD width=2%>${sm_font_tag}<a href='cvsview2.cgi?diff_mode=".
              "context\&whitespace_mode=show\&subdir=$url_dir" .
              "\&command=DIFF_FRAMESET\&file=$url_file" .
              "\&rev1=$prevrev&rev2=$rev&root=" .  $ci->[$::CI_REPOSITORY] . "'>$rev</a></font>\n";
    }
    else {
        print "<TD width=2%>\&nbsp;\n";
    }

    if( !$::query_branch_head ){
        print "<TD width=2%><TT><FONT SIZE=-1>$ci->[$::CI_BRANCH]&nbsp</FONT></TT>\n";
    }

    print "<TD width=2%>${sm_font_tag}$ci->[$::CI_LINES_ADDED]/$ci->[$::CI_LINES_REMOVED]</font>&nbsp\n";
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

if ($::versioninfo) {
    print "<FORM action='multidiff.cgi' method=post>";
    print "<INPUT TYPE='HIDDEN' name='allchanges' value = '$::versioninfo'>";
    print "<INPUT TYPE='HIDDEN' name='cvsroot' value = '$::CVS_ROOT'>";
    print "<INPUT TYPE=SUBMIT VALUE='Show me ALL the Diffs'>";
    print "</FORM>";
    print "<tt>(+$::lines_added/$::lines_removed)</tt> Lines changed.";
}

my $anchor = $ENV{QUERY_STRING};
$anchor =~ s/\&sortby\=[A-Za-z\ \+]*//g;
$anchor = "<a href=cvsquery.cgi?$anchor";

print "<TABLE border cellspacing=2>\n";
print "<b><TR ALIGN=LEFT>\n";
print "<TH width=2% $head_date>$anchor>When</a>\n";
print "<TH width=2% $head_who>${anchor}&sortby=Who>Who</a>\n";
print "<TH width=45% $head_file>${anchor}&sortby=File>File</a>\n";
print "<TH width=2%>Rev\n";

$descwidth = 47;
if( !$::query_branch_head ){
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
    my ( $log ) = @_;
    $log =~ s/&/&amp;/g;
    $log =~ s/</&lt;/g;
    $log =~ s/>/&gt;/g;
    return $log;
}

sub PrevRev {
    my( $rev ) = @_;
    my( $i, $j, $ret, @r );

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
    return &ExpectDate($d);
}



sub setup_script {

    my $script_str = qq{
<script $::script_type><!--
var event = 0;	// Nav3.0 compatibility

function js_who_menu(n,extra,d) {
    if( parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1 ){
        return true;
    }
    l = document.layers['popup'];
    l.src="$registryurl/who.cgi?email="+n+extra;

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
    if( parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1 ){
        return true;
    }
    for (var i=0;i<d.target.text.length;i++)
    {
        if (d.target.text.charAt(i)!=" ") {
            fileName+=d.target.text.charAt(i);
        }
    }
    l = document.layers['popup'];
    l.src="$registryurl/file.cgi?cvsroot="+repos+"&file="+file+"&dir="+dir+"&rev="+rev+"&branch="+branch+"&linked_text="+fileName;

    l.top = d.target.y - 6;
    l.left = d.target.x - 6;
    if( l.left + l.clipWidth > window.width ){
        l.left = window.width - l.clipWidth;
    }
    l.visibility="show";
    return false;
}


//--></script>

<layer name="popup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

};

    return $script_str;
}

#
# Actually do the query
#
sub query_to_english {
    my $english = 'Checkins ';

    $::query_module = 'all' unless defined $::query_module;
    if( $::query_module eq 'allrepositories' ){
        $english .= "to <i>All Repositories</i> ";
    }
    elsif( $::query_module ne 'all' && @::query_dirs == 0 ){
        $english .= "to module <i>" . html_quote($::query_module) . "</i> ";
    }
    elsif( $::FORM{dir} ne "" ) {
        my $word = "directory";
        if (@::query_dirs > 1) {
            $word = "directories";
        }
        $english .= "to $word <i>" . html_quote($::FORM{dir}) . "</i> ";
    }

    if ($::query_file ne "") {
        if ($english ne 'Checkins ') {
            $english .= "and ";
        }
        if (defined($::query_filetype) && $::query_filetype eq 'regexp') {
            $english .= "to a file like ";
        } elsif (defined($::query_filetype) && $::query_filetype eq 'notregexp') {
            $english .= "to a file not like ";
        } else {
            $english .= "to file ";
        }
        $english .= "<i>" . &html_quote($::query_file) . "</i> ";
    }

    if (!$::query_branch_head) {
        if ($::query_branch eq '') {
            $english .= "on all branches ";
        } else {
            if ($::query_branchtype eq 'notregexp') {
                if ($::query_branch eq 'HEAD') {
                    $english .= "not on ";
                } else {
                    $english .= "not like ";
                }
            } elsif ($::query_branchtype eq 'regexp') {
                $english .= "like ";
            } else {
                $english .= "on ";
            }
            $english .= "branch <i>" . html_quote($::query_branch) . "</i> ";
        }
    }

    if( $::query_who) {
        if (defined($::query_whotype) && $::query_whotype eq 'regexp') {
                $english .= "by someone like ";
        } elsif (defined($::query_whotype) && $::query_whotype eq 'notregexp') {
                $english .= "by someone not like ";
        } else {
                $english .= "by ";
        } 
        $english .= "<i>" . &html_quote($::query_who) . "</i> ";
    }

    $::query_date_type = $::FORM{'date'} || 'nothing';
    if( $::query_date_type eq 'hours' ){
        $english .="in the last $query_hours hours";
    }
    elsif( $::query_date_type eq 'day' ){
        $english .="in the last day";
    }
    elsif( $::query_date_type eq 'week' ){
        $english .="in the last week";
    }
    elsif( $::query_date_type eq 'month' ){
        $english .="in the last month";
    }
    elsif( $::query_date_type eq 'all' ){
        $english .="since the beginning of time";
    }
    elsif( $::query_date_type eq 'explicit' ){
        my ($w1, $w2);
        if ( $::FORM{mindate} && $::FORM{maxdate}) {
            $w1 = "between";
            $w2 = "and" ;
        }
        else {
            $w1 = "since";
            $w2 = "before";
        }

        if( $::FORM{'mindate'}){
            my $dd = &parse_date($::FORM{'mindate'});
            my ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            my $t = sprintf("%04d-%02d-%02d&nbsp;%02d:%02d",$year+1900,$mon+1,$mday,$hour,$minute);
            $english .= "$w1 <i>$t</i> ";
        }

        if( $::FORM{'maxdate'}){
            my $dd = &parse_date($::FORM{'maxdate'});
            my ($sec,$minute,$hour,$mday,$mon,$year) = localtime( $dd );
            my $t = sprintf("%04d-%02d-%02d&nbsp;%02d:%02d",$year+1900,$mon+1,$mday,$hour,$minute);
            $english .= "$w2 <i>$t</i> ";
        }
    }
    return $english . ":";
}

PutsTrailer();
