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

# dispatcher.commands
sub cmdUserPrefs {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getObject('user');
    if (not defined($user)) {
        $app->getService('user.login')->requireLogin($app);
    }
    my @userIDs = $app->input->peekArgument('userPrefs.userID');
    if (not scalar(@userIDs)) {
        # no users selected - so user wants to edit themselves
        @userIDs = ($user->userID);
    } else {
        # one or more users selected
        # let's get rid of the duplicates
        @userIDs = keys(map { $_ => 1 } @userIDs);
    }
    if ((scalar(@userIDs) > 0) or ($userID[0] <> $user->userID)) {
        if (not $user->hasRight('userPrefs.editOthers')) {
            $app->output->loginInsufficient('userPrefs.editOthers');
            return;
        }
    }
    $app->output->userPrefs(\@userIDs);
    # XXX
}

# dispatcher.output.generic
sub outputUserPrefs {
    my $self = shift;
    my($app, $output, $userIDs) = @_;
    $output->output('userPrefs.index', {
                                        'userIDs' => $userIDs, # XXX
                                       });
}

# dispatcher.output
sub strings {
    return (
            'userPrefs.index' => 'The user prefs index. XXX',
            );
}

# setup.install
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $self->output->setProgress('userPrefs');
    my $userDataSource = $app->getService('dataSource.user');
    $userDataSource->addRight($app, 'userPrefs.editOthers');
    return;
}
