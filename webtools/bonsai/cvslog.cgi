#!/usr/bonsaitools/bin/perl -w
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

#  cvslog.cgi -- cvslog with logs as popups and allowing html in comments.
#
#  Created: Steve Lamm <slamm@netscape.com>, 31-Mar-98.
#
#  Arguments (passed via GET or POST):
#    file - path to file name (e.g. ns/cmd/xfe/Makefile)
#    root - cvs root (e.g. /warp/webroot)
#    rev  - revision (default is the latest version)
#    mark - highlight a revision
#    author - filter based on author
#

use diagnostics;
use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::CVS_ROOT;
    $zz = $::head_revision;
    $zz = $::revision_ctime;
    $zz = $::revision_log;
}

require 'CGI.pl';
require 'cvsblame.pl';

# Some Globals
#
$| = 1;

my @src_roots = getRepositoryList();


# Handle the "file" argument
#
my $filename = '';
$filename = $::FORM{'file'} if defined($::FORM{'file'});
if ($filename eq '') 
{
    print "Content-Type:text/html\n\n";
    &print_usage;
    exit;
}
my ($file_head, $file_tail) = $filename =~ m@(.*/)?(.+)@;
my $url_filename = url_quote($filename);
my $url_file_tail = url_quote($file_tail);

# Handle the "rev" argument
#
$::opt_rev = "";
$::opt_rev = sanitize_revision($::FORM{'rev'}) if 
    defined $::FORM{'rev'} && $::FORM{'rev'} !~ m/^(HEAD|MAIN)$/;
my $revstr = '';
$revstr = "&rev=$::opt_rev" unless $::opt_rev eq '';
my $browse_revtag = 'HEAD';
$browse_revtag = $::opt_rev if ($::opt_rev =~ /[A-Za-z]/);
my $revision = '';


# Handle the "root" argument
#
my $root = $::FORM{'root'};
if (defined $root && $root ne '') {
    $root =~ s|/$||;
    validateRepository($root);
    if (-d $root) {
        unshift(@src_roots, $root);
    } else {
        print "Content-Type:text/html\n\n";
        &print_top;
        print "Error:  Root, $root, is not a directory.<BR><BR>\n";
        print "</BODY></HTML>\n";
        &print_bottom;
        exit;
    }
}



# Find the rcs file
#
my $rcs_filename;
foreach (@src_roots) {
    $root = $_;
    $rcs_filename = "$root/$filename,v";
    $rcs_filename = Fix_BonsaiLink($rcs_filename);
    goto found_file if -r $rcs_filename;
    $rcs_filename = "$root/${file_head}Attic/$file_tail,v";
    goto found_file if -r $rcs_filename;
}
# File not found
print "Content-Type:text/html\n\n";
&print_top;
my $escaped_filename = html_quote($filename);
print "Rcs file, $escaped_filename, does not exist.<BR><BR>\n";
print "</BODY></HTML>\n";
&print_bottom;
exit;

found_file:

my $rcs_path;
($rcs_path) = $rcs_filename =~ m@$root/(.*)/.+?,v@;


# Parse the rcs file ($::opt_rev is passed as a global)
#
$revision = &parse_cvs_file($rcs_filename);
my $file_rev = $revision;

my $start_rev;
if ($browse_revtag eq 'HEAD') {
    $start_rev = $::head_revision;  # $::head_revision is a global from cvsblame.pl
} else {
    $start_rev = map_tag_to_revision($browse_revtag);
}
print "Content-Type:text/html\n";
print "Last-Modified: ".time2str("%a, %d %b %Y %T %Z", str2time($::revision_ctime{$start_rev}), "GMT")."\n";
print "Expires: ".time2str("%a, %d %b %Y %T %Z", time+1200, "GMT")."\n";
print "\n";

# Handle the "mark" argument
#
my %mark;
my $mark_arg = '';
$mark_arg = $::FORM{'mark'} if defined($::FORM{'mark'});
foreach my $rev (split(',',$mark_arg)) {
    $mark{$rev} = 1;
}


# Handle the "author" argument
#
my %use_author;
my $author_arg = '';
$author_arg = $::FORM{'author'} if defined($::FORM{'author'});
foreach my $author (split(',',$author_arg)) {
    $use_author{$author} = 1;
}


# Handle the "sort" argument
my $opt_sort = '';
$opt_sort = $::FORM{'sort'} if defined $::FORM{'sort'};


# Start printing out the page
#
&print_top;
print Param('bannerhtml', 1);


# Print link at top for directory browsing
#
print q(
<TABLE BORDER=0 CELLPADDING=5 CELLSPACING=0 WIDTH="100%">
 <TR>
  <TD ALIGN=LEFT VALIGN=CENTER>
   <NOBR><FONT SIZE="+2"><B>
    CVS Log
   </B></FONT></NOBR>
   <BR><B>
);

my $link_path;
my $lxr_path;
foreach my $path (split('/',$rcs_path)) {
    $link_path .= url_encode2($path).'/';
    $lxr_path = Fix_LxrLink($link_path);
    print "<A HREF='$lxr_path'>$path</a>/ ";
}
$lxr_path = Fix_LxrLink("$link_path$file_tail");
print "<A HREF='$lxr_path'>$file_tail</a> ";

my $graph_cell = Param('cvsgraph') ? <<"--endquote--" : "";
       </TR><TR>
        <TD>
         <A HREF="cvsgraph.cgi?file=$url_filename">graph</A>&nbsp;
        </TD><TD NOWRAP>
         View the revision history as a graph
        </TD>
--endquote--

print " (";
print "$browse_revtag:" unless $browse_revtag eq 'HEAD';
print $revision if $revision;
print ")";

print qq(
</B>
  </TD>

  <TD ALIGN=RIGHT VALIGN=TOP WIDTH="1%">
   <TABLE BORDER CELLPADDING=10 CELLSPACING=0>
    <TR>
     <TD NOWRAP BGCOLOR="#FAFAFA">
      <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>
       <TR>
        <TD>
         <A HREF="$lxr_path">lxr</A>
        </TD><TD NOWRAP>
         Browse the source code as hypertext.
        </TD>
       </TR><TR>
        <TD>
         <A HREF="cvsview2.cgi?command=DIRECTORY&subdir=$rcs_path&files=$url_file_tail&branch=$::opt_rev">diff</A>
        </TD><TD NOWRAP>
         Compare any two version.
        </TD>
       </TR><TR>
        <TD>
         <A HREF="cvsblame.cgi?file=$url_filename&rev=$::opt_rev&cvsroot=$root">blame</A>&nbsp;
        </TD><TD NOWRAP>
         Annotate the author of each line.
        </TD>
$graph_cell
       </TR>
      </TABLE>
     </TD>
    </TR>
   </TABLE>
  </TD>
 </TR>
</TABLE>
      );

#&print_useful_links($filename);
 
# Create a table with header links to sort by column.
#
my $table_tag = "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3 WIDTH='100%'>";
my $table_header_tag = "";
if ($opt_sort eq 'author') {
    $table_header_tag .= "<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$url_filename&root=$root&rev=$browse_revtag&sort=revision&author=$author_arg'>Rev</A><TH ALIGN=LEFT>Author<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$url_filename&root=$root&rev=$browse_revtag&sort=date&author=$author_arg'>Date</A><TH><TH ALIGN=LEFT>Log";
} else {
    $table_header_tag .= "<TH ALIGN=LEFT>Rev<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$url_filename&root=$root&rev=$browse_revtag&sort=author&author=$author_arg'>Author</A><TH ALIGN=LEFT>Date<TH><TH ALIGN=LEFT>Log";
}

$table_header_tag = &url_encode3($table_header_tag);
print "$table_tag$table_header_tag";

# Print each line of the revision, preceded by its annotation.
#
my $row_count = 0;
my $max_rev_length = length($start_rev);
my $max_author_length = 8;
my @revisions = ($start_rev, ancestor_revisions($start_rev));
@revisions = sort by_author @revisions if $opt_sort eq 'author';
#@revisions = sort by_author @revisions if $opt_sort eq 'date' && $rev eq 'all';
my $bgcolor;
foreach $revision (@revisions)
{
    my $author = $::revision_author{$revision};
    next unless $author_arg eq '' || $use_author{$author};

    my $log = $::revision_log{$revision};
    $log =~ s/&/&amp;/g;
    $log =~ s/</&lt;/g;
    $log =~ s/>/&gt;/g;
    $log = MarkUpText($log);
    $log =~ s/\n|\r|\r\n/<BR>/g;

    if ($revision eq $::opt_rev) {
        $bgcolor = ' BGCOLOR="LIGHTBLUE"';
    } elsif ($mark{$revision}) {
        $bgcolor = ' BGCOLOR="LIGHTGREEN"';
    } elsif (!defined $bgcolor || $bgcolor eq '') {
        $bgcolor = ' BGCOLOR="#E7E7E7"'; # Pick a grey that shows up on 8-bit.
    } else {
        $bgcolor = '';
    }
    
    my $output = '';
    $row_count++;
    if ($row_count > 20) {
        $output .= "</TABLE>\n$table_tag";
        $row_count = 0;
    }

    $output .= "<TR$bgcolor VALIGN=TOP><TD>"
        ."<A NAME=$revision>";

    my $anchor = "<A HREF=cvsview2.cgi";

    if (defined($::prev_revision{$revision})) {
        $anchor .= "?diff_mode=context&whitespace_mode=show&file=$url_file_tail&branch=$::opt_rev"
            ."&root=$root&subdir=$rcs_path&command=DIFF_FRAMESET"
            ."&rev1=$::prev_revision{$revision}&rev2=$revision";
    } else {
        $anchor .= "?files=$url_file_tail"
            ."&root=$root&subdir=$rcs_path\&command=DIRECTORY\&rev2=$revision&branch=$::opt_rev";
        $anchor .= "&branch=$browse_revtag" unless $browse_revtag eq 'HEAD';
    }

    $anchor = &url_encode3($anchor);

    $output .= $anchor;

    $output .= ">$revision</A>"
        .'&nbsp' x ($max_rev_length - length($revision)).'</TD>';

    $output .= "<TD>".$author
        .'&nbsp' x ($max_author_length - length($author)).'</TD>';
    my $rev_time = $::revision_ctime{$revision};
#    $rev_time =~ s/(19\d\d) (.\d:\d\d)/$1<BR><FONT SIZE=-2>$2<\/FONT>/;

    # jwz: print the date the way "ls" does.
    #
    # What ls does is actually: print "Mmm DD HH:MM" unless the file is
    # more than six months old, or more than 1 hour in the future, in
    # which case, print "Mmm DD YYYY".
    #
    # What the following does is: "Mmm DD HH:MM" unless the year is not
    # the current year; else print "Mmm DD YYYY".
    #
    # If we had $rev_time as an actual time_t instead of as a string,
    # it would be easy to do the "ls" thing (see the code I wrote for
    # this in "lxr/source").  -jwz, 15-Jun-98.
    #
    {
        my $current_time = time;
        my @t = gmtime($current_time);
        my ($csec, $cmin, $chour, $cmday, $cmon, $cyear) = @t;
        $cyear += 1900;
        $_ = $rev_time;
        $rev_time = "<FONT SIZE=\"-1\">$rev_time</FONT>";
    }

    $output .= "<TD NOWRAP ALIGN=RIGHT>$rev_time</TD>";
    $output .= "<TD>&nbsp;</TD><TD WIDTH=99%>$log</TD>";
    
    $output .= "</TR>\n";

    print $output;
}
print "</TABLE>";
&print_bottom;

## END of main script

sub by_revision {
    my (@a_parts) = split(/\./,$a);
    my (@b_parts) = split(/\./,$b);
    while(1) {
        my ($aa) = shift @a_parts;
        my ($bb) = shift @b_parts;
        return  1 if $aa eq '';
        return -1 if $bb eq '';
        return $bb <=> $aa if $aa ne $bb;
    }
}

sub by_author {
    my ($a_author) = $::revision_author{$a};
    my ($b_author) = $::revision_author{$b};

    return $a_author cmp $b_author if $a_author ne $b_author;
    return by_revision;
}

sub revision_pad {
    my ($revision) = @_;
    return '&nbsp' x ($max_rev_length - length($revision));
}

sub sprint_author {
    my ($revision) = @_;
    my ($author) = $::revision_author{$revision};

    return 
}


sub print_top {
    my ($title_text) = "for $file_tail (";
    $title_text .= "$browse_revtag:" unless $browse_revtag eq 'HEAD';
    $title_text .= $revision if $revision;
    $title_text .= ")";
    $title_text =~ s/\(\)//;

    print <<__TOP__;
<HTML>
<HEAD>
  <TITLE>CVS Log $title_text</TITLE>
</HEAD>
<BODY BGCOLOR=WHITE TEXT=BLACK>

__TOP__
} # print_top

sub print_usage {
    my ($linenum_message) = '';
    my ($new_linenum, $src_roots_list);
    my ($title_text) = "Usage";

    $src_roots_list = join('<BR>', @src_roots);

    print <<__USAGE__;
<HTML>
<HEAD>
<TITLE>CVS Log $title_text</TITLE>
</HEAD><BODY>
<H2>CVS Log Usage</H2>
Add parameters to the query string to view a file.
<P>
<TABLE BORDER CELLPADDING=3>
<TR ALIGN=LEFT>
  <TH>Param</TH>
  <TH>Default</TH>
  <TH>Example</TH>
  <TH>Description</TH>
</TR><TR>
  <TD>file</TD>
  <TD>--</TD>
  <TD>ns/cmd/Makefile</TD>
  <TD>Path to file name</TD>
</TR><TR>
  <TD>root</TD>
  <TD>$src_roots_list</TD>
  <TD>/warp/webroot</TD>
  <TD>CVS root</TD>
</TR><TR>
  <TD>rev</TD>
  <TD>HEAD</TD>
  <TD>1.3
    <BR>ACTRA_branch</TD>
  <TD>Revision</TD>
</TR><TR>
  <TD>author</TD>
  <TD>--</TD>
  <TD>slamm,mtoy</TD>
  <TD>Filter out these authors</TD>
</TR>
</TR><TR>
  <TD>#&lt;rev_number&gt;</TD>
  <TD>--</TD>
  <TD>#1.2</TD>
  <TD>Jump to a revision</TD>
</TR>
</TABLE>

<P>Examples:
<TABLE><TR><TD>&nbsp;</TD><TD>
<A HREF="cvslog.cgi?file=ns/cmd/Makefile">
         cvslog.cgi?file=ns/cmd/Makefile</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvslog.cgi?file=ns/cmd/xfe/mozilla.c&rev=Dogbert4xEscalation_BRANCH">
         cvslog.cgi?file=ns/cmd/xfe/mozilla.c&amp;rev=Dogbert4xEscalation_BRANCH</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvslog.cgi?file=projects/bonsai/cvslog.cgi&root=/warp/webroot">
         cvslog.cgi?file=projects/bonsai/cvslog.cgi&root=/warp/webroot</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvslog.cgi?file=ns/cmd/xfe/dialogs.c#1.19">
         cvslog.cgi?file=ns/cmd/xfe/dialogs.c#1.19</A>
</TD></TR></TABLE>            
<P>
You may also begin a query with the <A HREF="cvsqueryform.cgi">CVS Query Form</A>.
</P>
__USAGE__
    &print_bottom;
} # sub print_usage

sub print_bottom {
     my $maintainer = Param('maintainer');

     print <<__BOTTOM__;
<HR WIDTH="100%">
<FONT SIZE=-1>
<A HREF="cvslog.cgi">Page configuration and help</A>.
Mail feedback to <A HREF="mailto:$maintainer?subject=About the cvslog script">&lt;$maintainer&gt;</A>. 
</FONT></BODY>
</HTML>
__BOTTOM__
} # print_bottom

sub print_useful_links {
    my ($path) = @_;
    my ($dir, $file) = $path =~ m@(.*/)?(.+)@;
    $dir =~ s@/$@@;

    my $diff_base = "cvsview2.cgi";
    my $blame_base = "cvsblame.cgi";

    my $lxr_path = $path;
    my $lxr_link = Fix_LxrLink($lxr_path);
    my $diff_link = "$diff_base?command=DIRECTORY\&subdir=$dir\&files=$file\&branch=$::opt_rev";
    my $blame_link = "$blame_base?root=$::CVS_ROOT\&file=$path\&rev=$::opt_rev";

print "<DIV ALIGN=RIGHT>
 <TABLE BORDER CELLPADDING=10 CELLSPACING=0>
  <TR>
   <TD>
    <TABLE BORDER=0 CELLPADDING=1 CELLSPACING=0>
     <TR>
      <TD VALIGN=TOP ALIGN=RIGHT><A HREF=\"$lxr_link\"><B>lxr:</B></A> </TD>
      <TD>browse the source code as hypertext.</TD>
     </TR>
     <TR>
      <TD VALIGN=TOP ALIGN=RIGHT><A HREF=\"$diff_link\"><B>diff:</B></A> </TD>
      <TD>compare any two versions.</TD>
     </TR>
     <TR>
      <TD VALIGN=TOP ALIGN=RIGHT><A HREF=\"$blame_link\"><B>blame:</B></A> </TD>
      <TD>annotate the author of each line.</TD>
     </TR>
    </TABLE>
   </TD>
  </TR>
 </TABLE>
</DIV>
";
}
