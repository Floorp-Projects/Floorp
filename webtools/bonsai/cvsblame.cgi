#!/usr/bonsaitools/bin/perl --
#  cvsblame.cgi -- cvsblame with logs as popups and allowing html in comments.

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


#  Created: Steve Lamm <slamm@netscape.com>, 12-Sep-97.
#  Modified: Marc Byrd <byrd@netscape.com> , 19971030.
#
#  Arguments (passed via GET or POST):
#    file - path to file name (e.g. ns/cmd/xfe/Makefile)
#    root - cvs root (e.g. /warp/webroot)
#         - default includes /m/src/ and /h/rodan/cvs/repository/1.0
#    rev  - revision (default is the latest version)
#    line_nums - boolean for line numbers on/off (use 1,0).
#                (1,on by default)
#    use_html - boolean for html comments on/off (use 1,0).
#                (0,off by default)
#    sanitize - path to sanitization dictionary
#               (e.g. /warp2/webdoc/projects/bonsai/dictionary/sanitization.db)
#    mark - highlight a line
#

require 'lloydcgi.pl';
require 'cvsblame.pl';
require 'utils.pl';
use SourceChecker;

$| = 1;

# Cope with the cookie and print the header, first thing.  That way, if
# any errors result, they will show up for the user.

print "Content-Type:text/html\n";
if ($ENV{"REQUEST_METHOD"} eq 'POST' && defined($form{'set_line'})) {
    # Expire the cookie 5 months from now
    print "Set-Cookie: line_nums=$form{'set_line'}; expires="
        . toGMTString(time + 86400 * 152) . "; path=/\n";
}     
print "\n";


# Some Globals
#

@src_roots = getRepositoryList();

# Do not use layers if the client does not support them.
$useLayers = 1;
$user_agent = $ENV{HTTP_USER_AGENT};
if (not $user_agent =~ m@^Mozilla/4.@ or $user_agent =~ /MSIE/) {
  $useLayers = 0;
}

#if ($user_agent =~ /Win/) {
#    $font_tag = "<PRE><FONT FACE='Lucida Console' SIZE=-1>";
#} else {
#    # We don't want your stinking Windows font
    $font_tag = "<PRE>";
#}

# Init sanitiazation source checker
#
$sanitization_dictionary = $form{'sanitize'};
$opt_sanitize = defined $sanitization_dictionary;
if ( $opt_sanitize )
{
    dbmopen %SourceChecker::token_dictionary, "$sanitization_dictionary", 0664;
}

# Init byrd's 'feature' to allow html in comments
#
$opt_html_comments = &html_comments_init();


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
$browse_revtag = "HEAD";
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

CheckHidden($rcs_filename);


# Parse the rcs file ($opt_rev is passed as a global)
#
$revision = &parse_cvs_file($rcs_filename);
$file_rev = $revision;

# Handle the "line_nums" argument
#
$opt_line_nums = 1;
$opt_line_nums = 1 if $cookie_jar{'line_nums'} eq 'on';
$opt_line_nums = 0 if $form{'line_nums'} =~ /off|no|0/i;
$opt_line_nums = 1 if $form{'line_nums'} =~ /on|yes|1/i;

# Option to make links to included files
$opt_includes = 0;
$opt_includes = 1 if $form{'includes'} =~ /on|yes|1/i;
$opt_includes = 1 if $opt_includes && $file_tail =~ /(.c|.h|.cpp)$/;

@text = &extract_revision($revision);
die "$progname: Internal consistency error" if ($#text != $#revision_map);


# Handle the "mark" argument
#
$mark_arg = '';
$mark_arg = $form{'mark'} if defined($form{'mark'});
foreach $mark (split(',',$mark_arg)) {
    if (($begin, $end) = $mark =~ /(\d*)\-(\d*)/) {
        $begin = 1 if $begin eq '';
        $end = $#text + 1 if $end eq '' || $end > $#text + 1;
        next if $begin >= $end;
        $mark_line{$begin} = 'begin';
        $mark_line{$end} = 'end';
    } else {
        $mark_line{$mark} = 'single';
    }
}

# Start printing out the page
#
&print_top;

# Print link at top for directory browsing
#
print q(
<TABLE BGCOLOR="#000000" WIDTH="100%" BORDER=0 CELLPADDING=0 CELLSPACING=0>
<TR><TD><A HREF="http://www.mozilla.org/"><IMG
 SRC="http://www.mozilla.org/images/mozilla-banner.gif" ALT=""
 BORDER=0 WIDTH=600 HEIGHT=58></A></TD></TR></TABLE>
<TABLE BORDER=0 CELLPADDING=5 CELLSPACING=0 WIDTH="100%">
 <TR>
  <TD ALIGN=LEFT VALIGN=CENTER>
   <NOBR><FONT SIZE="+2"><B>
    Blame Annotated Source
   </B></FONT></NOBR>
   <BR><B>
);

foreach $path (split('/',$rcs_path)) {
    $link_path .= url_encode2($path).'/' if $path ne 'mozilla';
    print "<A HREF='http://lxr.mozilla.org/mozilla/source/$link_path'>$path</a>/ ";
}
print "<A HREF='http://lxr.mozilla.org/mozilla/source/$link_path$file_tail'>$file_tail</a> ";

print " (<A HREF='cvsblame.cgi?file=$filename&rev=$revision&root=$root'";
print " onmouseover='return log(event,\"$prev_revision{$revision}\",\"$revision\");'" if $useLayers;
print ">";
print "$browse_revtag:" unless $browse_revtag eq 'HEAD';
print $revision if $revision;
print "</A>)";

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
        </TD><TD>
         Browse the source code as hypertext.
        </TD>
       </TR><TR>
        <TD>
         <A HREF="cvslog.cgi?file=$filename">log</A>&nbsp;
        </TD><TD>
         Full change log.
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



print $font_tag;

# Print each line of the revision, preceded by its annotation.
#
$start_of_mark = 0;
$end_of_mark = 0;
$count = $#revision_map;
if ($count == 0) {
    $count = 1;
}
$line_num_width = int(log($count)/log(10)) + 1;
$revision_width = 3;
$author_width = 5;
$line = 0;
$usedlog{$revision} = 1;
foreach $revision (@revision_map)
{
    $text = $text[$line++];
    $usedlog{$revision} = 1;


    if ($opt_html_comments) {
        # Don't escape HTML in C/C++ comments
        $text = &leave_html_comments($text);
    } elsif ( $opt_sanitize ){
        # Mark filty words and Escape HTML meta-characters
        $text = markup_line($text);
    } else {
        $text =~ s/&/&amp;/g;
        $text =~ s/</&lt;/g;
        $text =~ s/>/&gt;/g;
    }
    # Add a link to traverse to included files
    $text = &link_includes($text) if $opt_includes;


    $output = "<A NAME=$line></A>";

    # Highlight lines
    if (defined($mark_cmd = $mark_line{$line})
        && $mark_cmd ne 'end') {
	$output .= '</td></tr></TABLE>' if $useAlternateColor;
        $output .= '<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>'
            ."<TR><TD BGCOLOR=LIGHTGREEN WIDTH='100%'>$font_tag";
	$inMark = 1;
    }

    if ($old_revision ne $revision) {
      if ($useAlternateColor) {
	$useAlternateColor = 0;
	$output .= "</td></tr></TABLE>$font_tag" unless $inMark;
      } else {
	$useAlternateColor = 1;
	$output .= '<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH="100%">'
	  . "<TR><TD BGCOLOR=#e7e7e7>$font_tag" unless $inMark;
      }
    }

    $output .= sprintf("%${line_num_width}s ", $line) if $opt_line_nums;

    if ($old_revision ne $revision || $rev_count > 20) {

        $revision_width = max($revision_width,length($revision));

        $output .= "<A HREF ='cvsblame.cgi?file=$filename&rev=$revision&root=$root'";
	$output .= "onmouseover='return log(event,\"$prev_revision{$revision}\",\"$revision\");'" if $useLayers;
        $output .= ">";
	$author = $revision_author{$revision};
	$author =~ s/%.*$//;
        $author_width = max($author_width,length($author));
        $output .= sprintf("%-${author_width}s ", $author);
	$output .= "$revision</A> ";
        $output .= ' ' x ($revision_width - length($revision));

        $old_revision = $revision;
        $rev_count = 0;
    } else {
        $output .= '| ' . ' ' x ($author_width + $revision_width);
    }
    $rev_count++;

    $output .= "$text";
    
    # Close the highlighted section
    if (defined($mark_cmd) and $mark_cmd ne 'begin') {
        chop($output);
        $output .= "</TD>";
        #if( defined($prev_revision{$file_rev})) {
            $output .= "<TD ALIGN=RIGHT><A HREF=\"cvsblame.cgi?file=$filename&rev=$prev_revision{$file_rev}&root=$root&mark=$mark_arg\">Previous&nbsp;Revision&nbsp;($prev_revision{$file_rev})</A></TD><TD BGCOLOR=LIGHTGREEN>&nbsp</TD>";
        #}
        $output .= "</TR></TABLE>";
	$output .= '<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH="100%">'
	  . "<TR><TD BGCOLOR=#e7e7e7>$font_tag" if $useAlternateColor;
	$inMark = 0;
    }

    print $output;
}
if ($useAlternateColor) {
    print "</td></tr></TABLE>$font_tag" unless $inMark;
}
print "</FONT></PRE>\n";

if ($useLayers) {
  # Write out cvs log messages as a JS variables
  #
  print "<SCRIPT>";
  while (($revision, $junk) = each %usedlog) {
    
    # Create a safe variable name for a revision log
    $revisionName = $revision;
    $revisionName =~ tr/./_/;
    
    $log = $revision_log{$revision};
    $log =~ s/([^\n\r]{80})([^\n\r]*)/$1\n$2/g;
    eval ('$log =~ s@\d{4,6}@' . $BUGSYSTEMEXPR . '@g;');
    $log =~ s/\n|\r|\r\n/<BR>/g;
    $log =~ s/"/\\"/g;
    
    # Write JavaScript variable for log entry (e.g. log1_1 = "New File")
    $author = $revision_author{$revision};
    $author =~ tr/%/@/;
    print "log$revisionName = \""
      ."<b>$revision</b> &lt;<a href='mailto:$author'>$author</a>&gt;"
	." <b>$revision_ctime{$revision}</b><BR>"
	  ."<SPACER TYPE=VERTICAL SIZE=5>$log\";\n";
  }
  print "</SCRIPT>";
}

&print_bottom;

if ( $opt_sanitize )
{
    dbmclose %SourceChecker::token_dictionary;
}

## END of main script

sub max {
    my ($a, $b) = @_;
    return ($a > $b ? $a : $b);
}

sub print_top {
    my ($title_text) = "for $file_tail (";
    $title_text .= "$browse_revtag:" unless $browse_revtag eq 'HEAD';
    $title_text .= $revision if $revision;
    $title_text .= ")";
    $title_text =~ s/\(\)//;

    my ($diff_dir_link) = 
        "cvsview2.cgi?subdir=$rcs_path&files=$file_tail&command=DIRECTORY";
    $diff_dir_link .= "&root=$form{'root'}" if defined $form{'root'};
    $diff_dir_link .= "&branch=$browse_revtag" unless $browse_revtag eq 'HEAD';

    my ($diff_link) = "cvsview2.cgi?diff_mode=context&whitespace_mode=show";
    $diff_link .= "&root=$form{'root'}" if defined $form{'root'};
    $diff_link .= "&subdir=$rcs_path&command=DIFF_FRAMESET&file=$file_tail";
    
    $diff_link = &url_encode3($diff_link);

    print "<HTML><HEAD><TITLE>CVS Blame $title_text</TITLE>";

    print <<__TOP__ if $useLayers;
<SCRIPT>
var event = 0;	// Nav3.0 compatibility
document.loaded = false;

function finishedLoad() {
    if (parseInt(navigator.appVersion) < 4) {
        return true;
    }
    document.loaded = true;
    document.layers['popup'].visibility='hide';
    
    return true;
}

function revToName (rev) {
    revName = rev + "";
    revArray = revName.split(".");
    return revArray.join("_");
}

function log(event, prev_rev, rev) {
    window.defaultStatus = "";
    if (prev_rev == '') {
        window.status = "View diffs for " + file_tail;
    } else {
        window.status = "View diff for " + prev_rev + " vs." + rev;
    }

    if (parseInt(navigator.appVersion) < 4) {
        return true;
    }

    var l = document.layers['popup'];
    var shadow = document.layers['shadow'];
    if (document.loaded) {
        l.document.write("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#F0A000>");
        l.document.write("<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF><tt>");
        l.document.write(eval("log" + revToName(rev)) + "</TD></TR></TABLE>");
	l.document.write("</td></tr></table>");
        l.document.close();
    }

    if(event.target.y > window.innerHeight + window.pageYOffset - l.clip.height) { 
         l.top = (window.innerHeight + window.pageYOffset - (l.clip.height + 15));
    } else {
         l.top = event.target.y - 9;
    }
    l.left = event.target.x + 70;

    l.visibility="show";

    return true;
}

file_tail = "$file_tail";

initialLayer = "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#F0A000><TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF><B>Page loading...please wait.</B></TD></TR></TABLE></td></tr></table>";

</SCRIPT>
</HEAD>
<BODY onLoad="finishedLoad();" BGCOLOR="#FFFFFF" TEXT="#000000" LINK="#0000EE" VLINK="#551A8B" ALINK="#F0A000">
<LAYER SRC="javascript:initialLayer" NAME='popup' onMouseOut="this.visibility='hide';" LEFT=0 TOP=0 BGCOLOR='#FFFFFF' VISIBILITY='hide'></LAYER>
__TOP__
  print '<BODY BGCOLOR="#FFFFFF" TEXT="#000000" LINK="#0000EE" VLINK="#551A8B" ALINK="#F0A000">' if not $useLayers;
} # print_top

sub print_usage {
    my ($linenum_message) = '';
    my ($new_linenum, $src_roots_list);
    my ($title_text) = "Usage";

    if ($ENV{"REQUEST_METHOD"} eq 'POST' && defined($form{'set_line'})) {
  
        # Expire the cookie 5 months from now
        $set_cookie = "Set-Cookie: line_nums=$form{'set_line'}; expires="
            .&toGMTString(time + 86400 * 152)."; path=/";
    }
    if (!defined($cookie_jar{'line_nums'}) && !defined($form{'set_line'})) {
        $new_linenum = 'on';
    } elsif ($cookie_jar{'line_nums'} eq 'off' || $form{'set_line'} eq 'off') {
        $linenum_message = 'Line numbers are currently <b>off</b>.';
        $new_linenum = 'on';
    } else {
        $linenum_message = 'Line numbers are currently <b>on</b>.';
        $new_linenum = 'off';
    }
    $src_roots_list = join('<BR>', @src_roots);

    print <<__USAGE__;
<HTML>
<HEAD>
<TITLE>CVS Blame $title_text</TITLE>
</HEAD><BODY>
<H2>CVS Blame Usage</H2>
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
  <TD>path to file name</TD>
</TR><TR>
  <TD>root</TD>
  <TD>$src_roots_list</TD>
  <TD>/warp/webroot</TD>
  <TD>cvs root</TD>
</TR><TR>
  <TD>rev</TD>
  <TD>HEAD</TD>
  <TD>1.3
    <BR>ACTRA_branch</TD>
  <TD>revision</TD>
</TR><TR>
  <TD>line_nums</TD>
  <TD>off *</TD>
  <TD>on
    <BR>off</TD>
  <TD>line numbers</TD>
</TR><TR>
  <TD>#&lt;line_number&gt;</TD>
  <TD>--</TD>
  <TD>#111</TD>
  <TD>jump to a line</TD>
</TR>
</TABLE>

<P>Examples:
<TABLE><TR><TD>&nbsp;</TD><TD>
<A HREF="cvsblame.cgi?file=ns/cmd/Makefile">
         cvsblame.cgi?file=ns/cmd/Makefile</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvsblame.cgi?file=ns/cmd/xfe/mozilla.c&rev=Dogbert4xEscalation_BRANCH">
         cvsblame.cgi?file=ns/cmd/xfe/mozilla.c&amp;rev=Dogbert4xEscalation_BRANCH</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvsblame.cgi?file=projects/bonsai/cvsblame.cgi&root=/warp/webroot">
         cvsblame.cgi?file=projects/bonsai/cvsblame.cgi&root=/warp/webroot</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvsblame.cgi?file=ns/config/config.mk&line_nums=on">
         cvsblame.cgi?file=ns/config/config.mk&amp;line_nums=on</A>
</TD></TR><TR><TD>&nbsp;</TD><TD>
<A HREF="cvsblame.cgi?file=ns/cmd/xfe/dialogs.c#2384">
         cvsblame.cgi?file=ns/cmd/xfe/dialogs.c#2384</A>
</TD></TR></TABLE>            

<P>
You may also begin a query with the <A HREF="cvsqueryform.cgi">CVS Query Form</A>.
<FORM METHOD='POST' ACTION='cvsblame.cgi'>

<TABLE CELLPADDING=0 CELLSPACING=0>
<TR>
   <TD>*<SPACER TYPE=HORIZONTAL SIZE=6></TD>
   <TD>Instead of the <i>line_nums</i> parameter, you can
       <INPUT TYPE=submit value='set a cookie to turn $new_linenum'>
       line numbers.</TD>
</TR><TR>
   <TD></TD>
   <TD>$linenum_message</TD>
</TR></TABLE>

<INPUT TYPE=hidden NAME='set_line' value='$new_linenum'>
</FORM>
__USAGE__
    &print_bottom;
} # sub print_usage

sub print_bottom {
    print <<__BOTTOM__;
<HR WIDTH="100%">
<FONT SIZE=-1>
<A HREF="cvsblame.cgi">Page configuration and help</A>.
Mail feedback to <A HREF="mailto:slamm?subject=About the cvsblame script">&lt;slamm\@netscape.com></A>. 
</FONT></BODY>
</HTML>
__BOTTOM__
} # print_bottom


sub link_includes {
    my ($text) = $_[0];

    if ($text =~ /\#(\s*)include(\s*)"(.*?)"/) {
        foreach $trial_root (($rcs_path, 'ns/include', 
                              "$rcs_path/Attic", "$rcs_path/..")) {
            if (-r "$root/$trial_root/$3,v") {
                $text = "$`#$1include$2\"<A HREF='cvsblame.cgi"
                    ."?root=$root&file=$trial_root/$3&rev=".$browse_revtag
                    ."&use_html=$use_html'>$3</A>\";$'";
                last;
            }
        }
    }
    return $text;
}

sub html_comments_init {
    return 0 unless defined($form{'use_html'}) && $form{'use_html'};
                                                      
    # Initialization for C comment context switching
    $in_comments = 0;
    $open_delim = '\/\*';
    $close_delim = '\*\/';

    # Initialize the next expected delim
    $expected_delim = $open_delim;

    return 1;
}

sub leave_html_comments {
    my ($text) = $_[0];
    # Allow HTML in the comments.
    #
    $newtext = "";
    $oldtext = $text;
    while ($oldtext =~ /(.*$expected_delim)(.*\n)/) {
        $a = $1;
        $b = $2;
        # pay no attention to C++ comments within C comment context
        if ($in_comments == 0) {
            $a =~ s/</&lt;/g;
            $a =~ s/>/&gt;/g;
            $expected_delim = $close_delim;
            $in_comments = 1;
        }
        else {
            $expected_delim = $open_delim;
            $in_comments = 0;
        }
        $newtext = $newtext . $a;
        $oldtext = $b;
    }
    # Handle thre remainder
    if ($in_comments == 0){
      $oldtext =~ s/</&lt;/g;
      $oldtext =~ s/>/&gt;/g;
    }
    $text = $newtext . $oldtext;

    # Now fix the breakage of <username> stuff on xfe. -byrd
    if ($text =~ /(.*)<(.*@.*)>(.*\n)/) {
        $text = $1 . "<A HREF=mailto:$2?subject=$filename>$2</A>" . $3;
    }

    return $text;
}
