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
    $self->{'email'} = $email;
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
    # matches a substring.

    # $str contains the string to match against, while $limit contains the
    # maximum number of records to retrieve.
    my ($str, $limit, $exclude_disabled) = @_;
    
    # Build the query.
    my $sqlstr = &::SqlQuote($str);
    my $qry = "
          SELECT  userid, realname, login_name
            FROM  profiles
           WHERE  (INSTR(login_name, $sqlstr) OR INSTR(realname, $sqlstr))
    ";
    $qry .= "AND disabledtext = '' " if $exclude_disabled;
    $qry .= "ORDER BY realname, login_name ";
    $qry .= "LIMIT $limit " if $limit;

    # Execute the query, retrieve the results, and make them into User objects.
    my @users;
    &::PushGlobalSQLState();
    &::SendSQL($qry);
    push(@users, new Bugzilla::User(&::FetchSQLData())) while &::MoreSQLData();
    &::PopGlobalSQLState();

    return \@users;
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
