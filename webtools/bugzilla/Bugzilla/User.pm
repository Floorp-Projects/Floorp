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
# Contributor(s): Myk Melez <myk@mozilla.org>
#                 Erik Stambaugh <not_erik@dasbistro.com>

################################################################################
# Module Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

# This module implements utilities for dealing with Bugzilla users.
package Bugzilla::User;

################################################################################
# Functions
################################################################################

my $user_cache = {};
sub new {
    # Returns a hash of information about a particular user.

    my $invocant = shift;
    my $class = ref($invocant) || $invocant;
  
    my $exists = 1;
    my ($id, $name, $email) = @_;
    
    return undef if !$id;
    return $user_cache->{$id} if exists($user_cache->{$id});
    
    my $self = { 'id' => $id };
    
    bless($self, $class);
    
    if (!$name && !$email) {
        &::PushGlobalSQLState();
        &::SendSQL("SELECT 1, realname, login_name FROM profiles WHERE userid = $id");
        ($exists, $name, $email) = &::FetchSQLData();
        &::PopGlobalSQLState();
    }
    
    $self->{'name'} = $name;
    $self->{'email'} = $email || "__UNKNOWN__";
    $self->{'exists'} = $exists;
        
    # Generate a string to identify the user by name + email if the user
    # has a name or by email only if she doesn't.
    $self->{'identity'} = $name ? "$name <$email>" : $email;

    # Generate a user "nickname" -- i.e. a shorter, not-necessarily-unique name 
    # by which to identify the user.  Currently the part of the user's email
    # address before the at sign (@), but that could change, especially if we
    # implement usernames not dependent on email address.
    my @email_components = split("@", $email);
    $self->{'nick'} = $email_components[0];
    
    $user_cache->{$id} = $self;
    
    return $self;
}

sub match {
    # Generates a list of users whose login name (email address) or real name
    # matches a substring or wildcard.

    # $str contains the string to match, while $limit contains the
    # maximum number of records to retrieve.
    my ($str, $limit, $exclude_disabled) = @_;
    
    my @users = ();

    return \@users if $str =~ /^\s*$/;

    # The search order is wildcards, then exact match, then substring search.
    # Wildcard matching is skipped if there is no '*', and exact matches will
    # not (?) have a '*' in them.  If any search comes up with something, the
    # ones following it will not execute.

    # first try wildcards

    my $wildstr = $str;

    if ($wildstr =~ s/\*/\%/g) {   # don't do wildcards if no '*' in the string

        # Build the query.
        my $sqlstr = &::SqlQuote($wildstr);
        my $query  = "SELECT userid, realname, login_name " .
                     "FROM profiles " .
                     "WHERE (login_name LIKE $sqlstr " .
                     "OR realname LIKE $sqlstr) ";
        $query    .= "AND disabledtext = '' " if $exclude_disabled;
        $query    .= "ORDER BY length(login_name) ";
        $query    .= "LIMIT $limit " if $limit;

        # Execute the query, retrieve the results, and make them into
        # User objects.

        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) while &::MoreSQLData();
        &::PopGlobalSQLState();

    }
    else {    # try an exact match

        my $sqlstr = &::SqlQuote($str);
        my $query  = "SELECT userid, realname, login_name " .
                     "FROM profiles " .
                     "WHERE login_name = $sqlstr ";
        # Exact matches don't care if a user is disabled.

        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) if &::MoreSQLData();
        &::PopGlobalSQLState();
    }

    # then try substring search

    if ((scalar(@users) == 0)
        && (&::Param('usermatchmode') eq 'search')
        && (length($str) >= 3))
    {

        my $sqlstr = &::SqlQuote(uc($str));

        my $query  = "SELECT  userid, realname, login_name " .
                     "FROM  profiles " .
                     "WHERE  (INSTR(UPPER(login_name), $sqlstr) " .
                     "OR INSTR(UPPER(realname), $sqlstr)) ";
        $query    .= "AND disabledtext = '' " if $exclude_disabled;
        $query    .= "ORDER BY length(login_name) ";
        $query    .= "LIMIT $limit " if $limit;

        &::PushGlobalSQLState();
        &::SendSQL($query);
        push(@users, new Bugzilla::User(&::FetchSQLData())) while &::MoreSQLData();
        &::PopGlobalSQLState();
    }

    # order @users by alpha

    @users = sort { uc($a->{'email'}) cmp uc($b->{'email'}) } @users;

    return \@users;
}

# match_field() is a CGI wrapper for the match() function.
#
# Here's what it does:
#
# 1. Accepts a list of fields along with whether they may take multiple values
# 2. Takes the values of those fields from $::FORM and passes them to match()
# 3. Checks the results of the match and displays confirmation or failure
#    messages as appropriate.
#
# The confirmation screen functions the same way as verify-new-product and
# confirm-duplicate, by rolling all of the state information into a
# form which is passed back, but in this case the searched fields are
# replaced with the search results.
#
# The act of displaying the confirmation or failure messages means it must
# throw a template and terminate.  When confirmation is sent, all of the
# searchable fields have been replaced by exact fields and the calling script
# is executed as normal.
#
# match_field must be called early in a script, before anything external is
# done with the form data.
#
# In order to do a simple match without dealing with templates, confirmation,
# or globals, simply calling Bugzilla::User::match instead will be
# sufficient.

# How to call it:
#
# Bugzilla::User::match_field ({
#   'field_name'    => { 'type' => fieldtype },
#   'field_name2'   => { 'type' => fieldtype },
#   [...]
# });
#
# fieldtype can be either 'single' or 'multi'.
#

sub match_field {

    my $fields         = shift;   # arguments as a hash
    my $matches      = {};      # the values sent to the template
    my $matchsuccess = 1;       # did the match fail?
    my $need_confirm = 0;       # whether to display confirmation screen

    # prepare default form values

    my $vars = $::vars;
    $vars->{'form'}  = \%::FORM;
    $vars->{'mform'} = \%::MFORM;

    # What does a "--do_not_change--" field look like (if any)?
    my $dontchange = $vars->{'form'}->{'dontchange'};

    # Fields can be regular expressions matching multiple form fields
    # (f.e. "requestee-(\d+)"), so expand each non-literal field
    # into the list of form fields it matches.
    my $expanded_fields = {};
    foreach my $field_pattern (keys %{$fields}) {
        # Check if the field has any non-word characters.  Only those fields
        # can be regular expressions, so don't expand the field if it doesn't
        # have any of those characters.
        if ($field_pattern =~ /^\w+$/) {
            $expanded_fields->{$field_pattern} = $fields->{$field_pattern};
        }
        else {
            my @field_names = grep(/$field_pattern/, keys %{$vars->{'form'}});
            foreach my $field_name (@field_names) {
                $expanded_fields->{$field_name} = 
                  { type => $fields->{$field_pattern}->{'type'} };
                
                # The field is a requestee field; in order for its name 
                # to show up correctly on the confirmation page, we need 
                # to find out the name of its flag type.
                if ($field_name =~ /^requestee-(\d+)$/) {
                    my $flag = Bugzilla::Flag::get($1);
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      $flag->{'type'};
                }
                elsif ($field_name =~ /^requestee_type-(\d+)$/) {
                    $expanded_fields->{$field_name}->{'flag_type'} = 
                      Bugzilla::FlagType::get($1);
                }
            }
        }
    }
    $fields = $expanded_fields;

    # Skip all of this if the option has been turned off
    return 1 if (&::Param('usermatchmode') eq 'off');

    for my $field (keys %{$fields}) {

        # Tolerate fields that do not exist.
        #
        # This is so that fields like qa_contact can be specified in the code
        # and it won't break if $::MFORM does not define them.
        #
        # It has the side-effect that if a bad field name is passed it will be
        # quietly ignored rather than raising a code error.

        next if !defined($vars->{'mform'}->{$field});

        # Skip it if this is a --do_not_change-- field
        next if $dontchange && $dontchange eq $vars->{'form'}->{$field};

        # We need to move the query to $raw_field, where it will be split up,
        # modified by the search, and put back into $::FORM and $::MFORM
        # incrementally.

        my $raw_field = join(" ", @{$vars->{'mform'}->{$field}});
        $vars->{'form'}->{$field}  = '';
        $vars->{'mform'}->{$field} = [];

        my @queries = ();

        # Now we either split $raw_field by spaces/commas and put the list
        # into @queries, or in the case of fields which only accept single
        # entries, we simply use the verbatim text.

        $raw_field =~ s/^\s+|\s+$//sg;  # trim leading/trailing space

        # single field
        if ($fields->{$field}->{'type'} eq 'single') {
            @queries = ($raw_field) unless $raw_field =~ /^\s*$/;

        # multi-field
        }
        elsif ($fields->{$field}->{'type'} eq 'multi') {
            @queries =  split(/[\s,]+/, $raw_field);

        }
        else {
            # bad argument
            $vars->{'argument'} = $fields->{$field}->{'type'};
            $vars->{'function'} = 'Bugzilla::User::match_field';
            &::ThrowCodeError('bad_arg');
        }

        for my $query (@queries) {

            my $users = match(
                $query,                                 # match string
                (&::Param('maxusermatches') || 0) + 1,  # match limit
                1                                       # exclude_disabled
            );

            # skip confirmation for exact matches
            if ((scalar(@{$users}) == 1)
                && (@{$users}[0]->{'email'} eq $query))
            {
                # delimit with spaces if necessary
                if ($vars->{'form'}->{$field}) {
                    $vars->{'form'}->{$field} .= " ";
                }
                $vars->{'form'}->{$field} .= @{$users}[0]->{'email'};
                push @{$vars->{'mform'}->{$field}}, @{$users}[0]->{'email'};
                next;
            }

            $matches->{$field}->{$query}->{'users'}  = $users;
            $matches->{$field}->{$query}->{'status'} = 'success';

            # here is where it checks for multiple matches

            if (scalar(@{$users}) == 1) { # exactly one match
                # delimit with spaces if necessary
                if ($vars->{'form'}->{$field}) {
                    $vars->{'form'}->{$field} .= " ";
                }
                $vars->{'form'}->{$field} .= @{$users}[0]->{'email'};
                push @{$vars->{'mform'}->{$field}}, @{$users}[0]->{'email'};
                $need_confirm = 1 if &::Param('confirmuniqueusermatch');

            }
            elsif ((scalar(@{$users}) > 1)
                    && (&::Param('maxusermatches') != 1)) {
                $need_confirm = 1;

                if ((&::Param('maxusermatches'))
                   && (scalar(@{$users}) > &::Param('maxusermatches')))
                {
                    $matches->{$field}->{$query}->{'status'} = 'trunc';
                    pop @{$users};  # take the last one out
                }

            }
            else {
                # everything else fails
                $matchsuccess = 0; # fail
                $matches->{$field}->{$query}->{'status'} = 'fail';
                $need_confirm = 1;  # confirmation screen shows failures
            }
        }
    }

    return 1 unless $need_confirm; # skip confirmation if not needed.

    $vars->{'script'}        = $ENV{'SCRIPT_NAME'}; # for self-referencing URLs
    $vars->{'fields'}        = $fields; # fields being matched
    $vars->{'matches'}       = $matches; # matches that were made
    $vars->{'matchsuccess'}  = $matchsuccess; # continue or fail

    print "Content-type: text/html\n\n";

    $::template->process("global/confirm-user-match.html.tmpl", $vars)
      || &::ThrowTemplateError($::template->error());

    exit;

}

sub email_prefs {
    # Get or set (not implemented) the user's email notification preferences.
    
    my $self = shift;
    
    # If the calling code is setting the email preferences, update the object
    # but don't do anything else.  This needs to write email preferences back
    # to the database.
    if (@_) { $self->{email_prefs} = shift; return; }
    
    # If we already got them from the database, return the existing values.
    return $self->{email_prefs} if $self->{email_prefs};
    
    # Retrieve the values from the database.
    &::SendSQL("SELECT emailflags FROM profiles WHERE userid = $self->{id}");
    my ($flags) = &::FetchSQLData();

    my @roles = qw(Owner Reporter QAcontact CClist Voter);
    my @reasons = qw(Removeme Comments Attachments Status Resolved Keywords 
                     CC Other Unconfirmed);

    # If the prefs are empty, this user hasn't visited the email pane
    # of userprefs.cgi since before the change to use the "emailflags" 
    # column, so initialize that field with the default prefs.
    if (!$flags) {
        # Create a default prefs string that causes the user to get all email.
        $flags = "ExcludeSelf~on~FlagRequestee~on~FlagRequester~on~";
        foreach my $role (@roles) {
            foreach my $reason (@reasons) {
                $flags .= "email$role$reason~on~";
            }
        }
        chop $flags;
    }

    # Convert the prefs from the flags string from the database into
    # a Perl record.  The 255 param is here because split will trim 
    # any trailing null fields without a third param, which causes Perl 
    # to eject lots of warnings. Any suitably large number would do.
    my $prefs = { split(/~/, $flags, 255) };
    
    # Determine the value of the "excludeself" global email preference.
    # Note that the value of "excludeself" is assumed to be off if the
    # preference does not exist in the user's list, unlike other 
    # preferences whose value is assumed to be on if they do not exist.
    $prefs->{ExcludeSelf} = 
      exists($prefs->{ExcludeSelf}) && $prefs->{ExcludeSelf} eq "on";
    
    # Determine the value of the global request preferences.
    foreach my $pref (qw(FlagRequestee FlagRequester)) {
        $prefs->{$pref} = !exists($prefs->{$pref}) || $prefs->{$pref} eq "on";
    }
    
    # Determine the value of the rest of the preferences by looping over
    # all roles and reasons and converting their values to Perl booleans.
    foreach my $role (@roles) {
        foreach my $reason (@reasons) {
            my $key = "email$role$reason";
            $prefs->{$key} = !exists($prefs->{$key}) || $prefs->{$key} eq "on";
        }
    }

    $self->{email_prefs} = $prefs;
    
    return $self->{email_prefs};
}

1;
