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

require 'lloydcgi.pl';
require 'cvsblame.pl';
require 'utils.pl';
use SourceChecker;

# Some Globals
#

$| = 1;

print "Content-Type:text/html\n\n";

@src_roots = getRepositoryList();


# Handle the "file" argument
#
$filename = '';
$filename = $form{'file'} if defined($form{'file'});
if ($filename eq '') 
{
    &print_usage;
    exit;
}
($file_head, $file_tail) = $filename =~ m@(.*/)?(.+)@;


# Handle the "rev" argument
#
$opt_rev = $form{'rev'} if defined($form{'rev'} && $form{'rev'} ne 'HEAD');
$browse_revtag = 'HEAD';
$browse_revtag = $opt_rev if ($opt_rev =~ /[A-Za-z]/);
$revision = '';


# Handle the "root" argument
#
if (defined($root = $form{'root'}) && $root ne '') {
    $root =~ s|/$||;
    validateRepository($root);
    if (-d $root) {
        unshift(@src_roots, $root);
    } else {
        &print_top;
        print "Error:  Root, $root, is not a directory.<BR><BR>\n";
        print "</BODY></HTML>\n";
        &print_bottom;
        exit;
    }
}



# Find the rcs file
#
foreach (@src_roots) {
    $root = $_;
    $rcs_filename = "$root/$filename,v";
    goto found_file if -r $rcs_filename;
    $rcs_filename = "$root/${file_head}Attic/$file_tail,v";
    goto found_file if -r $rcs_filename;
}
# File not found
&print_top;
print "Rcs file, $filename, does not exist.<BR><BR>\n";
print "</BODY></HTML>\n";
&print_bottom;
exit;

found_file:
    ($rcs_path) = $rcs_filename =~ m@$root/(.*)/.+?,v@;


# Parse the rcs file ($opt_rev is passed as a global)
#
$revision = &parse_cvs_file($rcs_filename);
$file_rev = $revision;


# Handle the "mark" argument
#
$mark_arg = '';
$mark_arg = $form{'mark'} if defined($form{'mark'});
foreach $rev (split(',',$mark_arg)) {
        $mark{$rev} = 1;
}


# Handle the "author" argument
#
$author_arg = '';
$author_arg = $form{'author'} if defined($form{'author'});
foreach $author (split(',',$author_arg)) {
    $use_author{$author} = 1;
}


# Handle the "sort" argument
$opt_sort = '';
$opt_sort = $form{'sort'};


# Start printing out the page
#
&print_top;

open(BANNER, "<data/banner.html");
print while <BANNER>;

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

foreach $path (split('/',$rcs_path)) {
    $link_path .= url_encode2($path).'/' if $path ne 'mozilla';
    print "<A HREF='http://lxr.mozilla.org/mozilla/source/$link_path'>$path</a>/ ";
}
print "<A HREF='http://lxr.mozilla.org/mozilla/source/$link_path$file_tail'>$file_tail</a> ";

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
         <A HREF="http://lxr.mozilla.org/mozilla/source/$link_path$file_tail">lxr</A>
        </TD><TD NOWRAP>
         Browse the source code as hypertext.
        </TD>
       </TR><TR>
        <TD>
         <A HREF="cvsview2.cgi?command=DIRECTORY&subdir=$rcs_path&files=$file_tail">diff</A>
        </TD><TD NOWRAP>
         Compare any two version.
        </TD>
       </TR><TR>
        <TD>
         <A HREF="cvsblame.cgi?file=$filename">blame</A>&nbsp;
        </TD><TD NOWRAP>
         Annotate the author of each line.
        </TD>
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
$table_tag = "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3 WIDTH='100%'>";
if ($opt_sort eq 'author') {
    $table_header_tag .= "<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$filename&root=$root&rev=$browse_revtag&sort=revision&author=$author_arg'>Rev</A><TH ALIGN=LEFT>Author<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$filename&root=$root&rev=$browse_revtag&sort=date&author=$author_arg'>Date</A><TH><TH ALIGN=LEFT>Log";
} else {
    $table_header_tag .= "<TH ALIGN=LEFT>Rev<TH ALIGN=LEFT><A HREF='cvslog.cgi?file=$filename&root=$root&rev=$browse_revtag&sort=author&author=$author_arg'>Author</A><TH ALIGN=LEFT>Date<TH><TH ALIGN=LEFT>Log";
}

$table_header_tag = &url_encode3($table_header_tag);
print "$table_tag$table_header_tag";

# Print each line of the revision, preceded by its annotation.
#
if ($browse_revtag eq 'HEAD') {
    $start_rev = $head_revision;  # $head_revision is a global from cvsblame.pl
} else {
    $start_rev = map_tag_to_revision($browse_revtag);
}
$row_count = 0;
$max_rev_length = length($start_rev);
$max_author_length = 8;
@revisions = ($start_rev, ancestor_revisions($start_rev));
@revisions = sort by_author @revisions if $opt_sort eq 'author';
#@revisions = sort by_author @revisions if $opt_sort eq 'date' && $rev eq 'all';
foreach $revision (@revisions)
{
    $author = $revision_author{$revision};
    next unless $author_arg eq '' || $use_author{$author};

    $log = $revision_log{$revision};
    $log =~ s/&/&amp;/g;
    $log =~ s/</&lt;/g;
    $log =~ s/>/&gt;/g;
    eval ('$log =~ s@\d{4,6}@' . $BUGSYSTEMEXPR . '@g;');
    $log =~ s/\n|\r|\r\n/<BR>/g;

    if ($bgcolor eq '') {
        #$bgcolor = ' BGCOLOR="#EEEEEE"';# My browser translates this to white.
        $bgcolor = ' BGCOLOR="#E7E7E7"'; # Pick a grey that shows up on 8-bit.
    } else {
        $bgcolor = '';
    }
    
    $output = '';
    $row_count++;
    if ($row_count > 20) {
        $output .= "</TABLE>\n$table_tag";
        $row_count = 0;
    }

    $output .= "<TR$bgcolor VALIGN=TOP><TD>"
        ."<A NAME=$revision>";

    $anchor = "<A HREF=cvsview2.cgi";

    if (defined($prev_revision{$revision})) {
        $anchor .= "?diff_mode=context&whitespace_mode=show&file=$file_tail"
            ."&root=$root&subdir=$rcs_path&command=DIFF_FRAMESET"
            ."&rev1=$prev_revision{$revision}&rev2=$revision";
    } else {
        $anchor .= "?files=$file_tail"
            ."&root=$root&subdir=$rcs_path\&command=DIRECTORY\&rev2=$revision";
        $anchor .= "&branch=$browse_revtag" unless $browse_revtag eq 'HEAD';
    }

    $anchor = &url_encode3($anchor);

    $output .= $anchor;

    $output .= ">$revision</A>"
        .'&nbsp' x ($max_rev_length - length($revision)).'</TD>';

    $output .= "<TD>".$author
        .'&nbsp' x ($max_author_length - length($author)).'</TD>';
    $rev_time = $revision_ctime{$revision};
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
        my ($rday, $rmon, $ryear, $rhour, $rmin) =
            m/([0-9]+) ([A-Z][a-z]+) ([0-9][0-9]+) +([0-9]+):([0-9]+)/;
        $rmon =~ s/^(...).*$/$1/;

        if (!$rday) {
            # parse error -- be annoying so somebody complains.
            $rev_time = "<BLINK>\"$rev_time\"</BLINK>";
        } elsif ($cyear ne $ryear) {
            $rev_time = sprintf("%s %2d  %04d", $rmon, $rday, $ryear);
        } else {
            $rev_time = sprintf("%s %2d %02d:%02d",
                                $rmon, $rday, $rhour, $rmin);
        }

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
    local (@a_parts) = split(/\./,$a);
    local (@b_parts) = split(/\./,$b);
    while(1) {
        local ($aa) = shift @a_parts;
        local ($bb) = shift @b_parts;
        return  1 if $aa eq '';
        return -1 if $bb eq '';
        return $bb <=> $aa if $aa ne $bb;
    }
}

sub by_author {
    local ($a_author) = $revision_author{$a};
    local ($b_author) = $revision_author{$b};

    return $a_author cmp $b_author if $a_author ne $b_author;
    return by_revision;
}

sub revision_pad {
    local ($revision) = @_;
    return '&nbsp' x ($max_rev_length - length($revision));
}

sub sprint_author {
    local ($revision) = @_;
    local ($author) = $revision_author{$revision};

    return 
}


sub print_top {
    local ($title_text) = "for $file_tail (";
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
    local ($linenum_message) = '';
    local ($new_linenum, $src_roots_list);
    local ($title_text) = "Usage";

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
    print <<__BOTTOM__;
<HR WIDTH="100%">
<FONT SIZE=-1>
<A HREF="cvslog.cgi">Page configuration and help</A>.
Mail feedback to <A HREF="mailto:slamm\@netscape.com?subject=About the cvslog script">&lt;slamm\@netscape.com></A>. 
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

    # total kludge!!  lxr omits the top-level "mozilla" directory...
    my $lxr_path = $path;
    if ($mozilla_lxr_kludge eq 'TRUE') {
      $lxr_path =~ s@^ns/@@;
      $lxr_path =~ s@^mozilla/@@;
    }

    my $lxr_link = "$lxr_base/$lxr_path";
    my $diff_link = "$diff_base?command=DIRECTORY\&subdir=$dir\&files=$file";
    my $blame_link = "$blame_base?root=$CVS_ROOT\&file=$path";

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
