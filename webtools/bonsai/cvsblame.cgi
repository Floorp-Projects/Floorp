#!/usr/bonsaitools/bin/perl -w
#  cvsblame.cgi -- cvsblame with logs as popups and allowing html in comments.

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

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::progname;
    $zz = $::revision_ctime;
    $zz = $::revision_log;
}

require 'CGI.pl';
require 'cvsblame.pl';

# Cope with the cookie and print the header, first thing.  That way, if
# any errors result, they will show up for the user.

print "Content-Type:text/html\n";
if ($ENV{REQUEST_METHOD} eq 'POST' and defined $::FORM{set_line}) {
    # Expire the cookie 5 months from now
    print "Set-Cookie: line_nums=$::FORM{set_line}; expires="
        . toGMTString(time + 86400 * 152) . "; path=/\n";
}

# Some Globals
#
my $Head = 'CVS Blame';
my $SubHead = '';

my @src_roots = getRepositoryList();

# Init byrd's 'feature' to allow html in comments
#
my $opt_html_comments = &html_comments_init();


# Handle the "file" argument
#
my $filename = '';
$filename = $::FORM{file} if defined $::FORM{file};
if ($filename eq '') 
{
    print "\n";
    &print_usage;
    exit;
} elsif ($filename =~ /CVSROOT/) {
    print "\nFiles in the CVSROOT dir cannot be viewed.\n";
    exit;
}
my ($file_head, $file_tail) = $filename =~ m@(.*/)?(.+)@;
my $url_filename = url_quote($filename);
my $url_file_tail = url_quote($file_tail);

# Handle the "rev" argument
#
$::opt_rev = '';
$::opt_rev = sanitize_revision($::FORM{rev}) if 
    defined $::FORM{rev} and $::FORM{rev} ne 'HEAD';
my $revstr = '';
$revstr = "&rev=$::opt_rev" unless $::opt_rev eq '';
my $browse_revtag = 'HEAD';
$browse_revtag = $::opt_rev if ($::opt_rev =~ /[A-Za-z]/);
my $revision = '';

# Handle the "root" argument
#
my $root = $::FORM{root};
if (defined $root and $root ne '') {
    $root =~ s|/$||;
    validateRepository($root);
    if (-d $root) {
        unshift(@src_roots, $root);
    } else {
        print "\n";
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
my $found_rcs_file = 0;
foreach (@src_roots) {
    $root = $_;
    $rcs_filename = "$root/$filename,v";
    $rcs_filename = Fix_BonsaiLink($rcs_filename);
    $found_rcs_file = 1, last if -r $rcs_filename;
    $rcs_filename = "$root/${file_head}Attic/$file_tail,v";
    $found_rcs_file = 1, last if -r $rcs_filename;
}

unless ($found_rcs_file) {
  print "\n";
  &print_top;
  my $escaped_filename = html_quote($filename);
  my $shell_filename = shell_escape($filename);
  print STDERR "cvsblame.cgi: Rcs file, $shell_filename, does not exist.\n";
  print "Invalid filename: $escaped_filename.\n";
  &print_bottom;
  exit;
}

my $rcs_path;
($rcs_path) = $rcs_filename =~ m@$root/(.*)/.+?,v@;

CheckHidden($rcs_filename);

# Parse the rcs file ($::opt_rev is passed as a global)
#
$revision = &parse_cvs_file($rcs_filename);
my $file_rev = $revision;

my @text = &extract_revision($revision);
if ($#text != $#::revision_map) {
    print "\n";
    die "$::progname: Internal consistency error"
}

# Raw data opt (so other scripts can parse and play with the data)
if (defined $::FORM{data}) {
    print "\n";
    &print_raw_data;
    exit;
}

print "Last-Modified: ".time2str("%a, %d %b %Y %T %Z", str2time($::revision_ctime{$::opt_rev}), "GMT")."\n";
print "Expires: ".time2str("%a, %d %b %Y %T %Z", time+1200, "GMT")."\n";
print "\n"; 
#ENDHEADERS!!

# Handle the "line_nums" argument
#
my $opt_line_nums = 1;
if (defined $::COOKIE{line_nums}) {
    $opt_line_nums = 1 if $::COOKIE{line_nums} eq 'on';
}
if (defined $::FORM{line_nums}) {
     $opt_line_nums = 0 if $::FORM{line_nums} =~ /off|no|0/i;
     $opt_line_nums = 1 if $::FORM{line_nums} =~ /on|yes|1/i;
}

# Option to make links to included files
my $opt_includes = 0;
$opt_includes = 1 if defined $::FORM{includes} and
  $::FORM{includes} =~ /on|yes|1/i;
$opt_includes = 1 if $opt_includes and $file_tail =~ /(.c|.h|.cpp)$/;

my $use_html = 0;
$use_html = 1 if defined $::FORM{use_html} and $::FORM{use_html} eq '1';

# Handle the "mark" argument
#
my %mark_line;
my $mark_arg = '';
$mark_arg = $::FORM{mark} if defined $::FORM{mark};
foreach my $mark (split ',', $mark_arg) {
    my ($begin, $end);
    if (($begin, $end) = $mark =~ /(\d*)\-(\d*)/) {
        $begin = 1 if $begin eq '';
        $end = $#text + 1 if $end eq '' or $end > $#text + 1;
        next if $begin >= $end;
        $mark_line{$begin} = 'begin';
        $mark_line{$end}   = 'end';
    } else {
        $mark_line{$mark} = 'single';
    }
}

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
    CVS Blame
   </B></FONT></NOBR>
   <BR><B>
);

my $link_path = "";
foreach my $path (split('/',$rcs_path)) {

    # Customize this translation
    $link_path .= url_encode2($path).'/';
    my $lxr_path = Fix_LxrLink($link_path);
    print "<A HREF='$lxr_path'>$path</a>/ ";
}
my $lxr_path = Fix_LxrLink("$link_path$file_tail");
print "<A HREF='$lxr_path'>$file_tail</a> ";

my $graph_cell = Param('cvsgraph') ? <<"--endquote--" : "";
       </TR><TR>
        <TD NOWRAP>
         <A HREF="cvsgraph.cgi?file=$url_filename">Revision Graph</A>
        </TD>
--endquote--

print " (<A HREF='cvsblame.cgi?file=$url_filename&rev=$revision&root=$root'";
print " onmouseover='return log(event,\"$::prev_revision{$revision}\",\"$revision\");'" if $::use_layers;
print " onmouseover=\"showMessage('$revision','top')\" id=\"line_top\"" if $::use_dom;
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
        <TD NOWRAP>
         <A HREF="$lxr_path">LXR: Cross Reference</A>
        </TD>
       </TR><TR>
        <TD NOWRAP>
         <A HREF="cvslog.cgi?file=$url_filename$revstr">Full Change Log</A>
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

my $open_table_tag =
    '<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH="100%">';
print "$open_table_tag<TR><TD colspan=3><PRE>";

# Print each line of the revision, preceded by its annotation.
#
my $count = $#::revision_map;
if ($count == 0) {
    $count = 1;
}
my $line_num_width = int(log($count)/log(10)) + 1;
my $revision_width = 3;
my $author_width = 5;
my $line = 0;
my %usedlog;
$usedlog{$revision} = 1;
my $old_revision = 0;
my $row_color = '';
my $lines_in_table = 0;
my $inMark = 0;
my $rev_count = 0;
foreach $revision (@::revision_map)
{
    my $text = $text[$line++];
    $usedlog{$revision} = 1;
    $lines_in_table++;

    if ($opt_html_comments) {
        # Don't escape HTML in C/C++ comments
        $text = &leave_html_comments($text);
    } else {
        $text =~ s/&/&amp;/g;
        $text =~ s/</&lt;/g;
        $text =~ s/>/&gt;/g;
    }
    # Add a link to traverse to included files
    $text = &link_includes($text) if $opt_includes;

    my $output = '';

    # Highlight lines
    my $mark_cmd;
    if (defined($mark_cmd = $mark_line{$line}) and $mark_cmd ne 'end') {
	$output .= '</TD></TR><TR><TD BGCOLOR=LIGHTGREEN WIDTH="100%"><PRE>';
	$inMark = 1;
    }

    if ($old_revision ne $revision and $line != 1) {
      if ($row_color eq '') {
	$row_color=' BGCOLOR="#e7e7e7"';
      } else {
	$row_color='';
      }
      if (not $inMark) {
	if ($lines_in_table > 100) {
	  $output .= "</TD></TR></TABLE>$open_table_tag<TR><TD colspan=3$row_color><PRE>";
	  $lines_in_table=0;
	} else {
	  $output .= "</TD></TR><TR><TD colspan=3$row_color><PRE>";
	}
      }
    } elsif ($lines_in_table > 200 and not $inMark) {
      $output .= "</TD></TR></TABLE>$open_table_tag<TR><TD colspan=3$row_color><PRE>";
      $lines_in_table=0;
    }

    $output .= "<A NAME=$line></A>";

    $output .= sprintf("%${line_num_width}s ", $line) if $opt_line_nums;

    if ($old_revision ne $revision or $rev_count > 20) {

        $revision_width = max($revision_width,length($revision));

	if ($::prev_revision{$revision}) {
	  $output .= "<A HREF=\"cvsview2.cgi?diff_mode=context&whitespace_mode=show&root=$root&subdir=$rcs_path&command=DIFF_FRAMESET&file=$url_file_tail&rev2=$revision&rev1=$::prev_revision{$revision}\"";
	} else {
	  $output .= "<A HREF=\"cvsblame.cgi?file=$url_filename&rev=$revision&root=$root\"";
	}
	$output .= " onmouseover='return log(event,\"$::prev_revision{$revision}\",\"$revision\");'" if $::use_layers;
        $output .= " onmouseover=\"showMessage('$revision','$line')\" id=\"line_$line\"" if $::use_dom;
        $output .= ">";
	my $author = $::revision_author{$revision};
	$author =~ s/%.*$//;
        $author_width = max($author_width,length($author));
        $output .= sprintf("%-${author_width}s ", $author);
	$output .= "$revision</A> ";
        $output .= ' ' x ($revision_width - length($revision));

        $old_revision = $revision;
        $rev_count = 0;
    } else {
        $output .= '  ' . ' ' x ($author_width + $revision_width);
    }
    $rev_count++;

    $output .= "$text";
    
    # Close the highlighted section
    if (defined $mark_cmd and $mark_cmd ne 'begin') {
        chop($output);
        $output .= "</TD></TR><TR><TD colspan=3$row_color><PRE>";
	$inMark = 0;
    }

    print $output;
}
print "</TD></TR></TABLE>\n";

if ($::use_layers || $::use_dom) {
  # Write out cvs log messages as a JS variables
  # or hidden <div>'s
  print qq|<SCRIPT $::script_type><!--\n| if $::use_layers;
  while (my ($revision, $junk) = each %usedlog) {
    
    # Create a safe variable name for a revision log
    my $revisionName = $revision;
    $revisionName =~ tr/./_/;
    
    my $log = $::revision_log{$revision};
    $log =~ s/([^\n\r]{80})([^\n\r]*)/$1\n$2/g if $::use_layers;
    $log = html_quote($log);
    $log = MarkUpText($log);
    $log =~ s/\n|\r|\r\n/<BR>/g;
    $log =~ s/"/\\"/g if $::use_layers;
    
    # Write JavaScript variable for log entry (e.g. log1_1 = "New File")
    my $author = $::revision_author{$revision};
    $author =~ tr/%/@/;
    my $author_email = EmailFromUsername($author);
    print "<div id=\"rev_$revision\" class=\"log_msg\" style=\"display:none\">" if $::use_dom;
    print "log$revisionName = \"" if $::use_layers;
    print "<b>$revision</b> &lt;<a href='mailto:$author_email'>$author</a>&gt;"
	." <b>$::revision_ctime{$revision}</b><BR>"
	  ."<SPACER TYPE=VERTICAL SIZE=5>$log";
    print "\";\n" if $::use_layers;
    print "</div>\n" if $::use_dom;
  }
  print "//--></SCRIPT>" if $::use_layers;
}

&print_bottom;

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

    $| = 1;

    print "<HTML><HEAD><TITLE>CVS Blame $title_text</TITLE>";

    print <<__TOP__ if $::use_layers;
<SCRIPT $::script_type><!--
var event = 0;	// Nav3.0 compatibility
document.loaded = false;

function finishedLoad() {
    if (parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1) {
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
    if (parseInt(navigator.appVersion) < 4 ||
        navigator.userAgent.toLowerCase().indexOf("msie") != -1) {
        return true;
    }

    var l = document.layers['popup'];
    var guide = document.layers['popup_guide'];

    if (event.target.text.length > max_link_length) {
      max_link_length = event.target.text.length;

      guide.document.write("<PRE>" + event.target.text);
      guide.document.close();

      popup_offset = guide.clip.width;
    }

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
    l.left = event.target.x + popup_offset;

    l.visibility="show";

    return true;
}

file_tail = "$file_tail";
popup_offset = 5;
max_link_length = 0;

initialLayer = "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#F0A000><TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF><B>Page loading...please wait.</B></TD></TR></TABLE></td></tr></table>";

//--></SCRIPT>
</HEAD>
<BODY onLoad="finishedLoad();" BGCOLOR="#FFFFFF" TEXT="#000000" LINK="#0000EE" VLINK="#551A8B" ALINK="#F0A000">
<LAYER SRC="javascript:initialLayer" NAME='popup' onMouseOut="this.visibility='hide';" LEFT=0 TOP=0 BGCOLOR='#FFFFFF' VISIBILITY='hide'></LAYER>
<LAYER SRC="javascript:initialLayer" NAME='popup_guide' onMouseOut="this.visibility='hide';" LEFT=0 TOP=0 VISIBILITY='hide'></LAYER>
__TOP__
    print <<__TOP__ if $::use_dom;
<script $::script_type><!--
var r
function showMessage(rev,line) {
    if (r) {
        r.style.display='none'
    }
    r = document.getElementById('rev_'+rev)
    if (!r)
        return
    var l = document.getElementById('line_'+line)
    var t = l.offsetTop
    var p = l.offsetParent
    while (p.tagName != 'BODY') {
        t = t + p.offsetTop
        p = p.offsetParent
    }
    r.style.top = t
    r.style.left = l.offsetLeft + l.offsetWidth + 20
    r.style.display=''
}

function hideMessage() {
    if (r) {
        r.style.display='none'
    }
}
//--></script>

<style type="text/css">
body {
    background-color: white;
    color: black;
}

a:link {
    color: blue;
}

a:visited {
    color: purple;
}

a:active {
    color: orange;
}

.log_msg {
    border-style: solid;
    border-color: #F0A000;
    background-color: #FFFFFF;
    padding: 5;
    position: absolute;
}

pre {
    margin: 0;
}
</style>
</head>
<body onclick="hideMessage()">
__TOP__
  print '<BODY BGCOLOR="#FFFFFF" TEXT="#000000" LINK="#0000EE" VLINK="#551A8B" ALINK="#F0A000">' if not ($::use_layers || $::use_dom);
} # print_top

sub print_usage {
    my ($linenum_message) = '';
    my ($new_linenum, $src_roots_list);
    my ($title_text) = "Usage";

    if ($ENV{REQUEST_METHOD} eq 'POST' and defined $::FORM{set_line}) {
  
        # Expire the cookie 5 months from now
        my $set_cookie = "Set-Cookie: line_nums=$::FORM{set_line}; expires="
            .&toGMTString(time + 86400 * 152)."; path=/";
	# XXX Hey, nothing is done with this handy cookie string! ### XXX
    }
    if ( not defined $::COOKIE{line_nums} and not defined $::FORM{set_line}) {
        $new_linenum = 'on';
    } elsif ($::COOKIE{line_nums} eq 'off' or $::FORM{set_line} eq 'off') {
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
  <TD>on *</TD>
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
     my $maintainer = Param('maintainer');

     print <<__BOTTOM__;
<HR WIDTH="100%">
<FONT SIZE=-1>
<A HREF="cvsblame.cgi">Page configuration and help</A>.
Mail feedback to <A HREF="mailto:$maintainer?subject=About the cvsblame script">&lt;$maintainer&gt;</A>. 
</FONT></BODY>
</HTML>
__BOTTOM__
} # print_bottom

sub print_raw_data {
  my %revs_seen = ();
  my $prev_rev = $::revision_map[0];
  my $count = 0;
  for my $rev (@::revision_map) {
    if ($prev_rev eq $rev) {
      $count++;
    } else {
      print "$prev_rev:$count\n";
      $count = 1;
      $prev_rev = $rev;
      $revs_seen{$rev} = 1;
    }
  }
  print "$prev_rev:$count\n";
  print "REVISION DETAILS\n";
  for my $rev (sort keys %revs_seen) {
    print "$rev|$::revision_ctime{$rev}|$::revision_author{$rev}|$::revision_log{$rev}.\n";
  }
}

sub link_includes {
    my ($text) = $_[0];

    if ($text =~ /\#(\s*)include(\s*)"(.*?)"/) {
        foreach my $trial_root (($rcs_path, 'ns/include', 
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

my $in_comments = 0;
my $open_delim;
my $close_delim;
my $expected_delim;

sub html_comments_init {
    return 0 unless $use_html;
                                                      
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
    my $newtext = "";
    my $oldtext = $text;
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
        $text = $1 . "<A HREF=mailto:$2?subject=$url_filename>$2</A>" . $3;
    }

    return $text;
}
