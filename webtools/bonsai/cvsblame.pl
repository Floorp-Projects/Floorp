#!/usr/bonsaitools/bin/perl --
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

##############################################################################
#
# cvsblame.pl - Shamelessly adapted from Scott Furman's cvsblame script
#                 by Steve Lamm (slamm@netscape.com)
#             - Annotate each line of a CVS file with its author,
#               revision #, date, etc.
# 
# Report problems to Steve Lamm (slamm@netscape.com)
#
##############################################################################

use strict;

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub cvsblame_pl_sillyness {
    my $zz;
    $zz = $::file_description;
    $zz = $::opt_A;
    $zz = $::opt_d;
    $zz = $::principal_branch;
    $zz = %::lines_added;
    $zz = %::lines_removed;
};

use Time::Local qw(timegm);   # timestamps
use Date::Format;             # human-readable dates
use File::Basename;           # splits up paths into path, filename, suffix

my $debug = 0;
$::opt_m = 0 unless (defined($::opt_m));

# Extract base part of this script's name
($::progname = $0) =~ /([^\/]+)$/;

&cvsblame_init;

my $SECS_PER_DAY;
my $time;

sub cvsblame_init {
    # Use default formatting options if none supplied
    if (!$::opt_A && !$::opt_a && !$::opt_d && !$::opt_v) {
        $::opt_a = 1;
        $::opt_v = 1;
    }

    $time = time;
    $SECS_PER_DAY = 60 * 60 * 24;

    # Timestamp threshold at which annotations begin to occur, if -m option present.
    $::opt_m_timestamp = $time;
    if (defined $::opt_m) {
        $::opt_m_timestamp -= $::opt_m * $SECS_PER_DAY;
    }
}

# Generic traversal of a CVS tree.  Invoke callback function for
# individual directories that contain CVS files.
sub traverse_cvs_tree {
    my ($dir, $nlink);
    local *callback;
    ($dir, *callback, $nlink) = @_;
    my ($dev, $ino, $mode, $subcount);

    # Get $nlink for top-level directory
    ($dev, $ino, $mode, $nlink) = stat($dir) unless $nlink;

    # Read directory
    opendir(DIR, $dir) || die "Can't open $dir\n";
    my (@filenames) = readdir(DIR);
    closedir(DIR);

    return if ! -d "$dir/CVS";

    &callback($dir);

    # This dir has subdirs
    if ($nlink != 2) {
        $subcount = $nlink - 2; # Number of subdirectories
        for (@filenames) {
            last if $subcount == 0;
            next if $_ eq '.';
            next if $_ eq '..';
            next if $_ eq 'CVS';
            my $name = "$dir/$_";

            ($dev, $ino, $mode, $nlink) = lstat($name);
            next unless -d _;
            if (-x _ && -r _) {
                print STDERR "$::progname: Entering $name\n";
                &traverse_cvs_tree($name, *callback, $nlink);
            } else {
                warn("Couldn't chdir to $name");
            }
            --$subcount;
        }
    }
}

# Consume one token from the already opened RCSFILE filehandle.
# Unescape string tokens, if necessary.

my $line_buffer;

sub get_token {
    # Erase all-whitespace lines.
    $line_buffer = '' unless (defined($line_buffer));
    while ($line_buffer =~ /^$/) {
        die ("Unexpected EOF") if eof(RCSFILE);
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
    while ($line_buffer !~ /@/o || # Short-circuit optimization
           $line_buffer !~ /([^@]|^)@([^@]|$)/o) {
        $token .= $line_buffer;
        die ("Unexpected EOF") if eof(RCSFILE);
        $line_buffer = <RCSFILE>;
    }
           
    # Retain the remainder of the line after the terminating @ character.
    my $i = rindex($line_buffer, '@');
    $token .= substr($line_buffer, 0, $i);
    $line_buffer = substr($line_buffer, $i + 1);

    # Undo escape-coding of @ characters.
    $token =~ s/@@/@/og;
        
    # Digest any extra blank lines.
    while (($line_buffer =~ /^$/) && !eof(RCSFILE)) {
	$line_buffer = <RCSFILE>;
    }
    return $token;
}

my $rcs_pathname;

# Consume a token from RCS filehandle and ensure that it matches
# the given string constant.
sub match_token {
    my ($match) = @_;

    my ($token) = &get_token;
    die "Unexpected parsing error in RCS file $rcs_pathname.\n",
        "Expected token: $match, but saw: $token\n"
            if ($token ne $match);
}

# Push RCS token back into the input buffer.
sub unget_token {
    my ($token) = @_;
    $line_buffer = $token . " " . $line_buffer;
}

# Parses "administrative" header of RCS files, setting these globals:
# 
# $::head_revision          -- Revision for which cleartext is stored
# $::principal_branch
# $::file_description
# %::revision_symbolic_name -- maps from numerical revision # to symbolic tag
# %::tag_revision           -- maps from symbolic tag to numerical revision #
#
sub parse_rcs_admin {
    my ($token, $tag, $tag_name, $tag_revision);

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

        if ($token eq "head") {
            $::head_revision = &get_token;
            &get_token;         # Eat semicolon
        } elsif ($token eq "branch") {
            $::principal_branch = &get_token;
            &get_token;         # Eat semicolon
        } elsif ($token eq "symbols") {

            # Create an associate array that maps from tag name to
            # revision number and vice-versa.
            while (($tag = &get_token) ne ';') {
                ($tag_name, $tag_revision) = split(':', $tag);
                
                $::tag_revision{$tag_name} = $tag_revision;
                $::revision_symbolic_name{$tag_revision} = $tag_name;
            }
        } elsif ($token eq "comment") {
            $::file_description = &get_token;
            &get_token;         # Eat semicolon

        # Ignore all these other fields - We don't care about them.         
        } elsif (($token eq "locks")  ||
                 ($token eq "strict") ||
                 ($token eq "expand") ||
                 ($token eq "access")) {
            (1) while (&get_token ne ';');
        } else {
            warn ("Unexpected RCS token: $token\n");
        }
    }

    die "Unexpected EOF";
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
#   %::prev_delta        -- revision number of previous revision which forms
#                         the basis for the edit commands in this revision.
#                         This causes the tree to be traversed towards the
#                         trunk when on a branch, and towards the latest trunk
#                         revision when on the trunk.
#   %::next_delta   -- revision number of next "delta".  Inverts %::prev_delta.
#
# Also creates %::last_revision, keyed by a branch revision number, which
# indicates the latest revision on a given branch,
#   e.g. $::last_revision{"1.2.8"} == 1.2.8.5
#


my %revision_age;

sub parse_rcs_tree {
    my ($revision, $date, $author, $branches, $next);
    my ($branch, $is_trunk_revision);

    # Undefine variables, because we may have already read another RCS file
    undef %::timestamp;
    undef %revision_age;
    undef %::revision_author;
    undef %::revision_branches;
    undef %::revision_ctime;
    undef %::revision_date;
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
        my @date_fields = reverse(split(/\./, $date));
        $date_fields[4]--;      # Month ranges from 0-11, not 1-12
        $::timestamp{$revision} = timegm(@date_fields);

        # Pretty print the date string
        my @ltime = localtime($::timestamp{$revision});
        my $formated_date = strftime("%Y-%m-%d %H:%M", @ltime);
        $::revision_ctime{$revision} = $formated_date;

        # Save age
        $revision_age{$revision} =
            ($time - $::timestamp{$revision}) / $SECS_PER_DAY;

        # Parse author
        &match_token('author');
        $author = &get_token;
        $::revision_author{$revision} = $author;
        &match_token(';');

        # Parse state;
        &match_token('state');
        while (&get_token ne ';') { }

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

        if ($debug >= 3) {
            print "<pre>revision = $revision\n";
            print "date     = $date\n";
            print "author   = $author\n";
            print "branches = $branches\n";
            print "next     = $next</pre>\n\n";
        }
    }
}

sub parse_rcs_description {
    &match_token('desc');
    my $rcs_file_description = &get_token;
}

# Construct associative arrays containing info about individual revisions.
#
# The following associative arrays are created, keyed by revision number:
#   %::revision_log        -- log message
#   %::revision_deltatext  -- Either the complete text of the revision,
#                           in the case of the head revision, or the
#                           encoded delta between this revision and another.
#                           The delta is either with respect to the successor
#                           revision if this revision is on the trunk or
#                           relative to its immediate predecessor if this
#                           revision is on a branch.
sub parse_rcs_deltatext {
    undef %::revision_log;
    undef %::revision_deltatext;

    while (!eof(RCSFILE)) {
        my $revision = &get_token;
        print "Reading delta for revision: $revision\n" if ($debug >= 3);
        &match_token('log');
        $::revision_log{$revision} = &get_token;
        &match_token('text');
        $::revision_deltatext{$revision} = &get_token;
    }
}


# Reads and parses complete RCS file from already-opened RCSFILE descriptor.
# Or if a parameter is given use the corresponding file
sub parse_rcs_file {
    my $path = shift;
    if (defined $path) {
    	open (RCSFILE, $path);
    }	
    print "Reading RCS admin...\n" if ($debug >= 2);
    &parse_rcs_admin();
    print "Reading RCS revision tree topology...\n" if ($debug >= 2);
    &parse_rcs_tree();

    if( $debug >= 3 ){
        print "<pre>Keys:\n\n";
        for my $i (keys %::tag_revision ){
            my $k = $::tag_revision{$i};
            print "yoyuo $i: $k\n";            
        }
        print "</pre>\n";
    }

    &parse_rcs_description();
    print "Reading RCS revision deltas...\n" if ($debug >= 2);
    &parse_rcs_deltatext();
    print "Done reading RCS file...\n" if ($debug >= 2);
    close RCSFILE if (defined $path);
}

# Map a tag to a numerical revision number.  The tag can be a symbolic
# branch tag, a symbolic revision tag, or an ordinary numerical
# revision number.
sub map_tag_to_revision {
    my ($tag_or_revision) = @_;

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

# Construct an ordered list of ancestor revisions to the given
# revision, starting with the immediate ancestor and going back
# to the primordial revision (1.1).
#
# Note: The generated path does not traverse the tree the same way
#       that the individual revision deltas do.  In particular,
#       the path traverses the tree "backwards" on branches.

sub ancestor_revisions {
    my ($revision) = @_;
    my (@ancestors);
     
    $revision = $::prev_revision{$revision};
    while ($revision) {
        push(@ancestors, $revision);
        $revision = $::prev_revision{$revision};
    }

    return @ancestors;
}

# Extract the given revision from the digested RCS file.
# (Essentially the equivalent of cvs up -rXXX)
sub extract_revision {
    my ($revision) = @_;
    my (@path);
    my $add_lines_remaining = 0;
    my ($start_line, $count);
    # Compute path through tree of revision deltas to most recent trunk revision
    while ($revision) {
        push(@path, $revision);
        $revision = $::prev_delta{$revision};
    }
    @path = reverse(@path);
    shift @path;                # Get rid of head revision

    # Get complete contents of head revision
    my (@text) = split(/^/, $::revision_deltatext{$::head_revision});

    # Iterate, applying deltas to previous revision
    foreach $revision (@path) {
        my $adjust = 0;
        my @diffs = split(/^/, $::revision_deltatext{$revision});

        my ($lines_added) = 0;
        my ($lines_removed) = 0;

        foreach my $command (@diffs) {
            if ($add_lines_remaining > 0) {
                # Insertion lines from a prior "a" command.
                splice(@text, $start_line + $adjust,
                       0, $command);
                $add_lines_remaining--;
                $adjust++;
            } elsif ($command =~ /^d(\d+)\s(\d+)/) {
                # "d" - Delete command
                ($start_line, $count) = ($1, $2);
                splice(@text, $start_line + $adjust - 1, $count);
                $adjust -= $count;
                $lines_removed += $count;
            } elsif ($command =~ /^a(\d+)\s(\d+)/) {
                # "a" - Add command
                ($start_line, $count) = ($1, $2);
                $add_lines_remaining = $count;
                $lines_added += $lines_added;
            } else {
                die "Error parsing diff commands";
            }
        }
        $::lines_removed{$revision} += $lines_removed;
        $::lines_added{$revision} += $lines_added;
    }

    return @text;
}

sub parse_cvs_file {
    ($rcs_pathname) = @_;

    # Args in:  $::opt_rev - requested revision
    #           $::opt_m - time since modified
    # Args out: @::revision_map
    #           %::timestamp
    #           (%::revision_deltatext)

    my @diffs;
    my $revision;
    my $skip;
    my ($start_line, $count);
    my @temp;

    @::revision_map = ();
    CheckHidden($rcs_pathname);

    if (!open(RCSFILE, $rcs_pathname)) {
        my ($name, $path, $suffix) = fileparse($rcs_pathname);
        my $deleted_pathname = "${path}Attic/$name$suffix";
        die "$::progname: error: This file appeared to be under CVS control, " .
            "but the RCS file is inaccessible.\n" .
            "(Couldn't open '$rcs_pathname' or '$deleted_pathname').\n"
          if !open(RCSFILE, $deleted_pathname);
    }
    &parse_rcs_file();
    close(RCSFILE);

    if (!defined($::opt_rev) || $::opt_rev eq '' || $::opt_rev eq 'HEAD') {
        # Explicitly specified topmost revision in tree
        $revision = $::head_revision;
    } else {
        # Symbolic tag or specific revision number specified.
        $revision = &map_tag_to_revision($::opt_rev);
        die "$::progname: error: -r: No such revision: $::opt_rev\n"
            if ($revision eq '');
    }

    # The primordial revision is not always 1.1!  Go find it.
    my $primordial = $revision;
    while (exists($::prev_revision{$primordial}) &&
           $::prev_revision{$primordial} ne "") {
        $primordial = $::prev_revision{$primordial};
    }

    # Don't display file at all, if -m option is specified and no
    # changes have been made in the specified file.
    if ($::opt_m && $::timestamp{$revision} < $::opt_m_timestamp) {
        return '';
    }

    # Figure out how many lines were in the primordial, i.e. version 1.1,
    # check-in by moving backward in time from the head revision to the
    # first revision.
    my $line_count = 0;
    if (exists ($::revision_deltatext{$::head_revision}) && 
        $::revision_deltatext{$::head_revision}) {
         my @tmp_array =  split(/^/, $::revision_deltatext{$::head_revision});
         $line_count = @tmp_array;
    }
    $skip = 0 unless (defined($skip));
    my $rev;
    for ($rev = $::prev_revision{$::head_revision}; $rev;
         $rev = $::prev_revision{$rev}) {
        @diffs = split(/^/, $::revision_deltatext{$rev});
        foreach my $command (@diffs) {
            if ($skip > 0) {
                # Skip insertion lines from a prior "a" command.
                $skip--;
            } elsif ($command =~ /^d(\d+)\s(\d+)/) {
                # "d" - Delete command
                ($start_line, $count) = ($1, $2);
                $line_count -= $count;
            } elsif ($command =~ /^a(\d+)\s(\d+)/) {
                # "a" - Add command
                ($start_line, $count) = ($1, $2);
                $skip = $count;
                $line_count += $count;
            } else {
                die "$::progname: error: illegal RCS file $rcs_pathname\n",
                "  error appears in revision $rev\n";
            }
        }
    }

    # Now, play the delta edit commands *backwards* from the primordial
    # revision forward, but rather than applying the deltas to the text of
    # each revision, apply the changes to an array of revision numbers.
    # This creates a "revision map" -- an array where each element
    # represents a line of text in the given revision but contains only
    # the revision number in which the line was introduced rather than
    # the line text itself.
    #
    # Note: These are backward deltas for revisions on the trunk and
    # forward deltas for branch revisions.

    # Create initial revision map for primordial version.
    while ($line_count--) {
        push(@::revision_map, $primordial);
    }

    my @ancestors = &ancestor_revisions($revision);
    unshift (@ancestors, $revision); # 
    pop @ancestors;             # Remove "1.1"
    $::last_revision = $primordial;
    foreach $revision (reverse @ancestors) {
        my $is_trunk_revision = ($revision =~ /^[0-9]+\.[0-9]+$/);

        if ($is_trunk_revision) {
            @diffs = split(/^/, $::revision_deltatext{$::last_revision});

            # Revisions on the trunk specify deltas that transform a
            # revision into an earlier revision, so invert the translation
            # of the 'diff' commands.
            foreach my $command (@diffs) {
                if ($skip > 0) {
                    $skip--;
                } else {
                    if ($command =~ /^d(\d+)\s(\d+)$/) { # Delete command
                        ($start_line, $count) = ($1, $2);

                        $#temp = -1;
                        while ($count--) {
                            push(@temp, $revision);
                        }
                        splice(@::revision_map, $start_line - 1, 0, @temp);
                    } elsif ($command =~ /^a(\d+)\s(\d+)$/) { # Add command
                        ($start_line, $count) = ($1, $2);
                        splice(@::revision_map, $start_line, $count);
                        $skip = $count;
                    } else {
                        die "Error parsing diff commands";
                    }
                }
            }
        } else {
            # Revisions on a branch are arranged backwards from those on
            # the trunk.  They specify deltas that transform a revision
            # into a later revision.
            my $adjust = 0;
            @diffs = split(/^/, $::revision_deltatext{$revision});
            foreach my $command (@diffs) {
                if ($skip > 0) {
                    $skip--;
                } else {
                    if ($command =~ /^d(\d+)\s(\d+)$/) { # Delete command
                        ($start_line, $count) = ($1, $2);
                        splice(@::revision_map, $start_line + $adjust - 1, $count);
                        $adjust -= $count;
                    } elsif ($command =~ /^a(\d+)\s(\d+)$/) { # Add command
                        ($start_line, $count) = ($1, $2);

                        $skip = $count;
                        $#temp = -1;
                        while ($count--) {
                            push(@temp, $revision);
                        }
                        splice(@::revision_map, $start_line + $adjust, 0, @temp);
                        $adjust += $skip;
                    } else {
                        die "Error parsing diff commands";
                    }
                }
            }
        }
        $::last_revision = $revision;
    }    
    return $revision;
}

1;

__END__
#
# The following are parts of the original cvsblame script that are not
#   used for cvsblame.pl
#

# Read CVS/Entries and CVS/Repository files.
#
# Creates these associative arrays, keyed by the CVS file pathname
#
#  %cvs_revision         -- Revision # present in working directory
#  %cvs_date
#  %cvs_sticky_revision  -- Sticky tag, if any
#  
#  Also, creates %cvs_files, keyed by the directory path, which contains
#  a space-separated list of the files under CVS control in the directory
sub read_cvs_entries
{
    my ($directory) = @_;
    my ($filename, $rev, $date, $idunno, $sticky, $pathname);

    $cvsdir = $directory . '/CVS';

    CheckHidden($cvsdir);

    return if (! -d $cvsdir);

    return if !open(ENTRIES, "$cvsdir/Entries");
    
    while(<ENTRIES>) {
        chop;
        ($filename, $rev, $date, $idunno, $sticky) = split("/", substr($_, 1));
        ($pathname) = $directory . "/" . $filename;
        $cvs_revision{$pathname} = $rev;
        $cvs_date{$pathname} = $date;
        $cvs_sticky_revision{$pathname} = $sticky;
        $cvs_files{$directory} .= "$filename\\";
    }
    close(ENTRIES);

    return if !open(REPOSITORY, "$cvsdir/Repository");
    $repository = <REPOSITORY>;
    chop($repository);
    close(REPOSITORY);
    $repository{$directory} = $repository;
}

# Given path to file in CVS working directory, compute path to RCS
# repository file.  Cache that info for future use.


sub rcs_pathname {
    ($pathname) = @_;

    if ($pathname =~ m@/@) {
        ($directory,$filename) = $pathname =~ m@(.*)/([^/]+)$@;
    } else {
        ($directory,$filename) = ('.',$pathname);
        $pathname = "./" . $pathname;
    }
    if (!defined($repository{$directory})) {
        &read_cvs_entries($directory);
    }
       
    if (!defined($cvs_revision{$pathname})) {
        die "$::progname: error: File '$pathname' does not appear to be under" .
            " CVS control.\n"
    }

    print STDERR "file: $filename\n" if $debug;
    my ($rcs_path) = $repository{$directory} . '/' . $filename . ',v';
    return $rcs_path if (-r $rcs_path);

    # A file that exists only on the branch, not on the trunk, is found
    # in the Attic subdir.
    return $repository{$directory} . '/Attic/' . $filename . ',v';
}

sub show_annotated_cvs_file {
    my ($pathname) = @_;
    my (@output) = ();

    my $revision = &parse_cvs_file($pathname);

    @text = &extract_revision($revision);
    die "$::progname: Internal consistency error" if ($#text != $#::revision_map);

    # Set total width of line annotation.
    # Warning: field widths here must match format strings below.
    $annotation_width = 0;
    $annotation_width +=  8 if $::opt_a; # author
    $annotation_width +=  7 if $::opt_v; # revision
    $annotation_width +=  6 if $::opt_A; # age
    $annotation_width += 12 if $::opt_d; # date
    $blank_annotation = ' ' x $annotation_width;

    if ($multiple_files_on_command_line) {
        print "\n", "=" x (83 + $annotation_width);
        print "\n$::progname: Listing file: $pathname\n"
    }

    # Print each line of the revision, preceded by its annotation.
    $line = 0;
    foreach $revision (@::revision_map) {
        $text = $text[$line++];
        $annotation = '';

        # Annotate with revision author
        $annotation .= sprintf("%-8s", $::revision_author{$revision}) if $::opt_a;

        # Annotate with revision number
        $annotation .= sprintf(" %-6s", $revision) if $::opt_v;

        # Date annotation
        $annotation .= " $::revision_ctime{$revision}" if $::opt_d;

        # Age annotation ?
        $annotation .= sprintf(" (%3s)",
                               int($revision_age{$revision})) if $::opt_A;

        # -m (if-modified-since) annotion ?
        if ($::opt_m && ($::timestamp{$revision} < $::opt_m_timestamp)) {
            $annotation = $blank_annotation;
        }

        # Suppress annotation of whitespace lines, if requested;
        $annotation = $blank_annotation if $::opt_w && ($text =~ /^\s*$/);

#       printf "%4d ", $line if $::opt_l;
#       print "$annotation - $text";
        push(@output, sprintf("%4d ", $line)) if $::opt_l;
        push(@output, "$annotation - $text");
    }
    @output;
}

sub usage {
    die
"$::progname: usage: [options] [file|dir]...\n",
"   Options:\n",
"      -r <revision>      Specify CVS revision of file to display\n",
"                         <revision> can be any of:\n",
"                           + numeric tag, e.g. 1.23,\n",
"                           + symbolic branch or revision tag, e.g. CHEDDAR,\n",
"                           + HEAD keyword (most recent revision on trunk)\n",
"      -a                 Annotate lines with author (username)\n",
"      -A                 Annotate lines with age, in days\n",
"      -v                 Annotate lines with revision number\n", 
"      -d                 Annotate lines with date, in local time zone\n",
"      -l                 Annotate lines with line number\n",
"      -w                 Don't annotate all-whitespace lines\n",
"      -m <# days>        Only annotate lines modified within last <# days>\n",
"      -h                 Print help (this message)\n\n",
"   (-a -v assumed, if none of -a, -v, -A, -d supplied)\n"
;
}

 
&usage if (!&Getopts('r:m:Aadhlvw'));
&usage if ($::opt_h);             # help option

$multiple_files_on_command_line = 1 if ($#ARGV != 0);

&cvsblame_init;

sub annotate_cvs_directory
{
    my ($dir) = @_;
    &read_cvs_entries($dir);
    foreach $file (split(/\\/, $cvs_files{$dir})) {
        &show_annotated_cvs_file("$dir/$file");
    }
}

# No files on command-line ?  Use current directory.
push(@ARGV, '.') if ($#ARGV == -1);
 
# Iterate over files/directories on command-line
while ($#ARGV >= 0) {
    $pathname = shift @ARGV;
    # Is it a directory ?
    if (-d $pathname) {
        &traverse_cvs_tree($pathname, *annotate_cvs_directory);

    # No, it must be a file.
    } else {
        &show_annotated_cvs_file($pathname);
    }
}


1;
