#!/usr/bonsaitools/bin/perl5 -w
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>

#
# mozbot.pl harrison@netscape.com 10/14/98
# "irc bot for the gang on #mozilla"
#
# features: reports tinderbox status upon request.
# remembers urls. tells you the phase of the moon.
#
# hack on me! required reading:
#
# Net::IRC web page: 
#   http://netirc.betterbox.net/
#   (free software)
#   or get it from CPAN @ http://www.perl.com/CPAN
#
# RFC 1459 (Internet Relay Chat Protocol):
#   http://sunsite.cnlab-switch.ch/ftp/doc/standard/rfc/14xx/1459


$SIG{'INT'}  = 'killed';
$SIG{'KILL'} = 'killed';
$SIG{'TERM'} = 'killed';

use strict 'vars';
use lib ".";
use Net::IRC;
use Tinderbox;
use Carp;

$|++;

my $VERSION = "1.0";

my $debug = 1;

my %cmds = 
    (
    "about" =>  "bot_about",
    "hi" =>         "bot_hi", 
    "moon" =>   "bot_moon",
    "up" =>         "bot_up",
    "trees" =>  "bot_tinderbox",
    "url"   =>      "bot_urls",
    );

@::origargv = @ARGV;

my $server = shift;
my $port = shift;
my $nick = shift;
my $channel = shift;

$server = $server               || "irc.mozilla.org";
$port = $port                   || "6667";
$nick = $nick                   || "mozbot";
$channel = $channel             || "#mozilla";

&debug ("mozbot $VERSION starting up");

# create a pid file if we can

my $pid = "$ENV{'HOME'}/.pid-mozbot";
if (open PID, ">$pid")
    {
    print PID "$$\n";
    close PID;
    }
else
    {
    &debug ("warning: problem creating pid file: $pid, $!");
    }

my $uptime = 0;

# undef $moon if you don't want the moon

# my $moon = "$ENV{'HOME'}/bots/bin/moon";
my $phase;
my $last_moon;

# leave @trees empty if you don't want tinderbox details

my @trees = qw (Mozilla Mozilla-External raptor);
my $trees;
my $last;
my $last_tree;
my %broken;

my @urls;

# the action start here

my $irc = new Net::IRC;

my $bot = $irc->newconn
  (
  Server => $server,
  Port => $port,
  Nick => $nick,
  Ircname => "Thingy",
  Username => $nick,
  )
or die "$0: can't connect to $server, port $port";

&debug ("adding global handlers");
$bot->add_global_handler ([ 251,252,253,254,302,255 ], \&on_startup);
$bot->add_global_handler (376, \&on_connect);
$bot->add_global_handler (433, \&on_nick_taken);
$bot->add_global_handler ([ 'disconnect', 'kill', 474, 465 ], \&on_boot);

&debug ("adding more handlers");
$bot->add_handler ('msg', \&on_msg);
$bot->add_handler('public', \&on_public);

$bot->schedule (0, \&tinderbox);
$bot->schedule (0, \&checksourcechange);

&debug ("connecting to $server $port as $nick on $channel");
$irc->start;

# and done.

################################
# Net::IRC handler subroutines #
################################

sub on_startup
  {
  my ($self, $event) = @_;
  my (@args) = ($event->args);
  shift (@args);

  &debug ("@args\n");
  }

sub on_connect
  {
  my $self = shift;
    &debug ("startup took " . (time - $^T) . " seconds");

    $self->join ($channel);
    
    $uptime = time;
    }

# on_nick_taken: or do something smarter

sub on_nick_taken
    {
    die "hey! somebody took my nick!";
    }

# on_msg: private message received via /msg

sub on_msg
    {
  my ($self, $event) = @_;
  my ($nick) = $event->nick;
  my ($arg) = $event->args;
  my ($cmd, $rest) = split ' ', $arg;
    
    &debug ("msg from $nick: $arg");
    
    foreach (sort keys %cmds)
        {
        if ($_ =~ /^$cmd/i)
            {
            &debug ("command: $_");
            my $msg = &{$cmds{$_}} ($nick, $cmd, $rest);
            if (ref ($msg) eq "ARRAY")
                {
                foreach (@$msg)
                    {
                    $self->privmsg ($nick, $_);
                    }
                }
            else
                {
                $self->privmsg ($nick, $msg);
                }
            return;
            }
        }

    # print help message
    
    $self->privmsg ($nick, "unknown command \"$cmd\". " . &bot_hi ($nick));
    }

sub on_public
    {
  my ($self, $event) = @_;
  my ($to) = $event->to;
    my ($arg) = $event->args;
    my ($nick, $me) = ($event->nick, $self->nick);

    if (my ($cmd, $rest) = $arg =~ /^$me[:,]?\s+(\S+)(?:\s+(.*))?$/i)
        {
        # if this gets any larger, break this
        # out into a command/subroutine hash
    
        if ($cmd =~ /tree/i)
            {
            my $t = &bot_tinderbox (undef, undef, undef, $rest eq "all" ? 0 : 1);
            foreach (@$t)
                {
                next if ($_ eq ".");
                $self->privmsg ($channel, "$_");
                }
            }
        elsif ($cmd =~ /^(hi|hello|lo|sup)/)
            {
            $self->privmsg ($channel, "hi $nick");
            }
        }

        # catch urls, stick them in a list for mozbot's url command

    if ($arg =~ /(http|ftp|gopher):/i && $nick ne $me)
        {
    push @urls, "$arg (" . &logdate() . ")";
    while ($#urls > 10) 
            { shift @urls; }
        }
    }

$::dontQuitOnSignal = 0;
sub on_boot
    {
        if (!$::dontQuitOnSignal) {
            die "$0: disconnected from network";
        }
    }

################
# bot commands #
################

# bot_about: it's either an about box or the 
# address of the guy to blame when the bot 
# breaks

sub bot_about
    {
    return "i am mozbot. hack on me! " .
        "$VERSION harrison\@netscape.com 10/16/98. " .
        "connected to $server since " .
        &bot_up .
        " from $ENV{'HOSTNAME'}";
    }

# bot_hi: list commands, also default function for 
# unknown commands

sub bot_hi
    {
    my ($nick, $cmd, $rest) = @_;

    carp "bot_hi wants nick and optional first word of line, rest of line" 
        unless $nick;
    
    my @cmds = sort keys %cmds;
    return "i am mozbot. i know these commands: @cmds";
    }

# bot_moon: goodnight moon

sub bot_moon
    {
    return "- no moon -" if (! defined $::moon); 
    return $phase if ($phase && (time - $last_moon > (60 * 60 * 24)));
    
    # we only want to run this once/day
    $phase = `$::moon`;
    $last_moon = time;
    return $phase;
    }

# bot_up: report uptime

sub bot_up  
    {
    return &logdate ($uptime) . " (" . &days ($uptime) . ")";
    }

# bot_urls: show last ten urls caught by mozbot

sub bot_urls
    {
    return ($#urls == -1 ? "- mozbot has seen no URLs yet -" : \@urls);
    }

# show tinderbox status
#
# this is a messy little function but it works. 

sub bot_tinderbox
    {
    my ($nick, $cmd, $rest, $terse) = @_;
    my $bustage;
    my $buf;
    my @buf;
    my @tree;
    
    # user can supply a list of trees separated
    # by whitespace, default is all trees

    push @tree, $rest ? (split /\s+/, $rest) : @trees;
    
    # loop through requested trees

    foreach my $t (@tree)
        {
        $bustage = 0;
        $buf = "$t -> ";
        
        # politely report failures
        if (! exists $$trees{$t})
            {
            $buf .= "unknown tree \"$t\", trees include @trees. ";
            }
        else
            {
            foreach my $e (sort keys %{$$trees{$t}})
                {
                next if ($terse && $$trees{$t}{$e} ne "horked");
                $buf .= "[$e: $$trees{$t}{$e}] ";
                $bustage++;
                }
            }
        
        $buf .= "- no known bustage -" if (! $bustage);
        
        push @buf, $buf, ".";
        }

    $buf = $buf || "oh, rats. something broke. complain about this to somebody. ";
    push @buf, "last update: " .
        &logdate ($last_tree) . " (" . &days ($last_tree) . " ago)";
    return \@buf;
    }

#############
# utilities #
#############

sub debug
    {
    return if (! $debug);

    foreach (@_)
        {
        chomp;
        print &logdate() . " $_ [$$]\n";
        }
    }

# logdate: return nice looking date (10/16/98 18:29)

sub logdate
  {
  my $t = shift;
  $t = time unless ($t);
  my ($sec,$min,$hour,$mday,$mon,$year) = localtime ($t);

  return sprintf ("%02d/%02d/%02d %02d:%02d",
    $mon + 1, $mday, $year, $hour, $min);
  }

# days: how long ago was that? 

sub days
  {
  my ($then) = shift;
    
  my $seconds = time - $then;
  my $minutes = int ($seconds / 60);
  my $hours = int ($minutes / 60);
  my $days = int ($hours / 24);

  if ($seconds < 60)
    { return (sprintf "%d second%s", $seconds, $seconds == 1 ? "" : "s"); }
  elsif ($minutes < 60)
    { return (sprintf "%d minute%s", $minutes, $minutes == 1 ? "" : "s"); }
  elsif ($hours < 24)
    { return (sprintf "%d hour%s", $hours, $hours == 1 ? "" : "s"); }
  else
    { return (sprintf "%d day%s", $days, $days == 1 ? "" : "s"); }
  }

# signal handler

sub killed
    {
    confess "i have received a signal of some manner. good night.\n\n";
    }

# fetch tinderbox details

sub tinderbox
    {
    &debug ("fetching tinderbox status");
    my $newtrees;
    ($newtrees, $last) = Tinderbox::status (\@trees);
    $last_tree = time;
    if (defined $trees) {
        foreach my $t (@trees) {
            foreach my $e (sort keys %{$$newtrees{$t}}) {
                if (!defined $$trees{$t}{$e}) {
                    $bot->privmsg($channel, "$t: A new column '$e' has appeared ($$newtrees{$t}{$e})");
                } else {
                    if ($$trees{$t}{$e} ne $$newtrees{$t}{$e}) {
                        $bot->privmsg($channel, "$t: '$e' has changed state from $$trees{$t}{$e} to $$newtrees{$t}{$e}");
                    }
                }
            }
        }
    }
    $trees = $newtrees;
    
    $bot->schedule (360, \&tinderbox);
    }


# See if someone has changed our source.

$::ourdate = 0;
$::tinderboxdate = 0;

sub checksourcechange {
    my ($self) = @_;
    my $lastourdate = $::ourdate;
    my $lasttinderboxdate = $::tinderboxdate;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat("./mozbot.pl");
    $::ourdate = $mtime;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks)
        = stat("./Tinderbox.pm");
    $::tinderboxdate = $mtime;
    if ($lastourdate > 0 && ($::ourdate > $lastourdate ||
                             $::tinderboxdate > $lasttinderboxdate)) {
        $::dontQuitOnSignal = 1;
        $self->quit("someone seems to have changed my source code.  Be right back");
        &debug ("restarting self");
        exec "$0 @::origargv";
    }
    $bot->schedule (60, \&checksourcechange);
}
