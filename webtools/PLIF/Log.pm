# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# Hixie's Web log system
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

package Log;
use strict;
use vars qw(@ISA);
use PLIF::Service;
use PLIF::DataSource;
use POSIX qw(strftime);
use HTML::Entities;
use SOAP::Lite;
@ISA = qw(PLIF::Service PLIF::DataSource);
1;

# XXX duplication of code is starting... I need to factor stuff out

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'dispatcher.output.generic' or 
            $service eq 'dispatcher.output' or 
            $service eq 'dispatcher.commands' or
            $service eq 'dataSource.log' or
            $service eq 'dataSource.configuration.client' or
            $service eq 'component.log' or
            $service eq 'setup.install' or
            $service eq 'setup.configure' or
            $class->SUPER::provides($service));
}

sub init {
    my $self = shift;
    my($app) = @_;
    $self->SUPER::init(@_);
    $self->name('Unamed Web log');
    $self->canonicalURI('http://log.example.org/');
    $self->description('');
    eval {
        $app->getService('dataSource.configuration')->getSettings($app, $self, 'log');
    };
}

sub cmdShowLog {
    my $self = shift;
    my($app, $start, $count, $order) = @_;
    # get the settings from the arguments if we aren't being forced to
    # use specific settings
    $start ||= $app->input->peekArgument('start');
    $count ||= $app->input->peekArgument('count');
    $order ||= $app->input->peekArgument('order');
    # if we are still without settings, it must be the index page
    my $index = not ($start or $count or $order);
    # in which case, we should use the defaults
    $start ||= time(); # default to now
    $count ||= 10; # default to last 10 posts
    $order ||= -1;
    # get the data from the database
    my $dataSource =  $app->getService('dataSource.log');
    my $posts = $dataSource->getPosts($app, $start, $count);
    my $times = $dataSource->getTimes($app, $start);
    # and use it all to output our ideas
    $app->output->log($index, $start, $count, $order, $posts, $times);
}

sub outputLog {
    my $self = shift;
    my($app, $output, $index, $start, $count, $order, $posts, $times) = @_;
    my $now = time();
    $output->output('log', {
        'now' => $now,
        'nowString' => strftime('%Y-%m-%d %H:%M UTC', gmtime($now)),
        'start' => $start,
        'startString' => strftime('%Y-%m-%d %H:%M UTC', gmtime($start)),
        'count' => $count,
        'order' => $order,
        'posts' => $posts,
        'index' => $index,
        'times' => $times,
    });
}

# this requires you to be logged in
sub cmdAddLog {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'poster');
    if (defined($user)) {
        # update the website
        my $title = $app->input->getArgument('title');
        my $content = $app->input->getArgument('content');
        my $stamp = time();
        $app->getService('dataSource.log')->addPost($app, $title, $content, $stamp);
        # update weblogs.com
        $self->updateWeblogsCom($app);
        # update user
        $app->dispatch('showLog', $stamp, 1);
    }
}

# this requires you to be logged in
sub cmdEditLog {
    my $self = shift;
    my($app) = @_;
    my $user = $app->getService('user.login')->hasRight($app, 'poster');
    if (defined($user)) {
        # update database
        my $id = $app->input->getArgument('id');
        my $title = $app->input->getArgument('title');
        my $content = $app->input->getArgument('content');
        my $dataSource = $app->getService('dataSource.log');
        my $stamp = $dataSource->getStampByID($app, $id);
        $self->assert(defined($stamp), 1, 'Could not find entry to be edited');
        $dataSource->editPost($app, $id, $title, $content);
        # update weblogs.com
        $self->updateWeblogsCom($app);
        # update user
        $app->dispatch('showLog', $stamp, 1);
    }
}

sub updateWeblogsCom {
    my $self = shift;
    my $result = SOAP::Lite # XXX HARD CODED STUFF XXX
        -> service('file:lib/weblogsCom.wsdl')
        -> ping($self->name, $self->canonicalURI);
    if ($result->{'flerror'}) {
        $self->warn(4, 'Failed to update weblogs.com: '.$result->{'message'});
    }
}

# this requires you to be logged in
sub cmdRSS {
    my $self = shift;
    my($app) = @_;
    my $start = time();
    my $count = 10;
    # get the data from the database
    my $dataSource =  $app->getService('dataSource.log');
    my $posts = $dataSource->getPosts($app, $start, $count);
    # and use it all to output our ideas
    $app->output->RSS($start, $count, $posts);
}

sub outputRSS {
    my $self = shift;
    my($app, $output, $start, $count, $posts, $times) = @_;
    $output->output('rss', {
        'start' => $start,
        'startString' => strftime('%Y-%m-%d %H:%M UTC', gmtime($start)),
        'count' => $count,
        'posts' => $posts,
    });
}

# dispatcher.output
sub strings {
    return (
            'log' => 'The main log screen.',
            'rss' => 'The RSS feed.',
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
sub getPosts {
    my $self = shift;
    my($app, $from, $count) = @_;
    $self->assert($count !~ m/[^0-9]/os, 1, 'Internal error: count is nonnumeric');
    my $posts = $self->database($app)->execute("SELECT postID, title, content, stamp FROM posts
                                                WHERE stamp <= ? ORDER BY stamp DESC LIMIT $count", $from)->rows;
    foreach my $post (@$posts) {
        my $description = $post->[2];
        $description =~ s/<[^>]*>//gos;
        $description =~ s/\s+/ /gos;
        decode_entities($description);
        my $content = $post->[2];
        $content =~ s/<\?stamp\?>/$post->[3]/gos;
        $post = {
            'postID' => $post->[0],
            'title' => $post->[1],
            'originalContent' => $post->[2],
            'content' => $content,
            'description' => $description,
            'stamp' => $post->[3], 
            'stampString' => strftime('%Y-%m-%d %H:%M UTC', gmtime($post->[3])),
            'ISOtimestamp' => strftime('%Y-%m-%dT%H:%M:%S+00:00', gmtime($post->[3])),
        };
    }
    return $posts;
}

# dataSource.log
sub getTimes {
    my $self = shift;
    my($app, $center) = @_;
    my $times = $self->database($app)->execute('SELECT stamp, title FROM posts ORDER BY stamp DESC')->rows;
    my $position = 0;
    my $index = 0;
    foreach my $time (@$times) {
        $time = {
            'time' => $time->[0],
            'timeString' => strftime('%Y-%m-%d %H:%M UTC', gmtime($time->[0])),
            'dateString' => strftime('%Y-%m-%d', gmtime($time->[0])),
            'title' => $time->[1],
        };
        if ($time->{'time'} < $center) {
            $time->{'position'} = ++$position;
        } else {
            ++$index;
        }
    }
    foreach my $time (@$times) {
        if (--$index <= 0) {
            $time->{'position'} = 0;
            last;
        }
        $time->{'position'} = -1 * $index;
    }
    return $times;
}

# dataSource.log
sub addPost {
    my $self = shift;
    my($app, $title, $content, $stamp) = @_;
    $self->database($app)->execute('INSERT INTO posts SET title=?, content=?, stamp=?', $title, $content, $stamp);
}

# dataSource.log
sub editPost {
    my $self = shift;
    my($app, $id, $title, $content) = @_;
    $self->assert($id !~ m/[^0-9]/os, 1, 'Internal error: id is nonnumeric');
    return $self->database($app)->execute("UPDATE posts SET title=?, content=? WHERE postID = $id", $title, $content)->rowsAffected;
}

# dataSource.log
sub getStampByID {
    my $self = shift;
    my($app, $id) = @_;
    $self->assert($id !~ m/[^0-9]/os, 1, 'Internal error: id is nonnumeric');
    return $self->database($app)->execute("SELECT stamp FROM posts WHERE postID = $id")->row;
}

# dataSource.log
sub setupInstall {
    my $self = shift;
    my($app) = @_;
    $app->output->setupProgress('component.log');
    my $helper = $self->helper($app);
    my $database = $self->database($app);
    if (not $helper->tableExists($app, $database, 'posts')) {
        $app->output->setupProgress('dataSource.log.posts');
        $database->execute('
            CREATE TABLE posts (
                                postID integer unsigned auto_increment NOT NULL PRIMARY KEY,
                                title text NOT NULL,
                                content text NOT NULL,
                                stamp integer unsigned NOT NULL DEFAULT 0
                               )
        ');
    } else {
        # check its schema is up to date
    }
    my $userDataSource = $app->getService('dataSource.user');
    $userDataSource->addRight($app, 'poster');
    return;
}

sub setupConfigure {
    my $self = shift;
    my($app) = @_;
    $app->output->setupProgress('component.log.settings');

    # get the name of the log
    my $name = $app->input->getArgument('log.name', $self->name);
    if (not defined($name)) {
        return 'log.name';
    }
    # set the name of the log
    $self->name($name);

    # get the uri of the log
    my $uri = $app->input->getArgument('log.canonicalURI', $self->canonicalURI);
    if (not defined($uri)) {
        return 'log.canonicalURI';
    }
    # set the uri of the log
    $self->canonicalURI($uri);

    # get the description of the log
    my $description = $app->input->getArgument('log.description', $self->description);
    if (not defined($description)) {
        return 'log.description';
    }
    # set the description of the log
    $self->description($description);

    # save the name, uri and description of the log
    $app->getService('dataSource.configuration')->setSettings($app, $self, 'log');
    return;

}

# dataSource.configuration.client
sub settings {
    # if you change this, check out setupConfigure to make sure it is still up to date
    return qw(name canonicalURI description);
}
