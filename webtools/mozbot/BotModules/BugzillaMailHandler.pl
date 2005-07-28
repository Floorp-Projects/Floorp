#!/usr/bin/perl -w
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Mozilla IRC Bot
#
# The Initial Developer of the Original Code is Max Kanat-Alexander.
# Portions developed by Max Kanat-Alexander are Copyright (C) 2005
# Max Kanat-Alexander.  All Rights Reserved.
#
# Contributor(s): Max Kanat-Alexander <mkanat@bugzilla.org>
#
# This is loosely based off an older bugmail.pl by justdave.

# bugmail.pl requires that you have X-Bugzilla-Product and
# X-Bugzilla-Component headers in your incoming email. In 2.19.2 and above,
# this is easy. You just add two lines to your newchangedmail param:
# X-Bugzilla-Product: %product%
# X-Bugzilla-Component: %component%
# If you're running 2.18, you can do the same thing, but you need to
# apply the patch from bug 175222 <https://bugzilla.mozilla.org/show_bug.cgi?id=175222>
# to your installation.

use strict;
use Fcntl qw(:flock);
use File::Basename;

use Email::Simple;

#####################################################################
# Constants And Initial Setup
#####################################################################

# What separates Product//Component//[Fields], etc. in a log line.
use constant FIELD_SEPARATOR => '::::';

# These are fields that are multi-select fields, so when somebody
# adds something to them, the verbs "added to " or "removed from" should 
# be used instead of the verb "changed" or "set".
# It's a hash, where the names of the fields are the keys, and the values are 1.
# The fields are named as they appear in the "What" part of a bugmail "changes"
# table.
use constant MULTI_FIELDS => {
    'CC' => 1, 'Group' => 1, 'Keywords' => 1,
    'BugsThisDependsOn' => 1, 'OtherBugsDependingOnThis' => 1,
};

# Some fields have such long names for the "What" column that their names
# wrap. Normally, our code would think that those fields were two different
# fields. So, instead, we store a list of strings to use as an argument
# to "grep" for the field names that we need to "unwrap."
use constant UNWRAP_WHAT => ( 
    qr/^Attachment .\d+$/, qr/^Attachment .\d+ is$/, qr/^OtherBugsDep/, 
);

# Should be whatever Bugzilla::Util::find_wrap_point (or FindWrapPoint) 
# breaks on, in Bugzilla.
use constant BREAKING_CHARACTERS => (' ',',','-');

# The maximum width, in characters, of each field of the "diffs" table.
use constant WIDTH_WHAT    => 19;
use constant WIDTH_REMOVED => 28;
use constant WIDTH_ADDED   => 28;

# Our one command-line argument.
our $debug = $ARGV[0] && $ARGV[0] eq "-d";

# XXX - This probably should happen in the log directory instead, but that's
#       more difficult to figure out reliably.
my $bug_log = dirname($0) . '/.bugmail.log';

#####################################################################
# Utility Functions
#####################################################################

# When processing the "diffs" table in a bug, some lines wrap. This
# function properly appends the "next" line for unwrapping to an 
# existing string.
sub append_diffline ($$$$) {
    my ($append_to, $prev_line, $append_line, $max_width) = @_;
    my $ret_line = $append_to;

    debug_print("Appending Line: [$append_line] Prev Line: [$prev_line]");
    debug_print("Prev Line Len: " . length($prev_line) 
        . " Max Width: $max_width");

    # If the previous line is the width of the entire column, we
    # assume that we were forcibly wrapped in the middle of a word,
    # and no space is needed. We only add the space if we were actually
    # given a non-empty string to append.
    if ($append_line && length($prev_line) != $max_width) {
        debug_print("Adding a space unless we find a breaking character.");
        # However, sometimes even if we have a very short line, if it ended
        # in a "breaking character" like '-' then we also don't need a space.
        $ret_line .= " " unless grep($prev_line =~ /$_$/, BREAKING_CHARACTERS);
    }
    $ret_line .= $append_line;
    debug_print("Appended Line: [$ret_line]");
    return $ret_line;
}

# Prints a string if debugging is on. Appends a newline so you don't have to.
sub debug_print ($) {
    (print STDERR $_[0] . "\n") if $debug;
}

# Helps with generate_log for Flag messages.
sub flag_action ($$) {
    my ($new, $old) = @_;

    my $line = "";

    my ($flag_name, $action, $requestee) = split_flag($new);
    debug_print("Parsing Flag Change: Name: [$flag_name] Action: [$action]") 
        if $new;

    if (!$new) {
        $line .= " cancelled $old";
    }
    elsif ($action eq '+') {
        $line .= " granted $flag_name";
    }
    elsif ($action eq '-') {
        $line .= " denied $flag_name";
    }
    else {
        $line .= " requested $flag_name from";
        if ($requestee) {
            $line .= " " . $requestee;
        }
        else {
            $line .= " the wind";
        }
    }

    return $line;
}

# Takes the $old and $new from a Flag field and returns a hash,
# where the key is the name of the field, and the value is an
# array, where the first item is the old flag string, and the
# new flag string is the second item.
sub parse_flags ($$) {
    my ($new, $old) = @_;

    my %flags;
    foreach my $old_item (split /\s*,\s*/, $old) {
        my ($flag_name) = split_flag($old_item);
        $flags{$flag_name} = [$old_item, ''];
    }
    foreach my $new_item (split /\s*,\s*/, $new) {
        my ($flag_name) = split_flag($new_item);
        if (!exists $flags{$flag_name}) {
            $flags{$flag_name} = ['', $new_item];
        }
        else {
            $flags{$flag_name}[1] = $new_item;
        }
    }

    return %flags;
}

# Returns a list: the name of the flag, the action (+/-/?), and
# the requestee (if that exists).
sub split_flag ($) {
    my ($flag) = @_;
    if ($flag) {
        $flag =~ /\s*([^\?]+)(\+|-|\?)(?:\((.*)\))?$/;
        return ($1, $2, $3);
    }
    return ();
}

# Cuts the whitespace off the ends of a string. 
# Lovingly borrowed from Bugzilla::Util.
sub trim ($) {
    my ($str) = @_;
    if ($str) {
      $str =~ s/^\s+//g;
      $str =~ s/\s+$//g;
    }
    return $str;
}

#####################################################################
# Main Subroutines
#####################################################################

# Returns a hash, where the keys are the names of fields. The values
# are lists, where the first item is what was removed and the second
# item is what was added.
sub parse_diffs ($) {
    my ($body_lines) = @_;
    my @body = @$body_lines;

    my %changes = ();

    # Read in the What | Removed | Added table.
    # End|of|table will never get run
    my @diff_table = grep (/^.*\|.*\|.*$/, @body);
    # The first line is the "What|Removed|Added" line, so goes away.
    shift(@diff_table);

    my ($prev_what, $prev_added, $prev_removed);
    # We can't use foreach because we need to modify @diff_table.
    while (defined (my $line = shift @diff_table)) {
        $line =~ /^(.*)\|(.*)\|(.*)$/;
        my ($what, $removed, $added) = (trim($1), trim($2), trim($3));
        # These are used to set $prev_removed and $prev_added later.
        my ($this_removed, $this_added) = ($removed, $added);
        
        debug_print("---RawLine: $what|$removed|$added\n");

        # If we have a field name in the What field.
        if ($what) {
            # If this is a two-line "What" field...
            if( grep($what =~ $_, UNWRAP_WHAT) ) {
                # Then we need to grab the next line right now.
                my $next_line = shift @diff_table;
                debug_print("Next Line: $next_line");
                $next_line =~ /^(.*)\|(.*)\|(.*)$/;
                my ($next_what, $next_removed, $next_added) = 
                    (trim($1), trim($2), trim($3));

                debug_print("Two-line What: [$what][$next_what]");
                $what    = append_diffline($what, $what, $next_what, 
                                           WIDTH_WHAT);
                if ($next_added) {
                    debug_print("Two-line Added: [$added][$next_added]");
                    $added   = append_diffline($added, $added, 
                                               $next_added, WIDTH_ADDED);
                }
                if ($next_removed) {
                    debug_print("Two-line Removed: [$removed][$next_removed]");
                    $removed = append_diffline($removed, $removed, 
                        $next_removed, WIDTH_REMOVED);
                }
            }

            $changes{$what} = [$removed, $added];
            debug_print("Filed as $what: $removed => $added");

            # We only set $prev_what if we actually had a $what to put in it.
            $prev_what = $what;
        }
        # Otherwise we're getting data from a previous What.
        else {
            my $new_removed = append_diffline($changes{$prev_what}[0],
                $prev_removed, $removed, WIDTH_REMOVED);
            my $new_added   = append_diffline($changes{$prev_what}[1],
                $prev_added, $added, WIDTH_ADDED);

            $changes{$prev_what} = [$new_removed, $new_added];
            debug_print("Filed as $prev_what: $removed => $added");
        }

        ($prev_removed, $prev_added) = ($this_removed, $this_added);
    }

    return %changes;
}

# Takes a reference to an array of lines and returns a hashref
# containing data for a buglog entry.
# Returns undef if the bug should not be entered into the log.
sub parse_mail ($) {
    my ($mail_lines) = @_;
    my $mail_text = join('', @$mail_lines);
    my $email = Email::Simple->new($mail_text);

    debug_print("Parsing Message " . $email->header('Message-ID'));

    my $body = $email->body;
    my @body_lines = split("\n", $body);

    my %bug_info;

    # Bug ID
    my $subject = $email->header('Subject');

    if ($subject !~ /^\s*\[Bug (\d+)\] /i) {
        debug_print("Not bug: $subject");
        return undef;
    }
    $bug_info{'bug_id'} = $1;
    debug_print("Bug $bug_info{bug_id} found.");

    # Ignore Dependency mails
    # XXX - This should probably be an option in the mozbot instead
    if (my ($dep_line) = 
        grep /bug (\d+), which changed state\.\s*$/, @body_lines) 
    {
        debug_print("Dependency change ignored: $dep_line.");
        return undef;
    }

    # Product
    $bug_info{'product'} = $email->header('X-Bugzilla-Product');
    unless ($bug_info{'product'}) {
        debug_print("X-Bugzilla-Product header not found.");
        return undef;
    }
    debug_print("Product '$bug_info{product}' found.");

    # Component
    $bug_info{'component'} = $email->header('X-Bugzilla-Component');
    unless ($bug_info{'component'}) {
        debug_print("X-Bugzilla-Component header not found.");
        return undef;
    }
    debug_print("Component '$bug_info{component}' found.");

    # New or Changed, and getting who did it.
    if ($subject =~ /^\s*\[Bug \d+\]\s*New: /i) {
        $bug_info{'new'} = 1;
        debug_print("Bug is New.");
        my ($reporter) = grep /^\s+ReportedBy:\s/, @body_lines;
        $reporter =~ s/^\s+ReportedBy:\s//;
        $bug_info{'who'} = $reporter;
    }
    elsif ( my ($changer_line) = grep /^\S+\schanged:$/, @body_lines) {
        $changer_line =~ /^(\S+)\s/;
        $bug_info{'who'} = $1;
    }
    elsif ( my ($comment_line) = 
                grep /^-* Additional Comments From /, @body_lines )
    {
        $comment_line =~ /^-* Additional Comments From (\S+) /;
        $bug_info{'who'} = $1;
    } else {
        debug_print("Could not determine who made the change.");
        return undef;
    }
    debug_print("Who = $bug_info{who}");

    # Attachment
    my $attachid;
    if (($attachid) = grep /^Created an attachment \(id=\d+\)/, @body_lines) {
        $attachid =~ /^Created an attachment \(id=(\d+)\)/;
        $bug_info{'attach_id'} = $1;
        debug_print("attach_id: $bug_info{attach_id}");
    }

    # Duplicate
    my $dupid;
    if (($dupid) = grep /marked as a duplicate of \d+/, @body_lines) {
        $dupid =~ /marked as a duplicate of (\d+)/;
        $bug_info{'dup_of'} = $1;
        debug_print("Got dup_of: $bug_info{dup_of}");
    }

    # Figure out where the diff table ends, and where comments start.
    my $comments_start_at = 0;
    foreach my $check_line (@body_lines) {
        last if $check_line =~ /^-* Additional Comments From /;
        $comments_start_at++;
    }
    
    debug_print("Comments start at line $comments_start_at.");
    my @diff_lines = @body_lines[0 .. ($comments_start_at - 1)];
    my %diffs = parse_diffs(\@diff_lines);
    $bug_info{'diffs'} = \%diffs;

    return \%bug_info;
}

# Takes the %bug_info hash returned from parse_mail and
# makes it into one or more lines for the bugmail log.
# BugMail Log Lines have the following format:
# ID::::Product::::Component::::Who::::FieldName::::OldValue::::NewValue::::message
# OldValue and NewValue can be empty.
# FieldName will be 'NewBug' for new bugs, and 'NewAttach' for new attachments.
# Each line ends with a newline, except the last one.
sub generate_log ($) {
    my ($bug_info) = @_;

    my $prefix = $bug_info->{'bug_id'} . FIELD_SEPARATOR 
                 . $bug_info->{'product'} . FIELD_SEPARATOR
                 . $bug_info->{'component'} . FIELD_SEPARATOR
                 . $bug_info->{'who'} . FIELD_SEPARATOR;

    my @lines;

    # New bugs are easy to handle, so let's handle them first.
    if ($bug_info->{'new'}) {
        push(@lines, $prefix . 'NewBug' . FIELD_SEPARATOR 
            # Old and New are empty.
            . FIELD_SEPARATOR . FIELD_SEPARATOR
            . "New $bug_info->{product} bug $bug_info->{bug_id}"
            . " filed by $bug_info->{who}.");
    }

    if ($bug_info->{'attach_id'}) {
        push(@lines, $prefix . 'NewAttach' . FIELD_SEPARATOR
            # Old and New are empty.
            . FIELD_SEPARATOR . FIELD_SEPARATOR
            . "$bug_info->{'who'} added attachment $bug_info->{'attach_id'}"
            . " to bug $bug_info->{'bug_id'}.");
    }

    # And now we handle changes by going over all the diffs, one by one.
    my %diffs = %{$bug_info->{'diffs'}};
    foreach my $field (keys %diffs) {
        my $old = $diffs{$field}[0];
        my $new = $diffs{$field}[1];

        # For attachments, we don't want to include the bug number in
        # the output.
        $field =~ s/^(Attachment)( .)(\d+)/$1/;
        my $attach_id = $3;

        # Flags get a *very* special handling.
        if ($field =~ /Flag$/) {
            my %flags = parse_flags($new, $old);
            foreach my $flag (keys %flags) {
                my ($old_flag, $new_flag) = @{$flags{$flag}};
                my $line = $prefix . $field . FIELD_SEPARATOR
                           . $old_flag . FIELD_SEPARATOR
                           . $new_flag . FIELD_SEPARATOR
                           . $bug_info->{'who'};
                $line .= flag_action($new_flag, $old_flag);
                if ($field =~ /^Attachment/) {
                    $line .= " for attachment $attach_id";
                }
                $line .= " on bug $bug_info->{bug_id}.";
                push(@lines, $line);
            }
        }

        # All other, non-Flag fields.
        else {
            my $line = $prefix . $field . FIELD_SEPARATOR 
                       . $old . FIELD_SEPARATOR . $new . FIELD_SEPARATOR 
                       . $bug_info->{who};
            # Some fields require the verbs "added" and "removed", like the 
            # CC field.
            if (MULTI_FIELDS->{$field}) {
                ($line .= " added $new to") if $new;
                ($line .= " and") if $new && $old;
                ($line .= " removed $old from") if $old;
                $line .= " the $field field on bug $bug_info->{bug_id}.";
            }
            # If we didn't remove anything, only added something.
            elsif (!$old) {
                $line .= " set the $field field on bug"
                         . " $bug_info->{bug_id} to $new";
                # Hack for "RESOLVED DUPLICATE" messages.
                $line .= ' of bug ' . $bug_info->{dup_of} if exists $bug_info->{dup_of};
                $line .= '.';
            }
            # If we didn't add anything, only removed something.
            elsif (!$new) {
                $line .= " cleared the $field '$old' from bug"
                         . " $bug_info->{bug_id}.";
            }
            # If we changed a field from one value to another.
            else {
                $line .= " changed the $field on bug" 
                         . " $bug_info->{bug_id} from $old to $new.";
            }
            push(@lines, $line);
        }
    }

    debug_print("Generated Log Lines.");
    debug_print("Log Line: $_") foreach (@lines);
    
    return join("\n", @lines);
}

# Takes a string and appends it to the buglog.
sub append_log ($) {
    my ($string) = @_;

    (open FILE, ">>" . $bug_log)
        or die "Couldn't open bug log file $bug_log: $!";
    debug_print("Waiting for a lock on the log...");
    flock(FILE, LOCK_EX);
    print FILE $string . "\n";
    flock(FILE, LOCK_UN);
    debug_print("Printed lines to log and unlocked file.");
    close FILE;
}


#####################################################################
# Main Script
#####################################################################

debug_print("\n\n");

unless (-e $bug_log) {
    print STDERR "$bug_log does not exist, so I assume that mozbot is not"
                 . " running. Discarding incoming message.\n";
    exit;
}

my @mail_array = <STDIN>;
my $bug_info = parse_mail(\@mail_array);

if (defined $bug_info) {
    my $log_lines = generate_log($bug_info);
    # If we got an email with just a comment, $log_lines will be empty.
    append_log($log_lines) if $log_lines;
}

debug_print("All done!");
exit;
