# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>
#                 Dave Miller <justdave@syndicomm.com>
#                 Christopher Aillon <christopher@aillon.com>
#                 Gervase Markham <gerv@gerv.net>
#                 Christian Reis <kiko@async.com.br>

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use strict;
use lib ".";

# use Carp;                       # for confess

BEGIN {
    if ($^O =~ /MSWin32/i) {
        # Help CGI find the correct temp directory as the default list
        # isn't Windows friendly (Bug 248988)
        $ENV{'TMPDIR'} = $ENV{'TEMP'} || $ENV{'TMP'} || "$ENV{'WINDIR'}\\TEMP";
    }
}

use Bugzilla::Util;
use Bugzilla::Config;
use Bugzilla::Constants;
use Bugzilla::Error;

# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

sub CGI_pl_sillyness {
    my $zz;
    $zz = $::buffer;
}

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

use vars qw($template $vars);

# If Bugzilla is shut down, do not go any further, just display a message
# to the user about the downtime.  (do)editparams.cgi is exempted from
# this message, of course, since it needs to be available in order for
# the administrator to open Bugzilla back up.
if (Param("shutdownhtml") && $0 !~ m:(^|[\\/])(do)?editparams\.cgi$:) {
    $::vars->{'message'} = "shutdown";
    
    # Return the appropriate HTTP response headers.
    print Bugzilla->cgi->header();
    
    # Generate and return an HTML message about the downtime.
    $::template->process("global/message.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    exit;
}

# Implementations of several of the below were blatently stolen from CGI.pm,
# by Lincoln D. Stein.

# Get rid of all the %xx encoding and the like from the given URL.
sub url_decode {
    my ($todecode) = (@_);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# check and see if a given field exists, is non-empty, and is set to a 
# legal value.  assume a browser bug and abort appropriately if not.
# if $legalsRef is not passed, just check to make sure the value exists and 
# is non-NULL
sub CheckFormField (\%$;\@) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
        $legalsRef               # (optional) ref to a list of legal values 
       ) = @_;

    if ( !defined $formRef->{$fieldname} ||
         trim($formRef->{$fieldname}) eq "" ||
         (defined($legalsRef) && 
          lsearch($legalsRef, $formRef->{$fieldname})<0) ){

        SendSQL("SELECT description FROM fielddefs WHERE name=" . SqlQuote($fieldname));
        my $result = FetchOneColumn();
        my $field;
        if ($result) {
            $field = $result;
        }
        else {
            $field = $fieldname;
        }
        
        ThrowCodeError("illegal_field", { field => $field }, "abort");
      }
}

# check and see if a given field is defined, and abort if not
sub CheckFormFieldDefined (\%$) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
       ) = @_;

    if (!defined $formRef->{$fieldname}) {
        ThrowCodeError("undefined_field", { field => $fieldname });
    }
}

sub BugAliasToID {
    # Queries the database for the bug with a given alias, and returns
    # the ID of the bug if it exists or the undefined value if it doesn't.
    
    my ($alias) = @_;
    
    return undef unless Param("usebugaliases");
    
    PushGlobalSQLState();
    SendSQL("SELECT bug_id FROM bugs WHERE alias = " . SqlQuote($alias));
    my $id = FetchOneColumn();
    PopGlobalSQLState();
    
    return $id;
}

sub ValidateBugID {
    # Validates and verifies a bug ID, making sure the number is a 
    # positive integer, that it represents an existing bug in the
    # database, and that the user is authorized to access that bug.
    # We detaint the number here, too

    my ($id, $skip_authorization) = @_;
    
    # Get rid of white-space around the ID.
    $id = trim($id);
    
    # If the ID isn't a number, it might be an alias, so try to convert it.
    my $alias = $id;
    if (!detaint_natural($id)) {
        $id = BugAliasToID($alias);
        $id || ThrowUserError("invalid_bug_id_or_alias", {'bug_id' => $alias});
    }
    
    # Modify the calling code's original variable to contain the trimmed,
    # converted-from-alias ID.
    $_[0] = $id;
    
    # First check that the bug exists
    SendSQL("SELECT bug_id FROM bugs WHERE bug_id = $id");

    FetchOneColumn()
      || ThrowUserError("invalid_bug_id_non_existent", {'bug_id' => $id});

    return if $skip_authorization;
    
    return if Bugzilla->user->can_see_bug($id);

    # The user did not pass any of the authorization tests, which means they
    # are not authorized to see the bug.  Display an error and stop execution.
    # The error the user sees depends on whether or not they are logged in
    # (i.e. $::userid contains the user's positive integer ID).
    if ($::userid) {
        ThrowUserError("bug_access_denied", {'bug_id' => $id});
    } else {
        ThrowUserError("bug_access_query", {'bug_id' => $id});
    }
}

sub ValidateComment {
    # Make sure a comment is not too large (greater than 64K).
    
    my ($comment) = @_;
    
    if (defined($comment) && length($comment) > 65535) {
        ThrowUserError("comment_too_long");
    }
}

sub PasswordForLogin {
    my ($login) = (@_);
    SendSQL("select cryptpassword from profiles where login_name = " .
            SqlQuote($login));
    my $result = FetchOneColumn();
    if (!defined $result) {
        $result = "";
    }
    return $result;
}

sub CheckEmailSyntax {
    my ($addr) = (@_);
    my $match = Param('emailregexp');
    if ($addr !~ /$match/ || $addr =~ /[\\\(\)<>&,;:"\[\] \t\r\n]/) {
        ThrowUserError("illegal_email_address", { addr => $addr });
    }
}

sub MailPassword {
    my ($login, $password) = (@_);
    my $urlbase = Param("urlbase");
    my $template = Param("passwordmail");
    my $msg = PerformSubsts($template,
                            {"mailaddress" => $login . Param('emailsuffix'),
                             "login" => $login,
                             "password" => $password});

    open SENDMAIL, "|/usr/lib/sendmail -t -i";
    print SENDMAIL $msg;
    close SENDMAIL;
}

sub PutHeader {
    ($vars->{'title'}, $vars->{'h1'}, $vars->{'h2'}) = (@_);
     
    $::template->process("global/header.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
    $vars->{'header_done'} = 1;
}

sub PutFooter {
    $::template->process("global/footer.html.tmpl", $::vars)
      || ThrowTemplateError($::template->error());
}

sub CheckIfVotedConfirmed {
    my ($id, $who) = (@_);
    SendSQL("SELECT bugs.votes, bugs.bug_status, products.votestoconfirm, " .
            "       bugs.everconfirmed " .
            "FROM bugs, products " .
            "WHERE bugs.bug_id = $id AND products.id = bugs.product_id");
    my ($votes, $status, $votestoconfirm, $everconfirmed) = (FetchSQLData());
    if ($votes >= $votestoconfirm && $status eq $::unconfirmedstate) {
        SendSQL("UPDATE bugs SET bug_status = 'NEW', everconfirmed = 1 " .
                "WHERE bug_id = $id");
        my $fieldid = GetFieldID("bug_status");
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($id,$who,now(),$fieldid,'$::unconfirmedstate','NEW')");
        if (!$everconfirmed) {
            $fieldid = GetFieldID("everconfirmed");
            SendSQL("INSERT INTO bugs_activity " .
                    "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                    "($id,$who,now(),$fieldid,'0','1')");
        }
        
        AppendComment($id, DBID_to_name($who),
                      "*** This bug has been confirmed by popular vote. ***", 0);
                      
        $vars->{'type'} = "votes";
        $vars->{'id'} = $id;
        $vars->{'mailrecipients'} = { 'changer' => $who };
        
        $template->process("bug/process/results.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
    }

}
sub LogActivityEntry {
    my ($i,$col,$removed,$added,$whoid,$timestamp) = @_;
    # in the case of CCs, deps, and keywords, there's a possibility that someone    # might try to add or remove a lot of them at once, which might take more
    # space than the activity table allows.  We'll solve this by splitting it
    # into multiple entries if it's too long.
    while ($removed || $added) {
        my ($removestr, $addstr) = ($removed, $added);
        if (length($removestr) > 254) {
            my $commaposition = FindWrapPoint($removed, 254);
            $removestr = substr($removed,0,$commaposition);
            $removed = substr($removed,$commaposition);
            $removed =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $removed = ""; # no more entries
        }
        if (length($addstr) > 254) {
            my $commaposition = FindWrapPoint($added, 254);
            $addstr = substr($added,0,$commaposition);
            $added = substr($added,$commaposition);
            $added =~ s/^[,\s]+//; # remove any comma or space
        } else {
            $added = ""; # no more entries
        }
        $addstr = SqlQuote($addstr);
        $removestr = SqlQuote($removestr);
        my $fieldid = GetFieldID($col);
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,removed,added) VALUES " .
                "($i,$whoid," . SqlQuote($timestamp) . ",$fieldid,$removestr,$addstr)");
    }
}

sub GetBugActivity {
    my ($id, $starttime) = (@_);
    my $datepart = "";

    die "Invalid id: $id" unless $id=~/^\s*\d+\s*$/;

    if (defined $starttime) {
        $datepart = "and bugs_activity.bug_when > " . SqlQuote($starttime);
    }
    my $suppjoins = "";
    my $suppwhere = "";
    if (Param("insidergroup") && !UserInGroup(Param('insidergroup'))) {
        $suppjoins = "LEFT JOIN attachments 
                   ON attachments.attach_id = bugs_activity.attach_id";
        $suppwhere = "AND NOT(COALESCE(attachments.isprivate,0))"; 
    }
    my $query = "
        SELECT COALESCE(fielddefs.description, bugs_activity.fieldid),
                fielddefs.name,
                bugs_activity.attach_id,
                DATE_FORMAT(bugs_activity.bug_when,'%Y.%m.%d %H:%i'),
                bugs_activity.removed, bugs_activity.added,
                profiles.login_name
        FROM bugs_activity $suppjoins LEFT JOIN fielddefs ON 
                                     bugs_activity.fieldid = fielddefs.fieldid,
             profiles
        WHERE bugs_activity.bug_id = $id $datepart
              AND profiles.userid = bugs_activity.who $suppwhere
        ORDER BY bugs_activity.bug_when";

    SendSQL($query);
    
    my @operations;
    my $operation = {};
    my $changes = [];
    my $incomplete_data = 0;
    
    while (my ($field, $fieldname, $attachid, $when, $removed, $added, $who) 
                                                               = FetchSQLData())
    {
        my %change;
        my $activity_visible = 1;
        
        # check if the user should see this field's activity
        if ($fieldname eq 'remaining_time' ||
            $fieldname eq 'estimated_time' ||
            $fieldname eq 'work_time') {

            if (!UserInGroup(Param('timetrackinggroup'))) {
                $activity_visible = 0;
            } else {
                $activity_visible = 1;
            }
        } else {
            $activity_visible = 1;
        }
                
        if ($activity_visible) {
            # This gets replaced with a hyperlink in the template.
            $field =~ s/^Attachment// if $attachid;

            # Check for the results of an old Bugzilla data corruption bug
            $incomplete_data = 1 if ($added =~ /^\?/ || $removed =~ /^\?/);
        
            # An operation, done by 'who' at time 'when', has a number of
            # 'changes' associated with it.
            # If this is the start of a new operation, store the data from the
            # previous one, and set up the new one.
            if ($operation->{'who'} 
                && ($who ne $operation->{'who'} 
                    || $when ne $operation->{'when'})) 
            {
                $operation->{'changes'} = $changes;
                push (@operations, $operation);
            
                # Create new empty anonymous data structures.
                $operation = {};
                $changes = [];
            }  
        
            $operation->{'who'} = $who;
            $operation->{'when'} = $when;            
        
            $change{'field'} = $field;
            $change{'fieldname'} = $fieldname;
            $change{'attachid'} = $attachid;
            $change{'removed'} = $removed;
            $change{'added'} = $added;
            push (@$changes, \%change);
        }   
    }
    
    if ($operation->{'who'}) {
        $operation->{'changes'} = $changes;
        push (@operations, $operation);
    }
    
    return(\@operations, $incomplete_data);
}

############# Live code below here (that is, not subroutine defs) #############

use Bugzilla;

# XXX - mod_perl - reset this between runs
$::cgi = Bugzilla->cgi;

# Set up stuff for compatibility with the old CGI.pl code
# This code will be removed as soon as possible, in favour of
# using the CGI.pm stuff directly

# XXX - mod_perl - reset these between runs

foreach my $name ($::cgi->param()) {
    my @val = $::cgi->param($name);
    $::FORM{$name} = join('', @val);
    $::MFORM{$name} = \@val;
}

$::buffer = $::cgi->query_string();

# This could be needed in any CGI, so we set it here.
$vars->{'help'} = $::cgi->param('help') ? 1 : 0;

1;
