# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::Service::Components::UserPrefs;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'component.userPrefs' or
            $service eq 'dispatcher.commands' or
            $service eq 'dispatcher.output.generic' or
            $service eq 'dispatcher.output' or
            $service eq 'setup.install' or
            $class->SUPER::provides($service));
}

__DATA__

# dispatcher.commands
sub cmdUserPrefs {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (not defined($user)) {
        $app->getService('user.login')->requireLogin($app);
        return;
    }

    # get the list of users to edit
    my @userIDs = $app->input->peekArgument('userPrefs.userID');
    if (not @userIDs) {
        # no users selected - so user wants to edit themselves
        @userIDs = ($user->userID);
    } else {
        # one or more users selected
        # let's get rid of the duplicates
        my %userIDs = map { $_ => 1 } @userIDs;
        @userIDs = keys(%userIDs);
    }

    # cache rights (for efficiently reasons)
    my @rights = $self->populateUserPrefsRights($app, $user);

    # get a handle to some useful services (also for efficiently reasons)
    my $userFactory = $app->getService('user.factory');
    my $userDataSource = $app->getService('dataSource.user');

    # get the data for each user being edited
    my $userData = {};
    my @notifications;
    foreach my $userID (@userIDs) {
        my $targetUser = $userFactory->getUserByID($app, $userID);
        if (defined($targetUser)) {
            $userData->{$userID} = $self->populateUserPrefsHash($app, $userDataSource, $user, $targetUser, $userID, $userID == $user->{userID}, @rights);
        } else {
            $self->warn(2, "someone tried to get the details of invalid user $userID");
            push(@notifications, [$userID, '', 'user.noSuchUser']);
        }
    }

    if (not @notifications) {
        # put it all together
        $app->output->userPrefs(\@userIDs, $userData, $self->populateUserPrefsMetaData($app, $userDataSource), $self->populateUserPrefsGroupNames($app, $userDataSource));
    } else {
        $app->output->userPrefsNotification(\@notifications);
    }

}

sub populateUserPrefsRights {
    my $self = shift;
    my($app, $user) = @_;
    # XXX field categories should have generic way of indicating
    # whether the user is allowed to edit either his own or other
    # people's. (Relevant rights are marked XXX below.)
    return ($user->hasRight('userPrefs.editOthers.adminMessage'),
            $user->hasRight('userPrefs.editOthers.passwords'),
            $user->hasRight('userPrefs.editOthers.mode'),
            $user->hasRight('userPrefs.editOthers.contactMethods'), # XXX
            $user->hasRight('userPrefs.editOthers.personalDetails'), # XXX
            $user->hasRight('userPrefs.editOthers.settings'), # XXX
            $user->hasRight('userPrefs.editOthers.groups'));
         # note there is no 'userPrefs.editOthers.state'
}

sub populateUserPrefsHash {
    my $self = shift;
    my($app, $userDataSource, $user, $targetUser, $targetUserID, $editingUserIsTargetUser,
       $rightPasswords, $rightAdminMessage, $rightMode, $rightContactMethods, $rightPersonalDetails, $rightSettings, $rightGroups, @otherRights) = @_;

    my $userData = {'editingUserIsTargetUser' => $editingUserIsTargetUser};

    if ($rightAdminMessage) {
        # give user admin message
        $userData->{'adminMessage'} = $targetUser->adminMessage;
    }

    if ($rightMode) {
        # give user mode
        $userData->{'mode'} = $targetUser->mode;
    }

    # XXX there is probably a more generic way of doing this...
    $userData->{'fields'} = {};
    $self->populateUserPrefsHashFieldCategory($app, $targetUser, $rightContactMethods, 'contact', $editingUserIsTargetUser, $userData);
    $self->populateUserPrefsHashFieldCategory($app, $targetUser, $rightPersonalDetails, 'personal', $editingUserIsTargetUser, $userData);
    $self->populateUserPrefsHashFieldCategory($app, $targetUser, $rightSettings, 'settings', $editingUserIsTargetUser, $userData);
    # don't do the 'state' category

    $userData->{'groups'} = {};
    if ($rightGroups) {
        foreach my $group (@{$userDataSource->getGroups($app)}) { # XXX should be cached
            # $group contains [groupID, name, [rightName]*]
            # give status of user in group
            my $groupID = $group->[0];
            $userData->{'groups'}->{$groupID} = $targetUser->levelInGroup($groupID);
        }
    } else {
        foreach my $group (@{$userDataSource->getGroups($app)}) { # XXX should be cached
            # $group contains [groupID, name, [rightName]*]
            # if $user is an op or better in this group, give status of $targetUser in group
            my $groupID = $group->[0];
            if ($user->levelInGroup($groupID) >= 2) { # XXX BARE CONSTANT ALERT
                $userData->{'groups'}->{$groupID} = $targetUser->levelInGroup($groupID);
            }
        }
    }

    return $userData;
}

sub populateUserPrefsHashFieldCategory {
    my $self = shift;
    my($app, $targetUser, $right, $category, $editingUserIsTargetUser, $userData) = @_;
    if ($right or $editingUserIsTargetUser) {
        # give user ${category}s
        $userData->{'fields'}->{$category} = {};
        if (defined($targetUser->fields->{$category})) {
            foreach my $field (values(%{$targetUser->fields->{$category}})) {
                $userData->{'fields'}->{$category}->{$field->name} = $field->data;
            }
        }
    }
}

sub populateUserPrefsMetaData {
    my $self = shift;
    my($app, $userDataSource) = @_;
    # get list of registered contact methods, personal details, settings
    # (also returns state, but outputs can just ignore that)
    return $userDataSource->getFieldsHierarchically($app);
}

sub populateUserPrefsGroupNames {
    my $self = shift;
    my($app, $userDataSource) = @_;
    # get list of groups
    my $groupList = $userDataSource->getGroups($app); # XXX should be cached (used above)
    my $groups = {};
    foreach my $group (@$groupList) {
        $groups->{$group->[0]} = $group->[1];
    }
    return $groups;
}

sub cmdUserPrefsSet {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (not defined($user)) {
        $app->getService('user.login')->requireLogin($app);
        return;
    }

    # get the list of users to edit
    my @userIDs = $app->input->peekArgument('userPrefs.userID');
    if (not @userIDs) {
        # no users selected - so user wants to edit themselves
        @userIDs = ($user->userID);
    } else {
        # one or more users selected
        # let's get rid of the duplicates
        my %userIDs = map { $_ => 1 } @userIDs;
        @userIDs = keys(%userIDs);
    }

    # cache rights (for efficiently reasons as well as to protect
    # ourselves from losing the rights half way through)
    my @rights = $self->populateUserPrefsRights($app, $user);

    # get a handle to the user factory (also for efficiently reasons)
    my $userFactory = $app->getService('user.factory');

    # We will store the messages for the user in @notifications.
    # The structure of this is an array of arrayrefs, each arrayref
    # containing the user ID, the field the notification is related to
    # ('' if the notification related to the user itself), and a
    # string identifying the notification.
    my @notifications = ();
    foreach my $userID (@userIDs) {
        my $targetUser = $userFactory->getUserByID($app, $userID);
        if (defined($targetUser)) {
            push(@notifications, $self->applyUserPrefsChanges($app, $user, $targetUser, $userID, $userID == $user->userID, @rights));
        } else {
            push(@notifications, [$userID, '', 'user.noSuchUser']);
        }
    }

    if (@notifications) {
        # the user needs to be notified of stuff
        # this can include (examples of such notifications in brackets):
        #    request for confirmation of e-mail address ($userID, 'fields.contact.email', 'contact.confirmationSent')
        #    report that last contact methods cannot be removed ($userID, 'fields.contact.email', 'contact.cannotRemoveLastContactMethod')
        #    report that the two new passwords did not match ($userID, 'password', 'password.mismatch.new')
        #    report that the old password was invalid ($userID, 'password', 'password.mismatch.old')
        #    report that a user was not found in the database ($userID, '', 'user.noSuchUser')
        #    report that a change was attempted which is not allowed ($userID, 'adminMessage', 'accessDenied')
        #    report that a group level was invalid ($userID, 'group.4', 'invalid')
        #    report regarding internal errors e.g. unknown fields ($userID, 'fields.profile.homepage', 'field.unknownCategory')
        $app->output->userPrefsNotification(\@notifications);
    } else {
        # it worked ok
        $app->output->userPrefsSuccess();
    }
}

# return 1 if something was attempted which was blocked, but do everything else
# note that if something was blocked you should call $self->warn(2, '...');
sub applyUserPrefsChanges {
    my $self = shift;
    my($app, $user, $targetUser, $targetUserID, $editingUserIsTargetUser,
       $rightPasswords, $rightAdminMessage, $rightMode, $rightContactMethods, $rightPersonalDetails, $rightSettings, $rightGroups, @otherRights) = @_;

    # We'll store any unexpected errors that happen in @notifications.
    # The structure of this is an array of arrayrefs, each arrayref
    # containing the user ID, the field the notification is related to
    # ('' if the notification related to the user itself), and a
    # string identifying the notification.
    my @notifications = ();

    # first, get a lists of all the relevant arguments
    my $arguments = $app->input->getArgumentsBranch("userPrefs.$targetUserID");

    if (defined($arguments->{'password'}) and $arguments->{'password'}->[0] ne '') {
        if ($editingUserIsTargetUser) {
            if (defined($arguments->{'password.old'}) and ($targetUser->checkPassword($arguments->{'password.old'}->[0]))) {
                if (defined($arguments->{'password.confirmation'}) and
                    $arguments->{'password.confirmation'}->[0] eq $arguments->{'password'}->[0]) {
                    $targetUser->password($app->getService('service.passwords')->encryptPassword($arguments->{'password'}->[0]));
                } else {
                    # new passwords don't match
                    push(@notifications, [$targetUserID, 'password', 'password.mismatch.new']);
                }
            } else {
                # old passwords invalid
                push(@notifications, [$targetUserID, 'password', 'password.mismatch.old']);
            }
        } elsif ($rightPasswords) {
            $targetUser->password($app->getService('service.passwords')->encryptPassword($arguments->{'password'}->[0]));
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's password: denied");
            push(@notifications, [$targetUserID, 'password', 'accessDenied']);
        }
    }

    my $userDataSource = $app->getService('dataSource.user');
    foreach my $fieldRow (@{$userDataSource->getFields($app)}) {
        # $field contains [type, fieldID, category, name, typeData]*
        my $fieldID = $fieldRow->[1];
        my $fieldCategory = $fieldRow->[2];
        my $fieldName = $fieldRow->[3];
        my $newValue = $arguments->{"fields.$fieldCategory.$fieldName"};
        if (defined($newValue)) {
            $newValue = $newValue->[0];
            my $field = $targetUser->getFieldByID($fieldID);
            my $oldValue = $field->data;
            if (not defined($oldValue)) {
                $oldValue = '';
            }
            if ($oldValue ne $newValue) {
                push (@notifications,
                      $self->applyUserPrefsFieldChange($app, $user, $targetUser, $targetUserID, $editingUserIsTargetUser, $field, $fieldID, $fieldCategory, $fieldName, $oldValue, $newValue,
                                                       $rightPasswords, $rightAdminMessage, $rightMode, $rightContactMethods, $rightPersonalDetails, $rightSettings, $rightGroups, @otherRights));
            }
        }
    }

    if (defined($arguments->{'adminMessage'})) {
        if ($rightAdminMessage) {
            $targetUser->adminMessage($arguments->{'adminMessage'}->[0]);
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's admin message: denied");
            push(@notifications, [$targetUserID, 'adminMessage', 'accessDenied']);
        }
    }

    if (defined($arguments->{'mode'})) {
        if ($rightAdminMessage) {
            $targetUser->mode($arguments->{'mode'}->[0]);
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's user mode: denied");
            push(@notifications, [$targetUserID, 'mode', 'accessDenied']);
        }
    }

    foreach my $group (@{$userDataSource->getGroups($app)}) {
        # $group contains [groupID, name, [rightName]*]
        my $groupID = $group->[0];
        my $newValue = $arguments->{"groups.$groupID"};
        if (defined($newValue)) {
            $newValue = $newValue->[0];
            if ($newValue =~ /^\d+$/o) {
                my $userLevel = $user->levelInGroup($groupID);
                if (($userLevel > 2) or ($rightGroups)) {
                    $targetUser->joinGroup($groupID, $newValue);
                } elsif ($userLevel == 2) { # if editing user is a group admin # XXX BARE CONSTANT ALERT
                    my $targetUserLevel = $targetUser->levelInGroup($groupID);
                    # if target user is member or below and is being made member or below
                    if (($targetUserLevel < 2) and
                        ($newValue < 2)) { # XXX BARE CONSTANTS ALERT
                        $targetUser->joinGroup($groupID, $newValue);
                    } else {
                        my $userID = $user->userID;
                        $self->warn(2, "user $userID tried to change user $targetUserID's group $groupID permissions from $targetUserLevel to $newValue while at level $userLevel: denied");
                        push(@notifications, [$targetUserID, "groups.$groupID", 'accessDenied']);
                    }
                } else {
                    my $userID = $user->userID;
                    $self->warn(2, "user $userID tried to change user $targetUserID's group $groupID permissions: denied");
                    push(@notifications, [$targetUserID, "groups.$groupID", 'accessDenied']);
                }
            } else {
                push(@notifications, [$targetUserID, "groups.$groupID", 'invalid']);
                my $userID = $user->userID;
                $self->warn(2, "user $userID tried to change user $targetUserID's group $groupID permissions to level $newValue: invalid level");
            }
        }
    }

    return @notifications;
}

# descendants should only call this once they have tried to handle field changes
sub applyUserPrefsFieldChange {
    my $self = shift;
    my($app, $user, $targetUser, $targetUserID, $editingUserIsTargetUser, $field, $fieldID, $fieldCategory, $fieldName, $oldValue, $newValue,
       $rightPasswords, $rightAdminMessage, $rightMode, $rightContactMethods, $rightPersonalDetails, $rightSettings, $rightGroups, @otherRights) = @_;
    # this should return a list of notifications as described above
    if ($fieldCategory eq 'contact') {
        if ($editingUserIsTargetUser) {
            if ($newValue eq '') {
                # old value must be defined (and different) else we couldn't have reached here
                if (scalar(keys(%{$targetUser->fields->{'contact'}})) > 1) {
                    $field->remove();
                } else {
                    return [$targetUserID, "fields.$fieldCategory.$fieldName", 'contact.cannotRemoveLastContactMethod'];
                }
            } else {
                my $field = $targetUser->getField('contact', $fieldName);
                my($overrideCrypt, $overridePassword) = $app->getService('service.passwords')->newPassword();
                my $overrideToken = $targetUser->addFieldChange($field, $field->data, $overrideCrypt, 1); # XXX BARE CONSTANT ALERT
                my($changeCrypt, $changePassword) = $app->getService('service.passwords')->newPassword();
                my $changeToken = $targetUser->addFieldChange($field, $newValue, $changeCrypt, 0); # XXX BARE CONSTANT ALERT
                # send confirmation requests:
                $app->output($fieldName, $targetUser)->userPrefsOverrideDetails($overrideToken, $overridePassword, $oldValue, $newValue);
                $field->returnNewData();
                $app->output($fieldName, $targetUser)->userPrefsChangeDetails($changeToken, $changePassword, $oldValue, $newValue);
                return [$targetUserID, "fields.$fieldCategory.$fieldName", 'contact.confirmationSent'];
            }
        } elsif ($rightContactMethods) {
            if ($newValue eq '') {
                $field->remove();
            } else {
                $field->data($newValue);
            }
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's $fieldCategory.$fieldName field: denied");
            return [$targetUserID, "fields.$fieldCategory.$fieldName", 'accessDenied'];
        }
    } elsif ($fieldCategory eq 'settings') {
        if ($editingUserIsTargetUser or $rightSettings) {
            if ($newValue eq '') {
                $field->remove();
            } else {
                $field->data($newValue);
            }
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's $fieldCategory.$fieldName field: denied");
            return [$targetUserID, "fields.$fieldCategory.$fieldName", 'accessDenied'];
        }
    } elsif ($fieldCategory eq 'personal') {
        if ($editingUserIsTargetUser or $rightPersonalDetails) {
            if ($newValue eq '') {
                $field->remove();
            } else {
                $field->data($newValue);
            }
        } else {
            my $userID = $user->userID;
            $self->warn(2, "user $userID tried to change user $targetUserID's $fieldCategory.$fieldName field: denied");
            return [$targetUserID, "fields.$fieldCategory.$fieldName", 'accessDenied'];
        }
    } else {
        my $userID = $user->userID;
        $self->warn(2, "user $userID tried to change user $targetUserID's $fieldCategory.$fieldName field: unknown category");
        return [$targetUserID, "fields.$fieldCategory.$fieldName", 'field.unknownCategory'];
    }
    return;
}

# dispatcher.commands
sub cmdUserPrefsConfirmSet {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (not defined($user)) {
        $app->getService('user.login')->requireLogin($app);
        return;
    }
    if ($user->performFieldChange($app,
                                  $app->input->getArgument('changeID'),
                                  $app->input->getArgument('changePassword'),
                                  0)) { # XXX
        $app->output->userPrefsSuccess();
    } else {
        $app->output->userPrefsNotification([[$user->userID, 'change', 'accessDenied']]);
    }
}


# dispatcher.output.generic
sub outputUserPrefs {
    my $self = shift;
    my($app, $output, $userIDs, $userData, $metaData, $groupNames) = @_;
    $output->output('userPrefs.index', {
                                        'userIDs' => $userIDs,
                                        'userData' => $userData,
                                        'metaData' => $metaData,
                                        'groupNames' => $groupNames,
                                       });
}

# dispatcher.output.generic
sub outputUserPrefsNotification {
    my $self = shift;
    my($app, $output, $notifications) = @_;
    $output->output('userPrefs.notification', {
                                               'notifications' => $notifications,
                                              });
}

# dispatcher.output.generic
sub outputUserPrefsSuccess {
    my $self = shift;
    my($app, $output) = @_;
    $output->output('userPrefs.success', {});
}

# dispatcher.output.generic
sub outputUserPrefsOverrideDetails {
    my $self = shift;
    my($app, $output, $overrideToken, $overridePassword, $oldValue, $newValue) = @_;
    $output->output('userPrefs.change.overrideDetails', {
                                                         'token' => $overrideToken,
                                                         'password' => $overridePassword,
                                                         'oldAddress' => $oldValue,
                                                         'newAddress' => $newValue,
                                                        });
}

# dispatcher.output.generic
sub outputUserPrefsChangeDetails {
    my $self = shift;
    my($app, $output, $changeToken, $changePassword, $oldValue, $newValue) = @_;
    $output->output('userPrefs.change.changeDetails', {
                                                       'token' => $changeToken,
                                                       'password' => $changePassword,
                                                       'oldAddress' => $oldValue,
                                                       'newAddress' => $newValue,
                                                      });
}

# dispatcher.output
sub strings {
    return (
            'userPrefs.index' => 'The user preferences editor. This will be passed a stack of user profiles in a multi-level hash called userData, the array of userIDs in userIDs, the details of each field in another multi-level hash as metaData, and the mapping of group IDs to group names in groupNames.',
            'userPrefs.notification' => 'If anything needs to be reported after the prefs are submitted then this will be called. Things to notify are reported in notifications, which is an array of arrays containing the userID relatd to the notification, the field related to the notification, and the notification type, which will be one of: contact.confirmationSent (no error occured), contact.cannotRemoveLastContactMethod (user error), password.mismatch.new (user error), password.mismatch.old *user error), user.noSuchUser (internal error), invalid (internal error), field.unknownCategory (internal error), accessDenied (internal error). The internal errors could also be caused by a user attempting to circumvent the security system.',
            'userPrefs.success' => 'If the user preferences are successfully submitted, this will be used. Typically this will simply be the main application command index.',
            'userPrefs.change.overrideDetails' => 'The message that is sent containing the token and password required to override a change of address.',
            'userPrefs.change.changeDetails' => 'The message that is sent containing the token and password required to confirm a change of address.',
            );
}

# setup.install
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $app->output->setupProgress('userPrefs');
    my $userDataSource = $app->getService('dataSource.user');
    # XXX field categories should have generic way of indicating
    # whether the user is allowed to edit either his own or other
    # people's. (Relevant rights are marked XXX below.)
    # users all have the following pseudo-rights:
    #    userPrefs.editSelf.password (requires confirm)
    #    userPrefs.editSelf.contactMethods (requires confirm) # XXX
    #    userPrefs.editSelf.personalDetails # XXX
    #    userPrefs.editSelf.settings # XXX
    # and nobody has:
    #    userPrefs.editSelf.adminMessage
    #    userPrefs.editSelf.mode
    #    userPrefs.editSelf.groups
    $userDataSource->addRight($app, 'userPrefs.editOthers.passwords');
    $userDataSource->addRight($app, 'userPrefs.editOthers.adminMessage');
    $userDataSource->addRight($app, 'userPrefs.editOthers.mode');
    $userDataSource->addRight($app, 'userPrefs.editOthers.contactMethods'); # XXX
    $userDataSource->addRight($app, 'userPrefs.editOthers.personalDetails'); # XXX
    $userDataSource->addRight($app, 'userPrefs.editOthers.settings'); # XXX
    $userDataSource->addRight($app, 'userPrefs.editOthers.groups');
    return;
}
