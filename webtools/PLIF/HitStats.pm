# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# PLIF Hit Counter
#
# Copyright (c) 2002 by Ian Hickson
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

package HitStats;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use PLIF::DataSource;
@ISA = qw(PLIF::Service PLIF::DataSource);
1;

# XXX duplication of code is starting... I need to factor stuff out

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dispatcher.output.generic' or 
            $service eq 'dispatcher.output' or 
            $service eq 'dispatcher.commands' or
            $service eq 'dataSource.hitStats' or
            $service eq 'component.hitStats' or
            $service eq 'setup.install' or
            (defined($ENV{'HTTP_REFERER'}) and $service eq 'input.verify') or
            $class->SUPER::provides($service));
}

# input.verify
sub verifyInput {
    my $self = shift;
    my($app) = @_;
    # XXX get metadata from input
    my $source = $ENV{'HTTP_REFERER'};
    my $target = "http://$ENV{'HTTP_HOST'}$ENV{'REQUEST_URI'}";
    if (defined($source) and $source ne '' and defined($target) and $target ne '') {
        if ($app->getService('dataSource.hitStats')->add($app, $source, $target)) {
            eval {
                $app->output('email', $app->getService('user.factory')->getUserByID($app, 1))->newReferrer($source, $target);
            };
            if ($@) {
                $self->warn(4, "Error while sending e-mail about a new referrer: $@");
            }
        }
    }
    return; # nope, nothing to see here... (no error, anyway)
}

sub cmdShowStats {
    my $self = shift;
    my($app) = @_;
    $app->output->hitStats($app->getService('dataSource.log')->getHitStats($app));
}

sub outputHitStats {
    my $self = shift;
    my($app, $output, $data) = @_;
    $output->output('hitStats', {
        'hitStats' => $data,
    });
}

sub outputNewReferrer {
    my $self = shift;
    my($app, $output, $source, $target) = @_;
    $output->output('newReferrer', {
        'source' => $source,
        'target' => $target,
    });
}

# dispatcher.output
sub strings {
    return (
            'hitStats' => 'A list of referrers.',
            );
}

# dataSource.log
sub databaseName {
    return 'default';
}

# dataSource.log
sub databaseType {
    return qw(mysql);
}

# dataSource.log
sub add {
    my $self = shift;
    my($app, $source, $target) = @_;
    # sanitise and untaint
    foreach ($source, $target) {
        s/\`/hex(ord('`'))/geos;
        m/^(.*)$/os;
        $_ = $1;
    }
    eval { $self->database($app)->execute("INSERT INTO hitStats SET source=?, target=?", $source, $target) };
    my $new = not $@;
    $self->database($app)->execute("UPDATE hitStats SET count=count+1 WHERE source = ? AND target = ?", $source, $target);        
    return $new;
}

# dataSource.log
sub getHitStats {
    my $self = shift;
    my($app) = @_;
    my $stats = $self->database($app)->execute('SELECT source, target, count FROM hitStats ORDER BY count')->rows;
    foreach my $stat (@$stats) {
        $stat = {
            'source' => $stat->[0],
            'target' => $stat->[1], 
            'count' => $stat->[2],
        };
    }
    return $stats;
}

# dataSource.log
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $app->output->setupProgress('component.log');
    my $helper = $self->helper($app);
    my $database = $self->database($app);

    if (not $helper->tableExists($app, $database, 'hitStats')) {
        $app->output->setupProgress('dataSource.hitStats.hitStats');
        $database->execute('
            CREATE TABLE hitStats (
                                source VARCHAR(128) NOT NULL,
                                target VARCHAR(128) NOT NULL,
                                count integer unsigned NOT NULL DEFAULT 0,
                                PRIMARY KEY (source, target)
                               )
        ');
    } else {
        # check its schema is up to date
    }

    # XXX should ask which user to e-mail, and store user id in .PLIF
    # XXX should add pref so that we know what contact method the user wants

    return;
}

