#!/usr/bonsaitools/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
# cvsview.cgi - fake up some HTML based on RCS logs and diffs
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

# brendan and fur
#
# TODO in no particular order:
# - Mocha-automate the main page's form so clicking on rev links in the table
#   change the default filename and revisions.
# - Add a tab width input to the main page's form.
# - Include log message in wasted horizontal real-estate of Shortcuts frame.
# - Make old and new diff lines go to separate, side-by-side frames, and use
#   Mocha to slave their scrollbars together.
# - Allow expansion of the top-level table to include the revision histories
#   of all the files in the directory.
# - More more more xdiff/gdiff-like features...
#

#
# SRCROOTS is an array of repository roots under which to look for CVS files.
#

use diagnostics;
use strict;

use CGI;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::TreeInfo;
    $zz = $::TreeList;
    $zz = $::file_description;
    $zz = $::principal_branch;
    $zz = %::timestamp;
}

my $anchor_num = 0;
my $font_tag = "";
# Figure out which directory bonsai is in by looking at argv[0]

my $bonsaidir = $0;
$bonsaidir =~ s:/[^/]*$::;      # Remove last word, and slash before it.
if ($bonsaidir eq '') {
    $bonsaidir = '.';
}

chdir $bonsaidir || die "Can't chdir to $bonsaidir";
require 'CGI.pl';

my $cocommand = Param('cocommand');
my $rcsdiffcommand = Param('rcsdiffcommand');

LoadTreeConfig();

my @SRCROOTS;
NEXTTREE: foreach my $i (@::TreeList) {
    my $r = $::TreeInfo{$i}->{'repository'};
    foreach my $j (@SRCROOTS) {
        if ($r eq $j) {
            next NEXTTREE;
        }
    }
    push @SRCROOTS, $r;
}

my $debug = 0;


my $MAX_REVS = 8;


#
# Make sure both kinds of standard output go to STDOUT.
# XXX dup stdout onto stderr and flush stdout after the following prints
#
# Until then, replace standard die built-in with our own.
#  sub die {
#      print 'fatal error: ';
#      print @_;
#      exit;
#  }


my $line_buffer;

# Consume one token from the already opened RCSFILE filehandle.
# Unescape string tokens, if necessary.
sub get_token {
    # Erase all-whitespace lines.
    while ($line_buffer =~ /^$/) {
        die ('Unexpected EOF') if eof(RCSFILE);
        $line_buffer = <RCSFILE>;
        $line_buffer =~ s/^\s+//; # Erase leading whitespace
    }
    
    # A string of non-whitespace characters is a token ...
    return $1 if ($line_buffer =~ s/^([^;@][^;\s]*)\s*//o);

    # ...and so is a single semicolon ...
    return ';' if ($line_buffer =~ s/^;\s*//o);

    # ...or an RCS-encoded string that starts with an @ character.
    $line_buffer =~ s/^@([^@]*)//o;
    my $token = $1;

    # Detect single @ character used to close RCS-encoded string.
    while ($line_buffer !~ /^@[^@]*$/o) {
        $token .= $line_buffer;
        die ('Unexpected EOF') if eof(RCSFILE);
        $line_buffer = <RCSFILE>;
    }
    
    # Retain the remainder of the line after the terminating @ character.
    ($line_buffer) = ($line_buffer =~ /^@\s*([^@]*)/o);
    
    # Undo escape-coding of @ characters.
    $token =~ s/@@/@/og;
        
    return $token;
}

# Consume a token from RCS filehandle and ensure that it matches
# the given string constant.
sub match_token {
    my ($match) = @_;

    my ($token) = &get_token;
    die ("Unexpected parsing error in RCS file.\n",
          "Expected token: $match, but saw: $token\n")
            if ($token ne $match);
}

# Push RCS token back into the input buffer.
sub unget_token {
    my ($token) = @_;
    $line_buffer = "$token $line_buffer";
}

# Parses "administrative" header of RCS files, setting these globals:
# 
# $::head_revision           -- Revision for which cleartext is stored
# $::principal_branch
# $::file_description
# %::revision_symbolic_name  -- maps from numerical revision # to symbolic tag
# %::tag_revision            -- maps from symbolic tag to numerical revision #
#
sub parse_rcs_admin {
    my ($token, $tag, $tag_name, $tag_revision);
    my (@tags);

    # Undefine variables, because we may have already read another RCS file
    undef %::tag_revision;
    undef %::revision_symbolic_name;

    while (1) {
        # Read initial token at beginning of line
        $token = &get_token();

        # We're done once we reach the description of the RCS tree
        if ($token =~ /^\d/o) {
            &unget_token($token);
            return;
        }
        
#       print "token: $token\n";

        if ($token eq 'head') {
            $::head_revision = &get_token;
            &get_token;         # Eat semicolon
        } elsif ($token eq 'branch') {
            $::principal_branch = &get_token;
            &get_token;         # Eat semicolon
        } elsif ($token eq 'symbols') {

            # Create an associate array that maps from tag name to
            # revision number and vice-versa.
            while (($tag = &get_token) ne ';') {
                ($tag_name, $tag_revision) = split(':', $tag);
                
                $::tag_revision{$tag_name} = $tag_revision;
                $::revision_symbolic_name{$tag_revision} = $tag_name;
            }
        } elsif ($token eq 'comment') {
            $::file_description = &get_token;
            &get_token;         # Eat semicolon

        # Ignore all these other fields - We don't care about them.         
        } elsif (($token eq 'locks')  ||
                 ($token eq 'strict') ||
                 ($token eq 'expand') ||
                 ($token eq 'access')) {
            (1) while (&get_token ne ';');
        } else {
            warn ("Unexpected RCS token: $token\n");
        }
    }

    die('Unexpected EOF');
}

# Construct associative arrays that represent the topology of the RCS tree
# and other arrays that contain info about individual revisions.
#
# The following associative arrays are created, keyed by revision number:
#   %::revision_date     -- e.g. "96.02.23.00.21.52"
#   %::timestamp         -- seconds since 12:00 AM, Jan 1, 1970 GMT
#   %::revision_author   -- e.g. "tom"
#   %::revision_branches -- descendant branch revisions, separated by spaces,
#                         e.g. "1.21.4.1 1.21.2.6.1"
#   %::prev_revision     -- revision number of previous *ancestor* in RCS tree.
#                         Traversal of this array occurs in the direction
#                         of the primordial (1.1) revision.
#   %::prev_delta        -- revision number of previous revision which forms the
#                         basis for the edit commands in this revision.
#                         This causes the tree to be traversed towards the
#                         trunk when on a branch, and towards the latest trunk
#                         revision when on the trunk.
#   %::next_delta        -- revision number of next "delta".  Inverts %::prev_delta.
#
# Also creates %::last_revision, keyed by a branch revision number, which
# indicates the latest revision on a given branch,
#   e.g. $::last_revision{"1.2.8"} == 1.2.8.5
#
sub parse_rcs_tree {
    my($revision, $date, $author, $branches, $next);
    my($branch, $is_trunk_revision);

    # Undefine variables, because we may have already read another RCS file
    undef %::revision_date;
    undef %::timestamp;
    undef %::revision_author;
    undef %::revision_branches;
    undef %::prev_revision;
    undef %::prev_delta;
    undef %::next_delta;
    undef %::last_revision;

    while (1) {
        $revision = &get_token;

        # End of RCS tree description ?
        if ($revision eq 'desc') {
            &unget_token($revision);
            return;
        }

        $is_trunk_revision = ($revision =~ /^[0-9]+\.[0-9]+$/);
        
        $::tag_revision{$revision} = $revision;
        ($branch) = $revision =~ /(.*)\.[0-9]+/o;
        $::last_revision{$branch} = $revision;

        # Parse date
        &match_token('date');
        $date = &get_token;
        $::revision_date{$revision} = $date;
        &match_token(';');
        
        # Convert date into timestamp
#       @date_fields = reverse(split(/\./, $date));
#       $date_fields[4]--;      # Month ranges from 0-11, not 1-12
#       $::timestamp{$revision} = &timegm(@date_fields);

        # Parse author
        &match_token('author');
        $author = &get_token;
        $::revision_author{$revision} = $author;
        &match_token(';');

        # Parse state;
        &match_token('state');
        (1) while (&get_token ne ';');

        # Parse branches
        &match_token('branches');
        $branches = '';
        my $token;
        while (($token = &get_token) ne ';') {
            $::prev_revision{$token} = $revision;
            $::prev_delta{$token} = $revision;
            $branches .= "$token ";
        }
        $::revision_branches{$revision} = $branches;

        # Parse revision of next delta in chain
        &match_token('next');
        $next = '';
        if (($token = &get_token) ne ';') {
            $next = $token;
            &get_token;         # Eat semicolon
            $::next_delta{$revision} = $next;
            $::prev_delta{$next} = $revision;
            if ($is_trunk_revision) {
                $::prev_revision{$revision} = $next;
            } else {
                $::prev_revision{$next} = $revision;
            }
        }

        if ($debug > 1) {
            print "revision = $revision\n";
            print "date     = $date\n";
            print "author   = $author\n";
            print "branches = $branches\n";
            print "next     = $next\n\n";
        }
    }
}

# Reads and parses complete RCS file from already-opened RCSFILE descriptor.
sub parse_rcs_file {
    my ($file) = @_;
    die("Couldn't open $file\n") if !open(RCSFILE, "< $file");
    $line_buffer = '';
    print "Reading RCS admin...\n" if ($debug);
    &parse_rcs_admin();
    print "Reading RCS revision tree topology...\n" if ($debug);
    &parse_rcs_tree();
    print "Done reading RCS file...\n" if ($debug);
    close(RCSFILE);
}

# Map a tag to a numerical revision number.  The tag can be a symbolic
# branch tag, a symbolic revision tag, or an ordinary numerical
# revision number.
sub map_tag_to_revision {
    my($tag_or_revision) = @_;

    my ($revision) = $::tag_revision{$tag_or_revision};
    
    # Is this a branch tag, e.g. xxx.yyy.0.zzz
    if ($revision =~ /(.*)\.0\.([0-9]+)/o) {
        my $branch = $1 . '.' . $2;
        # Return latest revision on the branch, if any.
        return $::last_revision{$branch} if (defined($::last_revision{$branch}));
        return $1;              # No revisions on branch - return branch point
    } else {
        return $revision;
    }
}

#
# Print HTTP content-type header and the header-delimiting extra newline.
#
my $request = new CGI;
print $request->header();

my $request_method = $request->request_method(); # e.g., "GET", "POST", etc.
my $script_name = $ENV{'SCRIPT_NAME'};
my $prefix = $script_name . '?'; # prefix for HREF= entries
$prefix = $script_name . $ENV{PATH_INFO} . '?' if (exists($ENV{PATH_INFO}));


# Parse options in URL.  For example,
# http://w3/cgi/cvsview.pl?subdir=foo&file=bar would assign
#   $opt_subdir = foo and $opt_file = bar.

my $opt_rev1              = $request->param('rev1');
my $opt_rev2              = $request->param('rev2');
my $opt_root              = $request->param('root');
my $opt_files             = $request->param('files');
my $opt_skip              = $request->param('skip') || 0;
my $opt_diff_mode         = $request->param('diff_mode') || 'context';
my $opt_whitespace_mode   = $request->param('whitespace_mode') || 'show';
my $opt_file              = $request->param('file');
my $opt_rev               = $request->param('diff_mode');
my $opt_subdir            = $request->param('subdir');
my $opt_branch            = $request->param('branch');
my $opt_command           = $request->param('command');

if (defined($opt_branch) && $opt_branch eq 'HEAD' ) { $opt_branch = ''; }

# Configuration colors for diff output.

my $stable_bg_color   = 'White';
my $skipping_bg_color = '#c0c0c0';
my $header_bg_color   = 'Orange';
my $change_bg_color   = 'LightBlue';
my $addition_bg_color = 'LightGreen';
my $deletion_bg_color = 'LightGreen';
my $diff_bg_color     = 'White';

# Ensure that necessary arguments are present
die("command not defined in URL\n") if $opt_command eq '';
die("command $opt_command: subdir not defined\n") if $opt_subdir eq '';
if ($opt_command eq 'DIFF'          ||
    $opt_command eq 'DIFF_FRAMESET' ||
    $opt_command eq 'DIFF_LINKS') {
    die("command $opt_command: file not defined in URL\n") if $opt_file eq '';
    die("command $opt_command: rev1 not defined in URL\n") if $opt_rev1 eq '';
    die("command $opt_command: rev2 not defined in URL\n") if $opt_rev2 eq '';

}

# Propagate diff options to created links
$prefix .= "diff_mode=$opt_diff_mode";
$prefix .= "&whitespace_mode=$opt_whitespace_mode";
$prefix .= "&root=$opt_root";

# Create a shorthand for the longest common initial substring of our URL.
my $magic_url = "$prefix&subdir=$opt_subdir";

# Now that we've munged QUERY_STRING into perl variables, set rcsdiff options.
my $rcsdiff = "$rcsdiffcommand -f";
$rcsdiff .= ' -w' if ($opt_whitespace_mode eq 'ignore');

# Handle the "root" argument
#
my $root = $opt_root;
if (defined $root && $root ne '') {
    $root =~ s|/$||;
    if (-d $root) {
        unshift(@SRCROOTS, $root);
    } else {
        print "Error:  Root, $root, is not a directory.<BR>\n";
        print "</BODY></HTML>\n";
        exit;
    }
}

my $found = 0;
my $dir;
foreach $root (@SRCROOTS) {
    $dir = "$root/$opt_subdir";
    if (-d $dir) {
        $found = 1;
        last;
    }
}
if (!$found) {
    print "<FONT SIZE=5><B>Error:</B> $opt_subdir not found in "
        .join(',', @SRCROOTS), "</FONT>\n";
    exit;
}

# Create top-level frameset document.
sub do_diff_frameset {
    chdir($dir);

    print "<TITLE>$opt_file: $opt_rev1 vs. $opt_rev2</TITLE>\n";
    print "<FRAMESET ROWS='*,90' FRAMESPACING=0 BORDER=1>\n";

    print "  <FRAME NAME=diff+$opt_file+$opt_rev1+$opt_rev2 ",
          "         SRC=\"$magic_url&command=DIFF";
    print "&root=$opt_root" if defined($opt_root);
    print "&file=$opt_file&rev1=$opt_rev1&rev2=$opt_rev2\">\n";

    print "  <FRAME SRC=\"$magic_url&command=DIFF_LINKS";
    print "&root=$opt_root" if defined($opt_root);
    print "&file=$opt_file&rev1=$opt_rev1&rev2=$opt_rev2\">\n";
    print "</FRAMESET>\n";
}


# Create links to document created by DIFF command.
sub do_diff_links {

    print qq%
        <HEAD>
        <SCRIPT type="application/x-javascript"><!--
        var anchor = -1;
        function nextAnchor() {
            if (anchor < parent.frames[0].document.anchors.length)
                parent.frames[0].location.hash = ++anchor;
        };
        function prevAnchor() {
            if (anchor > 0)
                parent.frames[0].location.hash = --anchor;
        };
        //--></SCRIPT>
        <TITLE>$opt_file: $opt_rev1 vs. $opt_rev2</TITLE>
        </HEAD>
        <BODY BGCOLOR="#FFFFFF" TEXT="#000000"
            LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
    %;
    CheckHidden("$dir/$opt_file");

    chdir($dir);

    open(RCSDIFF, "$rcsdiff -r$opt_rev1 -r$opt_rev2 $opt_file 2>/dev/null |");

    print '<FORM><TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0><TR VALIGN=TOP>';

    my $diff_base = "cvsview2.cgi";
    my $blame_base = "cvsblame.cgi";

    my $lxr_path = "$opt_subdir/$opt_file";
    my $lxr_link = Fix_LxrLink($lxr_path);
    my $blame_link = "$blame_base?file=$opt_subdir/$opt_file";
    $blame_link .= "&root=$opt_root" if defined($opt_root);
    my $diff_link = "$magic_url&command=DIRECTORY&file=$opt_file&rev1=$opt_rev1&rev2=$opt_rev2";
    $diff_link .= "&root=$opt_root" if defined($opt_root);

    print "<TD NOWRAP ALIGN=LEFT VALIGN=CENTER>";
    print "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>";
    print "<TR><TD NOWRAP ALIGN=RIGHT VALIGN=TOP><A HREF=\"$diff_link\" TARGET=_top><B>diff:</B></A> </TD>";
    print "<TD NOWRAP>Change diff parameters.</TD></TR>\n";
    print "<TR><TD NOWRAP ALIGN=RIGHT VALIGN=TOP><A HREF=\"$blame_link\" TARGET=_top><B>blame:</B></A></TD>";
    print "<TD NOWRAP>Annotate line authors.</TD></TR>\n";
    print "<TR><TD NOWRAP ALIGN=RIGHT VALIGN=TOP><A HREF=\"$lxr_link\" TARGET=_top><B>lxr:</B></A> </TD>";
    print "<TD NOWRAP>Browse source as hypertext.</TD></TR>\n";
    print "</TABLE>";
    print "</TD>";

    print "<TD WIDTH=8</TD>";

    print "<TD>";
    print "<INPUT TYPE=button VALUE='Prev' ONCLICK='prevAnchor()'><BR>";
    print "<INPUT TYPE=button VALUE='Next' ONCLICK='nextAnchor()'>";
    print "</TD>";
    print "<TD WIDTH=8></TD>";

    print "<TD><CODE>";

    $anchor_num = 0;
    while (<RCSDIFF>) {
        # Get one command from the diff file
        my $line = "";
        if (/^(c|a)(\d+)/) {
            $line = $2;
            while (<RCSDIFF>) {
                last if /^\.$/;
            }
        } elsif (/^d(\d+)/) {
            $line = $1;
        } else {
            print "<FONT SIZE=5 COLOR=#ffffff><B>Internal error:</B>",
                  " unknown command $_",
                  " at $. in $opt_file $opt_rev1\n";
        }

        print '&nbsp' x (4 - length($line));
        print "<A TARGET='diff+$opt_file+$opt_rev1+$opt_rev2'",
              "       HREF=\"$magic_url&command=DIFF";
        print "&root=$opt_root" if defined($opt_root);
        print "&file=$opt_file&rev1=$opt_rev1&rev2=$opt_rev2#$anchor_num\"",
              "       ONCLICK='anchor = $anchor_num'>$line</A> ";
        $anchor_num++;
    }
    close(RCSDIFF);

    print '</TD></TR></TABLE>';
    print "</FORM></BODY>\n";
}


# Default tab width, although it's frequently 4.
my $tab_width = 8;

sub next_tab_stop {
    my ($pos) = @_;

    return int(($pos + $tab_width) / $tab_width) * $tab_width;
}

#
# Look for the magic emacs tab width comment, or for long lines with more
# than 4 leading tabs in more than 50% of the lines that start with a tab.
# In the latter case, set $tab_width to 4.
#
sub guess_tab_width {
    my ($opt_file) = @_;
    my ($found_tab_width) = 0;
    my ($many_tabs, $any_tabs) = (0, 0);

    open(RCSFILE, "$opt_file");
    while (<RCSFILE>) {
        if (/tab-width: (\d)/) {
            $tab_width = $1;
            $found_tab_width = 1;
            last;
        }
        if (/^(\t+)/) {
            $many_tabs++ if (length($1) >= 4);
            $any_tabs++;
        }
    }
    if (!$found_tab_width && $many_tabs > $any_tabs / 2) {
        $tab_width = 4;
    }
    close(RCSFILE);
}

# Create gdiff-like output.
sub do_diff {

    print "<HTML><HEAD>";
    print "<TITLE>$opt_file: $opt_rev1 vs. $opt_rev2</TITLE>\n";
    print "</HEAD>";
    print "<BODY BGCOLOR=\"$diff_bg_color\" TEXT=\"#000000\"";
    print " LINK=\"#0000EE\" VLINK=\"#551A8B\" ALINK=\"#FF0000\">";

    CheckHidden("$dir/$opt_file");

    chdir($dir);

    my ($rcsfile) = "$opt_file,v";
    $rcsfile = "Attic/$opt_file,v" if (! -r $rcsfile);
    &guess_tab_width($rcsfile);

    &html_diff($rcsfile, $opt_rev1, $opt_rev2);
    print "\n</BODY>\n";
}


# Show specified CVS log entry.
sub do_log {
    print "<TITLE>$opt_file: $opt_rev CVS log entry</TITLE>\n";
    print '<PRE>';

    CheckHidden("$dir/$opt_file");

    chdir($dir);

    open(RCSLOG, "rlog -r$opt_rev $opt_file |");

    while (<RCSLOG>) {
        last if (/^revision $opt_rev$/);
    }

    while (<RCSLOG>) {
        last if (/^===============================================/);
        print "$_<BR>";
    }
    close(RCSLOG);

    print '</PRE>';
}


#
# Main script: generate a table of revision diff and log message hotlinks
# for each modified file in $opt_subdir, and a form for choosing a file and any
# two of its revisions.
#
sub do_directory {

    my $output = "<DIV ALIGN=LEFT>";
    my $link_path = "";

    foreach my $path (split('/',$opt_subdir)) {
        $link_path .= $path;
        $output .= "<A HREF='rview.cgi?dir=$link_path";
        $output .= "&cvsroot=$opt_root" if defined $opt_root;
        $output .= "&rev=$opt_branch" if $opt_branch;
        $output .= "' onmouseover='window.status=\"Browse $link_path\";"
            ." return true;'>$path</A>/ ";
        $link_path .= '/';
    }
    chop ($output);

    if ($opt_branch) {
        $output .= "<BR>Branch: $opt_branch";
    }
    $output .= "</DIV>";

    PutsHeader("CVS Differences", $output);

    CheckHidden($dir);
    chdir($dir);

    print "<TABLE BORDER CELLPADDING=2>\n";

    foreach my $file (split(/\+/, $opt_files)) {
        my ($path) = "$dir/$file,v";

        CheckHidden($path);
        $path = "$dir/Attic/$file,v" if (! -r $path);
        &parse_rcs_file($path);

        my $lxr_path = "$opt_subdir/$file";
        my $lxr_link = Fix_LxrLink($lxr_path);

        print "<TR><TD NOWRAP><B>";
        print "<A HREF=\"$lxr_link\">$file</A><BR>";
        print "<A HREF=\"cvslog.cgi?file=$opt_subdir/$file";
        print "&rev=$opt_branch" if $opt_branch;
        print "&root=$opt_root" if defined($opt_root);
        print "\">Change Log</A></B></TD>\n";
        
        my $first_rev;
        if ($opt_branch) {
            $first_rev = &map_tag_to_revision($opt_branch);
            die("$0: error: -r: No such revision: $opt_branch\n")
                if ($first_rev eq '');
        } else {
            $first_rev = $::head_revision;
        }
        
        my $skip = $opt_skip;
        my $revs_remaining = $MAX_REVS;
        my $prev;
        for (my $rev = $first_rev; $rev; $rev = $prev) {
            $prev = $::prev_revision{$rev};
            next if $skip-- > 0;
            if (!$revs_remaining--) {
                #print '<TD ROWSPAN=2 VALIGN=TOP>';
                print '<TD VALIGN=TOP>';
                print "<A HREF=\"$magic_url&command=DIRECTORY";
                print "&root=$opt_root" if defined($opt_root);
                print "&files=$opt_files&branch=$opt_branch&skip=", $opt_skip + $MAX_REVS, "\"><i>Prior revisions</i></A>", "</TD>\n";
                last;
            }

            my $href_open = "";
            my $href_close = "";
            if ( $prev && $rev ) {
                $href_open = "<A HREF=\"$magic_url&command=DIFF_FRAMESET";
                $href_open .= "&root=$opt_root" if defined($opt_root);
                $href_open .= "&file=$file&rev1=$prev&rev2=$rev\">";
                $href_close = "</A>";
            }
            print "<TD>$href_open$rev$href_close<BR>";
            print "$::revision_author{$rev}</TD>";
        }

        print "</TR>\n";

        if (0) {
        print "<TR>\n";
        $skip = $opt_skip;
        $revs_remaining = $MAX_REVS;
        for (my $rev = $first_rev; $rev; $rev = $::prev_revision{$rev}) {
            next if $skip-- > 0;
            last if !$revs_remaining--;
            print "<TD><A HREF=\"$magic_url&command=LOG";
            print "root=$opt_root" if defined($opt_root);
            print "&file=$file&rev=$rev\">$::revision_author{$rev}</A>",
            "</TD>\n";
        }
        print "</TR>\n";}
    }

    print "</TABLE><SPACER TYPE=VERTICAL SIZE=20>\n";
    print '<FORM METHOD=get>';
    print '<INPUT TYPE=hidden NAME=command VALUE=DIFF>';
    print "<INPUT TYPE=hidden NAME=subdir VALUE=$opt_subdir>";
    print '<FONT SIZE=+1><B>New Query:</B></FONT>';
    print '<UL><TABLE BORDER=1 CELLSPACING=0 CELLPADDING=7><TR><TD>';


    # pick something remotely sensible to put in the "Filename" field.
    my $file = $opt_file;
    if ( !$file && $opt_files ) {
        $file = $opt_files;
        $file =~ s@\+.*@@;
    }

    print "\n<TABLE CELLPADDING=0 CELLSPACING=0><TR><TD>\n",
          'Filename:',
          '</TD><TD>',
          '<INPUT TYPE=text NAME=file VALUE="', $file, '" SIZE=40>',
          "\n</TD></TR><TR><TD>\n",

          'Old version:',
          '</TD><TD>',
          '<INPUT TYPE=text NAME=rev1 VALUE="', $opt_rev1, '" SIZE=20>',
          "\n</TD></TR><TR><TD>\n",

          'New version:',
          '</TD><TD>',
          '<INPUT TYPE=text NAME=rev2 VALUE="', $opt_rev2, '" SIZE=20>',
          "\n</TD></TR></TABLE>\n";
    print '<TABLE BORDER=0 CELLPADDING=5 WIDTH="100%"><TR><TD>',
          '<INPUT TYPE=radio NAME=whitespace_mode VALUE="show" CHECKED>',
          ' Show Whitespace',
          '<BR><INPUT TYPE=radio NAME=whitespace_mode VALUE="ignore">',
          ' Ignore Whitespace',
          '</TD><TD>',
          '<INPUT TYPE=radio NAME=diff_mode VALUE="context" CHECKED>',
          ' Context Diffs',
          '<BR><INPUT TYPE=radio NAME=diff_mode VALUE="full">',
          ' Full Source Diffs';
    print '</TD></TR></TABLE>';
    print "<INPUT TYPE=submit>\n";
    print '</TD></TR></TABLE></UL>';
    print "</FORM>\n";

    &print_bottom;
}

#
# This function generates a gdiff-style, side-by-side display using HTML.
# It requires two arguments, each of which must be an open filehandle.
# The first filehandle, DIFF, must be a `diff -f` style output containing
# commands to convert the contents of the second filehandle, OLDREV, into
# a later version of OLDREV's file.
#
sub html_diff {
    my ($file, $rev1, $rev2) = @_;
    my ($old_line_num) = 1;
    my ($old_line);
    my ($point, $mark); 

    open(DIFF, "$rcsdiff -f -r$rev1 -r$rev2 $file 2>/dev/null |");
    open(OLDREV, "$cocommand -p$rev1 $file 2>/dev/null |");

    $anchor_num = 0;

    if ($ENV{'HTTP_USER_AGENT'} =~ /Win/) {
        $font_tag = "<PRE><FONT FACE='Lucida Console' SIZE=-1>";
    } else {
        # We don't want your stinking Windows font
        $font_tag = "<PRE>";
    }
    print "<TABLE BGCOLOR=$stable_bg_color "
        .'CELLPADDING=0 CELLSPACING=0 WIDTH="100%" COLS=2>';
    print "<TR BGCOLOR=$header_bg_color><TH>Version $rev1<TH>Version $rev2</TR>";
    while (<DIFF>) {
        $mark = 0;
        if (/^a(\d+)/) {
            $point = $1;
            $old_line_num = skip_to_line($point + 1, $old_line_num);
            while (<DIFF>) {
                last if (/^\.$/);
                &print_row('', $stable_bg_color, $_, $addition_bg_color);
            }
        } elsif ((($point, $mark) = /^c(\d+) (\d+)$/) ||
                 (($point) = /^c(\d+)$/)) {
            $mark = $point if (!$mark);
            $old_line_num = skip_to_line($point, $old_line_num);
            while (<DIFF>) {
                last if (/^\.$/);
                if ($old_line_num <= $mark) {
                    $old_line = <OLDREV>;
                    $old_line_num++;
                } else {
                    $old_line = ''
                }
                &print_row($old_line, $change_bg_color, $_, $change_bg_color);
            }
            while ($old_line_num <= $mark) {
                $old_line = <OLDREV>;
                $old_line_num++;
                &print_row($old_line, $change_bg_color, '', $change_bg_color);
            }
        } elsif ((($point, $mark) = /^d(\d+) (\d+)$/) ||
                 (($point) = /^d(\d+)$/)) {
            $mark = $point if (!$mark);
            $old_line_num = skip_to_line($point, $old_line_num);
            while (1) {
                $old_line = <OLDREV>;
                last unless defined $old_line;
                $old_line_num++;
                &print_row($old_line, $deletion_bg_color, '', $stable_bg_color);
                last if ($. == $mark);
            }
        } else {
            print "</TABLE><FONT SIZE=5 COLOR=#ffffff><B>Internal error:</B>",
                  " unknown command $_",
                  " at $. in $opt_file $opt_rev1\n";
            exit;
        }
    }

    #
    # Print the remaining lines in the original file.  These are lines that
    # were not modified in the later revision
    #
    my ($base_old_line_num) = $old_line_num;
    while (1) {
        $old_line = <OLDREV>;
        last unless defined $old_line;
        $old_line_num++;
        &print_row($old_line, $stable_bg_color, $old_line, $stable_bg_color)
            if ($opt_diff_mode eq 'full' ||
                $old_line_num <= $base_old_line_num + 5);
    }

#    print "</FONT></PRE>\n";
    print "</TABLE></FONT>\n";

    &print_bottom;

    close(OLDREV);
    close(DIFF);
}

sub skip_to_line {
    my ($line_num, $old_line_num);
    ($line_num, $old_line_num) = @_;
    my ($anchor_printed) = 0;
    my ($skip_line_printed) = ($line_num - $old_line_num <= 10);
    my ($base_old_line_num) = $old_line_num;

    while ($old_line_num < $line_num) {
        if (!$anchor_printed && $old_line_num >= $line_num - 10) {
            print "<A NAME=$anchor_num>";
            $anchor_printed = 1;
        }

        if ($opt_diff_mode eq 'context' && !$skip_line_printed &&
            $line_num - 5 <= $old_line_num) {
            print "</TABLE>";
            print "<TABLE BGCOLOR=$stable_bg_color "
                .'CELLPADDING=0 CELLSPACING=0 WIDTH="100%" COLS=2>';
            print "<TR BGCOLOR=$skipping_bg_color><TD>",
                 "<B>Skipping to line $old_line_num:</B><TD>&nbsp;";
            $skip_line_printed = 1;
        }

        my $old_line = <OLDREV>;
        $old_line_num++;

        &print_row($old_line, $stable_bg_color, $old_line, $stable_bg_color)
            if ($opt_diff_mode eq 'full' ||
                $old_line_num <= $base_old_line_num + 5 ||
                $line_num - 5 < $old_line_num);
    }

    print "<A NAME=$anchor_num>" if (!$anchor_printed);
    print '</A>';
    $anchor_num++;
    return $old_line_num;
}

sub print_cell {
    my ($line, $color) = @_;
    my ($i, $j, $k, $n);
    my ($c, $newline);

    if ($color eq $stable_bg_color) {
        print "<TD>$font_tag";
    } else {
        print "<TD BGCOLOR=$color>$font_tag";
    }

    chomp $line;
    $n = length($line);
    $newline = '';
    for ($i = $j = 0; $i < $n; $i++) {
        $c = substr($line, $i, 1);
        if ($c eq "\t") {
            for ($k = &next_tab_stop($j); $j < $k; $j++) {
                $newline .= ' ';
            }
        } else {
            $newline .= $c;
            $j++;
        }
    }
    $newline =~ s/\s+$//;
    if (length($newline) <= 80) {
        $newline = sprintf("%-80.80s", $newline);
    } else {
        $newline =~ s/([^\n\r]{80})([^\n\r]*)/$1\n$2/g;
    }
    $newline =~ s/&/&amp;/g;
    $newline =~ s/</&lt;/g;
    $newline =~ s/>/&gt;/g;
    print $newline;
}

sub print_row {
    my ($line1, $color1, $line2, $color2) = @_;
    print "<TR>";
    $line1 = "" unless defined $line1;
    $line2 = "" unless defined $line2;
    &print_cell($line1, $color1);
    &print_cell($line2, $color2);
}

sub print_bottom {
     my $maintainer = Param('maintainer');

     print <<__BOTTOM__;
<P>
<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=0><TR><TD>
<HR>
<TR><TD>
<FONT SIZE=-1>
&nbsp;&nbsp;Mail feedback and feature requests to <A HREF="mailto:$maintainer?subject=About the cvs differences script">$maintainer</A>.&nbsp;&nbsp; 
</TABLE>
</BODY>
</HTML>
__BOTTOM__
} # print_bottom


sub do_cmd {
    if ($opt_command eq 'DIFF_FRAMESET') { do_diff_frameset; }
    elsif ($opt_command eq 'DIFF_LINKS') { do_diff_links; }
    elsif ($opt_command eq 'DIFF') { do_diff; }
    elsif ($opt_command eq 'LOG') { do_log; }
    elsif ($opt_command eq 'DIRECTORY') { do_directory; }
    else { print "invalid command \"$opt_command\"."; }
    exit;
}

do_cmd;
