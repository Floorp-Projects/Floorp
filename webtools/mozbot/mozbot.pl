#!/usr/bin/perl -wT
# -*- Mode: perl; indent-tabs-mode: nil -*-
# DO NOT REMOVE THE -T ON THE FIRST LINE!!!
#
#                       _           _
#        m o z i l l a |.| o r g   | |
#    _ __ ___   ___ ___| |__   ___ | |_
#   | '_ ` _ \ / _ \_  / '_ \ / _ \| __|
#   | | | | | | (_) / /| |_) | (_) | |_
#   |_| |_| |_|\___/___|_.__/ \___/ \__|
#   ====================================
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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>
#                 Risto Kotalampi <risto@kotalampi.com>
#                 Josh Soref <timeless@bemail.org>
#                 Ian Hickson <mozbot@hixie.ch>
#                 Ken Coar <ken.coar@golux.com>
#                 Adam Di Carlo <adam@onshored.com>
#
# mozbot.pl harrison@netscape.com 1998-10-14
# "irc bot for the gang on #mozilla"
#
# mozbot.pl mozbot@hixie.ch 2000-07-04
# "irc bot engine for anyone" :-)
#
# hack on me! required reading:
#
# Net::IRC web page:
#   http://sourceforge.net/projects/net-irc/
#   (free software)
#   or get it from CPAN @ http://www.perl.com/CPAN
#
# RFC 1459 (Internet Relay Chat Protocol):
#   http://sunsite.cnlab-switch.ch/ftp/doc/standard/rfc/14xx/1459
#
# Please file bugs in Bugzilla, under the 'Webtools' product,
# component 'Mozbot'.  http://bugzilla.mozilla.org/

# TO DO LIST
# XXX Something that checks modules that failed to compile and then
#     reloads them when possible
# XXX an HTML entity convertor for things that speak web page contents
# XXX UModeChange
# XXX minor checks
# XXX throttle nick changing and away setting (from module API)
# XXX compile self before run
# XXX parse mode (+o, etc)
# XXX customise gender
# XXX optimisations
# XXX maybe should catch hangup signal and go to background?
# XXX protect the bot from DOS attacks causing server overload
# XXX protect the server from an overflowing log (add log size limitter
#     or rotation)
# XXX fix the "hack hack hack" bits to be better.


################################
# Initialisation               #
################################

# -- #mozwebtools was here --
#      <Hixie> syntax error at oopsbot.pl line 48, near "; }"
#      <Hixie> Execution of oopsbot.pl aborted due to compilation errors.
#      <Hixie> DOH!
#     <endico> hee hee. nice smily in the error message

# catch nasty occurances
$SIG{'INT'}  = sub { &killed('INT'); };
$SIG{'KILL'} = sub { &killed('KILL'); };
$SIG{'TERM'} = sub { &killed('TERM'); };

# this allows us to exit() without shutting down (by exec($0)ing)
BEGIN { exit() if ((defined($ARGV[0])) and ($ARGV[0] eq '--abort')); }

# pragmas
use strict;
use diagnostics;

# chroot if requested
my $CHROOT = 0;
if ((defined($ARGV[0])) and ($ARGV[0] eq '--chroot')) {
    # chroot
    chroot('.') or die "chroot failed: $!\nAborted";
    # setuid
    # This is hardcoded to use user ids and group ids 60001.
    # You'll want to change this on your system.
    $> = 60001; # setuid nobody
    $) = 60001; # setgid nobody
    shift(@ARGV);
    use lib '/lib';
    $CHROOT = 1;
} elsif ((defined($ARGV[0])) and ($ARGV[0] eq '--assume-chrooted')) {
    shift(@ARGV);
    use lib '/lib';
    $CHROOT = 1;
} else {
    use lib 'lib';
}

# important modules
use Net::IRC 0.7; # 0.7 is not backwards compatible with 0.63 for CTCP responses
use IO::SecurePipe; # internal based on IO::Pipe
use IO::Select;
use Socket;
use POSIX ":sys_wait_h";
use Carp qw(cluck confess);
use Configuration; # internal
use Mails; # internal

# Note: Net::SMTP is also used, see the sendmail function in Mails.

# force flushing
$|++;

# internal 'constants'
my $USERNAME = "pid-$$";
my $LOGFILEPREFIX;

# variables that should only be changed if you know what you are doing
my $LOGGING = 1; # set to '0' to disable logging
my $LOGFILEDIR; # set this to override the logging output directory

if ($LOGGING) {
    # set up the log directory
    unless (defined($LOGFILEDIR)) {
        if ($CHROOT) {
            $LOGFILEDIR = '/log';
        } else {
            # setpwent doesn't work on Windows, we should wrap this in some OS test
            setpwent; # reset the search settings for the getpwuid call below
            $LOGFILEDIR = (getpwuid($<))[7].'/log';
        }
    }
    "$LOGFILEDIR/$0" =~ /^(.*)$/os; # untaints the evil $0.
    $LOGFILEPREFIX = $1; # for some reason, $0 is considered tainted here, but not in other cases...
    mkdir($LOGFILEDIR, 0700); # if this fails for a bad reason, we'll find out during the next line
}

# begin session log...
&debug('-'x80);
&debug('mozbot starting up');
&debug('compilation took '.&days($^T).'.');
if ($CHROOT) {
    &debug('mozbot chroot()ed successfully');
}

# secure the environment
#
# XXX could automatically remove the current directory here but I am
# more comfortable with people knowing it is not allowed -- see the
# README file.
if ($ENV{'PATH'} =~ /^(?:.*:)?\.?(?::.*)?$/os) {
    die 'SECURITY RISK. You cannot have \'.\' in the path. See the README. Aborted';
}
$ENV{'PATH'} =~ /^(.*)$/os;
$ENV{'PATH'} = $1; # we have to assume their path is otherwise safe, they called us!
delete (@ENV{'IFS', 'CDPATH', 'ENV', 'BASH_ENV'});


# read the configuration file
my $cfgfile = shift || "$0.cfg";
$cfgfile =~ /^(.*)$/os;
$cfgfile = $1; # untaint it -- we trust this, it comes from the admin.
&debug("reading configuration from '$cfgfile'...");

# - setup variables
# note: owner is only used by the Mails module
my ($server, $port, $password, $localAddr, @nicks, @channels, %channelKeys, $owner,
    @ignoredUsers, @ignoredTargets);
my $nick = 0;
my $sleepdelay = 60;
my $connectTimeout = 120;
my $delaytime = 1.3; # amount of time to wait between outputs
my $recentMessageCountLimit = 5; # upper limit
my $recentMessageCountDecrementRate = 0.2; # how much to take off per $delaytime
my $recentMessageCountPenalty = 10; # if we hit the limit, bump it up by this much
my $variablepattern = '[-_:a-zA-Z0-9]+';
my %users = ('admin' => &newPassword('password')); # default password for admin
my %userFlags = ('admin' => 3); # bitmask; 0x1 = admin, 0x2 = delete user a soon as other admin authenticates
my $helpline = 'http://www.mozilla.org/projects/mozbot/'; # used in IRC name and in help
my $serverRestrictsIRCNames = '';
my $serverExpectsValidUsername = '';
my $username = 0; # makes the username default to the pid ($USERNAME)
my @modulenames = ('General', 'Greeting', 'Infobot', 'Parrot');

# - which variables can be saved.
&registerConfigVariables(
    [\$server, 'server'],
    [\$port, 'port'],
    [\$password, 'password'],
    [\$localAddr, 'localAddr'],
    [\@nicks, 'nicks'],
    [\$nick, 'currentnick'], # pointer into @nicks
    [\@channels, 'channels'],
    [\%channelKeys, 'channelKeys'],
    [\@ignoredUsers, 'ignoredUsers'],
    [\@ignoredTargets, 'ignoredTargets'],
    [\@modulenames, 'modules'],
    [\$owner, 'owner'],
    [\$sleepdelay, 'sleep'],
    [\$connectTimeout, 'connectTimeout'],
    [\$delaytime, 'throttleTime'],
    [\%users, 'users'], # usernames => &newPassword(passwords)
    [\%userFlags, 'userFlags'], # usernames => bits
    [\$variablepattern, 'variablepattern'],
    [\$helpline, 'helpline'],
    [\$username, 'username'],
    [\$serverRestrictsIRCNames, 'simpleIRCNameServer'],
    [\$serverExpectsValidUsername, 'validUsernameServer'],
    [\$Mails::smtphost, 'smtphost'],
);

# - read file
&Configuration::Get($cfgfile, &configStructure()); # empty gets entire structure

# - check variables are ok
# note. Ensure only works on an interactive terminal (-t).
# It will abort otherwise.
{ my $changed; # scope this variable
$changed = &Configuration::Ensure([
    ['Connect to which server?', \$server],
    ['To which port should I connect?', \$port],
    ['What is the server\'s password? (Leave blank if there isn\'t one.)', \$password],
    ['What channels should I join?', \@channels],
    ['What is the e-mail address of my owner?', \$owner],
    ['What is your SMTP host?', \$Mails::smtphost],
]);

# - check we have some nicks
until (@nicks) {
    $changed = &Configuration::Ensure([['What nicks should I use? (I need at least one.)', \@nicks]]) || $changed;
    # the original 'mozbot 2.0' development codename (and thus nick) was oopsbot.
}

# - check current nick pointer is valid
# (we assume that no sillyness has happened with $[ as,
# according to man perlvar, "Its use is highly discouraged".)
$nick = 0 if (($nick > $#nicks) or ($nick < 0));

# - check channel names are all lowercase
foreach (@channels) { $_ = lc; }

# save configuration straight away, to make sure it is possible and to save
# any initial settings on the first run, if anything changed.
if ($changed) {
    &debug("saving configuration to '$cfgfile'...");
    &Configuration::Save($cfgfile, &configStructure());
}

} # close the scope for the $changed variable

# ensure Mails is ready
&debug("setting up Mails module...");
$Mails::debug = \&debug;
$Mails::owner = \$owner;

# setup the IRC variables
&debug("setting up IRC variables...");
my $uptime;
my $irc = new Net::IRC or confess("Could not create a new Net::IRC object. Aborting");

# connect
&debug("attempting initial connection...");
&connect(); # hmm.

# setup the modules array
my @modules; # we initialize it lower down (at the bottom in fact)
my $lastadmin; # nick of last admin to be seen
my %authenticatedUsers; # hash of user@hostname=>users who have authenticated


################################
# Net::IRC handler subroutines #
################################

sub setEventArgs {
    my $event = shift;
    if ($Net::IRC::VERSION == 0.75) {
        # curses. This version of Net::IRC is broken. Work around
        # it here.
        return $event->args(\@_);
    } else {
        return $event->args(@_);
    }
}

my $lastNick;

# setup connection
sub connect {
    $uptime = time();

    &debug("connecting to $server:$port using nick '$nicks[$nick]'...");

    my ($bot, $mailed);

    $lastNick = undef;

    my $ircname = 'mozbot';
    if ($serverRestrictsIRCNames ne $server) {
        $ircname = "[$ircname] $helpline";
    }

    my $identd = getpwuid($<);
    if ($serverExpectsValidUsername ne $server) {
        $identd = $username || $USERNAME;
    }

    until (inet_aton($server) and # we check this first because Net::IRC::Connection doesn't
           $bot = $irc->newconn(
             Server => $server,
             Port => $port,
             Password => $password ne '' ? $password : undef, # '' will cause PASS to be sent
             Nick => $nicks[$nick],
             Ircname => $ircname,
             Username => $identd,
             LocalAddr => $localAddr,
           )) {
        &debug("Could not connect. Are you sure '$server:$port' is a valid host?");
        unless (inet_aton($server)) {
            &debug('I couldn\'t resolve it.');
        }
        if (defined($localAddr)) {
            &debug("Is '$localAddr' the correct address of the interface to use?");
        } else {
            &debug("Try editing '$cfgfile' to set 'localAddr' to the address of the interface to use.");
        }
        if ($Net::IRC::VERSION < 0.73) {
            &debug("Note that to use 'localAddr' you need Net::IRC version 0.73 or higher (you have $Net::IRC::VERSION)");
        }
        $mailed = &Mails::ServerDown($server, $port, $localAddr, $nicks[$nick], $ircname, $identd) unless $mailed;
        sleep($sleepdelay);
        &Configuration::Get($cfgfile, &configStructure(\$server, \$port, \$password, \@nicks, \$nick, \$owner, \$sleepdelay));
        &debug("connecting to $server:$port again...");
    }

    &debug("connected! woohoo!");

    # add the handlers
    &debug("adding event handlers");

    # $bot->debug(1); # this can help when debugging API stuff

    $bot->add_global_handler([ # Informational messages -- print these to the console
        251, # RPL_LUSERCLIENT
        252, # RPL_LUSEROP
        253, # RPL_LUSERUNKNOWN
        254, # RPL_LUSERCHANNELS
        255, # RPL_LUSERME
        302, # RPL_USERHOST
        375, # RPL_MOTDSTART
        372, # RPL_MOTD
    ], \&on_startup);

    $bot->add_global_handler([ # Informational messages -- print these to the console
        'snotice', # server notices
        461, # need more arguments for PASS command
        409, # noorigin
        405, # toomanychannels  XXX should do something about this!
        404, # cannot send to channel
        403, # no such channel
        401, # no such server
        402, # no such nick
        407, # too many targets
    ], \&on_notice);

    $bot->add_global_handler([ # should only be one command here - when to join channels
        376, # RPL_ENDOFMOTD
        422, # nomotd
    ], \&on_connect);

    $bot->add_handler('welcome', \&on_set_nick); # when we connect, to get our nick
    $bot->add_global_handler([ # when to change nick name
        'erroneusnickname',
        433, # ERR_NICKNAMEINUSE
        436, # nick collision
    ], \&on_nick_taken);
    $bot->add_handler('nick', \&on_nick); # when someone changes nick

    $bot->add_global_handler([ # when to give up and go home
        'disconnect', 'kill', # bad connection, booted offline
        465, # ERR_YOUREBANNEDCREEP
    ], \&on_disconnected);
    $bot->add_handler('destroy', \&on_destroy); # when object is GCed.

    $bot->add_handler('msg', \&on_private); # /msg bot hello
    $bot->add_handler('public', \&on_public);  # hello
    $bot->add_handler('notice', \&on_noticemsg); # notice messages
    $bot->add_handler('join', \&on_join); # when someone else joins
    $bot->add_handler('part', \&on_part); # when someone else leaves
    $bot->add_handler('topic', \&on_topic); # when topic changes in a channel
    $bot->add_handler('notopic', \&on_topic); # when topic in a channel is cleared
    $bot->add_handler('invite', \&on_invite); # when someone invites us
    $bot->add_handler('quit', \&on_quit); # when someone quits IRC
    $bot->add_handler('kick', \&on_kick); # when someone (or us) is kicked
    $bot->add_handler('mode', \&on_mode); # when modes change
    $bot->add_handler('umode', \&on_umode); # when modes of user change (by IRCop or ourselves)
    # XXX could add handler for 474, # ERR_BANNEDFROMCHAN

    $bot->add_handler([ # ones we handle to get our hostmask
        311, # whoisuser
    ], \&on_whois);
    $bot->add_handler([ # ones we handle just by outputting to the console
        312, # whoisserver
        313, # whoisoperator
        314, # whowasuser
        315, # endofwho
        316, # whoischanop
        317, # whoisidle
        318, # endofwhois
        319, # whoischannels
    ], \&on_notice);
    $bot->add_handler([ # names (currently just ignored)
        353, # RPL_NAMREPLY "<channel> :[[@|+]<nick> [[@|+]<nick> [...]]]"
    ], \&on_notice);
    $bot->add_handler([ # end of names (we use this to establish that we have entered a channel)
        366, # RPL_ENDOFNAMES "<channel> :End of /NAMES list"
    ], \&on_join_channel);

    $bot->add_handler('cping', \&on_cping); # client to client ping
    $bot->add_handler('crping', \&on_cpong); # client to client ping (response)
    $bot->add_handler('cversion', \&on_version); # version info of mozbot.pl
    $bot->add_handler('csource', \&on_source); # where is mozbot.pl's source
    $bot->add_handler('caction', \&on_me); # when someone says /me
    $bot->add_handler('cgender', \&on_gender); # guess

    $bot->schedule($connectTimeout, \&on_check_connect);

    # and done.
    &Mails::ServerUp($server) if $mailed;

}

# called when the client receives a startup-related message
sub on_startup {
    my ($self, $event) = @_;
    my (@args) = $event->args;
    shift(@args);
    &debug(join(' ', @args));
}

# called when the client receives a server notice
sub on_notice {
    my ($self, $event) = @_;
    &debug($event->type.': '.join(' ', $event->args));
}

# called when the client receives whois data
sub on_whois {
    my ($self, $event) = @_;
    &debug('collecting whois information: '.join('|', $event->args));
    # XXX could cache this information and then autoop people from
    # the bot's host, or whatever
}

my ($nickFirstTried, $nickHadProblem, $nickProblemEscalated) = (0, 0, 0);

# this is called both for the welcome message (001) and by the on_nick handler
sub on_set_nick {
    my ($self, $event) = @_;
    ($lastNick) = $event->args; # (args can be either array or scalar, we want the first value)
    # Find nick's index.
    my $newnick = 0;
    $newnick++ while (($newnick < @nicks) and ($lastNick ne $nicks[$newnick]));
    # If nick isn't there, add it.
    if ($newnick >= @nicks) {
        push(@nicks, $lastNick);
    }
    # set variable
    $nick = $newnick;
    &debug("using nick '$nicks[$nick]'");

    # try to get our hostname
    $self->whois($nicks[$nick]);

    if ($nickHadProblem) {
        Mails::NickOk($nicks[$nick]) if $nickProblemEscalated;
        $nickHadProblem = 0;
    }

    # save
    &Configuration::Save($cfgfile, &::configStructure(\$nick, \@nicks));
}

sub on_nick_taken {
    my ($self, $event, $nickSlept) = @_, 0;
    return unless $self->connected();

    if ($event->type eq 'erroneusnickname') {
        my ($currentNick, $triedNick, $err) = $event->args; # current, tried, errmsg
        &debug("requested nick ('$triedNick') refused by server ('$err')");
    } elsif ($event->type eq 'nicknameinuse') {
        my ($currentNick, $triedNick, $err) = $event->args; # current, tried, errmsg
        &debug("requested nick ('$triedNick') already in use ('$err')");
    } else {
        my $type = $event->type;
        my $args = join(' ', $event->args);
        &debug("message $type from server: $args");
    }

    if (defined $lastNick) {
        &debug("silently abandoning nick change idea :-)");
        return;
    }

    # at this point, we don't yet have a nick, but we need one
    
    if ($nickSlept) {
        &debug("waited for a bit -- reading $cfgfile then searching for a nick...");
        &Configuration::Get($cfgfile, &configStructure(\@nicks, \$nick));
        $nick = 0 if ($nick > $#nicks) or ($nick < 0); # sanitise
        $nickFirstTried = $nick;
    } else {
        if (not $nickHadProblem) {
            $nickHadProblem = 1;
            $nickFirstTried = $nick;
        }
        ++$nick;
        $nick = 0 if $nick > $#nicks; # sanitise

        if ($nick == $nickFirstTried) {
            # looped!
            local $" = ", ";
            &debug("could not find an acceptable nick");
            &debug("nicks tried: @nicks");

            if (not -t) {
                &debug("edit $cfgfile to add more nicks *hint* *hint*");
                $nickProblemEscalated ||= # only e-mail once (returns 0 on failure)
                  Mails::NickShortage($cfgfile, $self->server, $self->port,
                                      $self->username, $self->ircname, @nicks)
                &debug("going to wait $sleepdelay seconds so as not to overload ourselves.");
                $self->schedule($sleepdelay, \&on_nick_taken, $event, 1); # try again
                return; # otherwise we no longer respond to pings.
            }

            # else, we're terminal bound, ask user for nick
            print "Please suggest a nick (blank to abort): ";
            my $new = <>;
            chomp($new);
            if (not $new) {
                &debug("Could not find an acceptable nick");
                exit(1);
            }
            # XXX this could introduce duplicates
            @nicks = (@nicks[0..$nickFirstTried], $new, @nicks[$nickFirstTried+1..$#nicks]);
            $nick += 1; # try the new nick now
            $nickFirstTried = $nick;
        }
    }

    &debug("now going to try nick '$nicks[$nick]'");
    &Configuration::Save($cfgfile, &configStructure(\$nick, \@nicks));
    $self->nick($nicks[$nick]);
}

# called when we connect.
sub on_connect {
    my $self = shift;

    if (defined($self->{'__mozbot__shutdown'})) { # HACK HACK HACK
        &debug('Uh oh. I connected anyway, even though I thought I had timed out.');
        &debug('I\'m going to increase the timeout time by 20%.');
        $connectTimeout = $connectTimeout * 1.2;
        &Configuration::Save($cfgfile, &configStructure(\$connectTimeout));
        $self->quit('having trouble connecting, brb...');
        # XXX we don't call the SpottedQuit handlers here
        return;
    }

    # -- #mozwebtools was here --
    # *** oopsbot (oopsbot@129.59.231.42) has joined channel #mozwebtools
    # *** Mode change [+o oopsbot] on channel #mozwebtools by timeless
    #   <timeless> wow an oopsbot!
    # *** Signoff: oopsbot (oopsbot@129.59.231.42) has left IRC [Leaving]
    #   <timeless> um
    #   <timeless> not very stable.

    # now load all modules
    my @modulesToLoad = @modulenames;
    @modules = (BotModules::Admin->create('Admin', '')); # admin commands
    @modulenames = ('Admin');
    foreach (@modulesToLoad) {
        next if $_ eq 'Admin'; # Admin is static and is installed manually above
        my $result = LoadModule($_);
        if (ref($result)) {
            &debug("loaded $_");
        } else {
            &debug("failed to load $_", $result);
        }
    }

    # mass-configure the modules
    &debug("loading module configurations...");
    { my %struct; # scope this variable
    foreach my $module (@modules) { %struct = (%struct, %{$module->configStructure()}); }
    &Configuration::Get($cfgfile, \%struct);
    } # close the scope for the %struct variable

    # tell the modules they have joined IRC
    my $event = newEvent({
        'bot' => $self,
    });
    foreach my $module (@modules) {
        $module->JoinedIRC($event);
    }

    # join the channels
    &debug('going to join: '.join(',', @channels));
    foreach my $channel (@channels) {
        if (defined($channelKeys{$channel})) {
            $self->join($channel, $channelKeys{$channel});
        } else {
            $self->join($channel);
        }
    }
    @channels = ();

    # tell the modules to set up the scheduled commands
    &debug('setting up scheduler...');
    foreach my $module (@modules) {
        eval {
            $module->Schedule($event);
        };
        if ($@) {
            &debug("Warning: An error occured while loading the module:\n$@");
        }
    }

    # enable the drainmsgqueue
    &drainmsgqueue($self);
    $self->schedule($delaytime, \&lowerRecentMessageCount);

    # signal that we are connected (see next two functions)
    $self->{'__mozbot__active'} = 1; # HACK HACK HACK

    # all done!
    &debug('initialisation took '.&days($uptime).'.');
    $uptime = time();

}

sub on_check_connect {
    my $self = shift;
    return if (defined($self->{'__mozbot__shutdown'}) or defined($self->{'__mozbot__active'})); # HACK HACK HACK
    $self->{'__mozbot__shutdown'} = 1; # HACK HACK HACK
    &debug("connection timed out -- trying again");
    # XXX we don't call the SpottedQuit handlers here
    foreach (@modules) { $_->unload(); }
    @modules = ();
    $self->quit('connection timed out -- trying to reconnect');
    &connect();
}

# if something nasty happens
sub on_disconnected {
    my ($self, $event) = @_;
    return if defined($self->{'__mozbot__shutdown'}); # HACK HACK HACK
    $self->{'__mozbot__shutdown'} = 1; # HACK HACK HACK
    # &do(@_, 'SpottedQuit'); # XXX do we want to do this?
    my($reason) = $event->args;
    if ($reason =~ /Connection timed out/osi
        and ($serverRestrictsIRCNames ne $server
             or $serverExpectsValidUsername ne $server)) {
        # try to set everything up as simple as possible
        $serverRestrictsIRCNames = $server;
        $serverExpectsValidUsername = $server;
        &Configuration::Save($cfgfile, &configStructure(\$serverRestrictsIRCNames));
        &debug("Hrm, $server is having issues.");
        &debug("We're gonna try again with different settings, hold on.");
        &debug("The full message from the server was: '$reason'");
    } elsif ($reason =~ /Bad user info/osi and $serverRestrictsIRCNames ne $server) {
        # change our IRC name to something simpler by setting the flag
        $serverRestrictsIRCNames = $server;
        &Configuration::Save($cfgfile, &configStructure(\$serverRestrictsIRCNames));
        &debug("Hrm, $server didn't like our IRC name.");
        &debug("Trying again with a simpler one, hold on.");
        &debug("The full message from the server was: '$reason'");
    } elsif ($reason =~ /identd/osi and $serverExpectsValidUsername ne $server) {
        # try setting our username to the actual username
        $serverExpectsValidUsername = $server;
        &Configuration::Save($cfgfile, &configStructure(\$delaytime));
        &debug("Hrm, $server said something about an identd problem.");
        &debug("Trying again with our real username, hold on.");
        &debug("The full message from the server was: '$reason'");
    } elsif ($reason =~ /Excess Flood/osi) {
        # increase the delay by 20%
        $delaytime = $delaytime * 1.2;
        &Configuration::Save($cfgfile, &configStructure(\$delaytime));
        &debug('Hrm, we it seems flooded the server. Trying again with a delay 20% longer.');
        &debug("The full message from the server was: '$reason'");
    } elsif ($reason =~ /Bad Password/osi) {
        &debug('Hrm, we don\'t seem to know the server password.');
        &debug("The full message from the server was: '$reason'");
        if (-t) {
            print "Please enter the server password: ";
            $password = <>;
            chomp($password);
            &Configuration::Save($cfgfile, &configStructure(\$password));
        } else {
            &debug("edit $cfgfile to set the password *hint* *hint*");
            &debug("going to wait $sleepdelay seconds so as not to overload ourselves.");
            sleep $sleepdelay;
        }
    } else {
        &debug("eek! disconnected from network: '$reason'");
    }
    foreach (@modules) { $_->unload(); }
    @modules = ();
    &connect();
}

# on_join_channel: called when we join a channel
sub on_join_channel {
    my ($self, $event) = @_;
    my ($nick, $channel) = $event->args;
    $channel = lc($channel);
    push(@channels, $channel);
    &Configuration::Save($cfgfile, &configStructure(\@channels));
    &debug("joined $channel, about to autojoin modules...");
    foreach (@modules) {
        $_->JoinedChannel(newEvent({
            'bot' => $self,
            'channel' => $channel,
            'target' => $channel,
            'nick' => $nick
        }), $channel);
    }
}

# if something nasty happens
sub on_destroy {
    &debug("Connection: garbage collected");
}

sub targetted {
    my ($data, $nick) = @_;
    return $data =~ /^(\s*$nick(?:[\s,:;.!?]+|\s*:-\s*|\s*--+\s*|\s*-+>?\s+))(.+)$/is ?
      (defined $2 ? $2 : '') : undef;
}

# on_public: messages received on channels
sub on_public {
    my ($self, $event) = @_;
    my $data = join(' ', $event->args);
    if (defined($_ = targetted($data, quotemeta($nicks[$nick])))) {
        if ($_ ne '') {
            setEventArgs($event, $_);
            $event->{'__mozbot__fulldata'} = $data;
            &do($self, $event, 'Told', 'Baffled');
        } else {
            &do($self, $event, 'Heard');
        }
    } else {
        foreach my $nick (@ignoredTargets) {
            if (defined targetted($data, $nick)) {
                my $channel = &toToChannel($self, @{$event->to});
                &debug("Ignored (target matched /$nick/): $channel <".$event->nick.'> '.join(' ', $event->args));
                return;
            }
        }
        &do($self, $event, 'Heard');
    }
}

# on_noticemsg: notice messages from the server, some service, or another
# user.  beware!  it's generally Bad Juju to respond to these, but for
# some things (like opn's NickServ) it's appropriate.
sub on_noticemsg {
    my ($self, $event) = @_;
    &do($self, $event, 'Noticed');
}

sub on_private {
    my ($self, $event) = @_;
    my $data = join(' ', $event->args);
    my $nick = quotemeta($nicks[$nick]);
    if (($data =~ /^($nick(?:[-\s,:;.!?]|\s*-+>?\s+))(.+)$/is) and ($2)) {
        # we do this so that you can say 'mozbot do this' in both channels
        # and /query screens alike (otherwise, in /query screens you would
        # have to remember to omit the bot name).
        setEventArgs($event, $2);
    }
    &do($self, $event, 'Told', 'Baffled');
}

# on_me: /me actions (CTCP actually)
sub on_me {
    my ($self, $event) = @_;
    my @data = $event->args;
    my $data = join(' ', @data);
    setEventArgs($event, $data);
    my $nick = quotemeta($nicks[$nick]);
    if ($data =~ /(?:^|[\s":<([])$nick(?:[])>.,?!\s'&":]|$)/is) {
        &do($self, $event, 'Felt');
    } else {
        &do($self, $event, 'Saw');
    }
}

# on_topic: for when someone changes the topic
# also for when the server notifies us of the topic
# ...so we have to parse it carefully.
sub on_topic {
    my ($self, $event) = @_;
    if ($event->userhost eq '@') {
        # server notification
        # need to parse data
        my (undef, $channel, $topic) = $event->args;
        setEventArgs($event, $topic);
        $event->to($channel);
    }
    &do(@_, 'SpottedTopicChange');
}

# on_kick: parse the kick event
sub on_kick {
    my ($self, $event) = @_;
    my ($channel, $from) = $event->args; # from is already set anyway
    my $who = $event->to;
    $event->to($channel);
    foreach (@$who) {
        setEventArgs($event, $_);
        if ($_ eq $nicks[$nick]) {
            &do(@_, 'Kicked');
        } else {
            &do(@_, 'SpottedKick');
        }
    }
}

# Gives lag results for outgoing PINGs.
sub on_cpong {
    my ($self, $event) = @_;
    &debug('completed CTCP PING with '.$event->nick.': '.days($event->args->[0]));
    # XXX should be able to use this then... see also Greeting module
    # in standard distribution
}

# -- #mozbot was here --
#   <timeless> $conn->add_handler('gender',\&on_ctcp_gender);
#   <timeless> sub on_ctcp_gender{
#   <timeless>     my (undef, $event)=@_;
#   <timeless>     my $nick=$event->nick;
#      <Hixie>     # timeless this suspense is killing me!
#   <timeless>     $bot->ctcp_reply($nick, 'neuter');
#   <timeless> }

# on_gender: What gender are we?
sub on_gender {
    my ($self, $event) = @_;
    my $nick = $event->nick;
    $self->ctcp_reply($nick, 'female'); # changed to female by special request
}

# on_nick: A nick changed -- was it ours?
sub on_nick {
    my ($self, $event) = @_;
    if ($event->nick eq $nicks[$nick]) {
        on_set_nick($self, $event);
    }
    &do(@_, 'SpottedNickChange');
}

# simple handler for when users do various things and stuff
sub on_join { &do(@_, 'SpottedJoin'); }
sub on_part { &do(@_, 'SpottedPart'); }
sub on_quit { &do(@_, 'SpottedQuit'); }
sub on_invite { &do(@_, 'Invited'); }
sub on_mode { &do(@_, 'ModeChange'); } # XXX need to parse modes # XXX on key change, change %channelKeys hash
sub on_umode { &do(@_, 'UModeChange'); }
sub on_version { &do(@_, 'CTCPVersion'); }
sub on_source { &do(@_, 'CTCPSource'); }
sub on_cping { &do(@_, 'CTCPPing'); }

sub newEvent($) {
    my $event = shift;
    $event->{'time'} = time();
    return $event;
}

sub toToChannel {
    my $self = shift;
    my $channel;
    foreach (@_) {
        if (/^[#&+\$]/os) {
            if (defined($channel)) {
                return '';
            } else {
                $channel = $_;
            }
        } elsif ($_ eq $nicks[$nick]) {
            return '';
        }
    }
    return lc($channel); # if message was sent to one person only, this is it
}

# XXX some code below calls this, on lines marked "hack hack hack". We
# should fix this so that those are supported calls.
sub do {
    my $self = shift @_;
    my $event = shift @_;
    my $to = $event->to;
    my $channel = &toToChannel($self, @$to);
    my $e = newEvent({
        'bot' => $self,
        '_event' => $event, # internal internal internal do not use... ;-)
        'channel' => $channel,
        'from' => $event->nick,
        'target' => $channel || $event->nick,
        'user' => $event->userhost,
        'data' => join(' ', $event->args),
        'fulldata' => defined($event->{'__mozbot__fulldata'}) ? $event->{'__mozbot__fulldata'} : join(' ', $event->args),
        'to' => $to,
        'subtype' => $event->type,
        'firsttype' => $_[0],
        'nick' => $nicks[$nick],
        # level   (set below)
        # type  (set below)
    });
    # updated admin field if person is an admin
    if ($authenticatedUsers{$event->userhost}) {
        if (($userFlags{$authenticatedUsers{$event->userhost}} & 1) == 1) {
            $lastadmin = $event->nick;
        }
        $e->{'userName'} = $authenticatedUsers{$event->userhost};
        $e->{'userFlags'} = $userFlags{$authenticatedUsers{$event->userhost}};
    } else {
        $e->{'userName'} = 0;
    }
    unless (scalar(grep $e->{'user'} =~ /^$_$/gi, @ignoredUsers)) {
        my $continue;
        do {
            my $type = shift @_;
            my $level = 0;
            my @modulesInNextLoop = @modules;
            $continue = 1;
            $e->{'type'} = $type;
            &debug("$type: $channel <".$event->nick.'> '.join(' ', $event->args));
            do {
                $level++;
                $e->{'level'} = $level;
                my @modulesInThisLoop = @modulesInNextLoop;
                @modulesInNextLoop = ();
                foreach my $module (@modulesInThisLoop) {
                    my $currentResponse;
                    eval {
                        $currentResponse = $module->do($self, $event, $type, $e);
                    };
                    if ($@) {
                        # $@ contains the error
                        &debug("ERROR IN MODULE $module->{'_name'}!!!", $@);
                    } elsif (!defined($currentResponse)) {
                        &debug("ERROR IN MODULE $module->{'_name'}: invalid response code to event '$type'.");
                    } else {
                        if ($currentResponse > $level) {
                            push(@modulesInNextLoop, $module);
                        }
                        $continue = ($continue and $currentResponse);
                    }
                }
            } while ($continue and @modulesInNextLoop);
        } while ($continue and scalar(@_));
    } else {
        &debug('Ignored (from \'' . $event->userhost . "'): $channel <".$event->nick.'> '.join(' ', $event->args));
    }
    &doLog($e);
}

sub doLog {
    my $e = shift;
    foreach my $module (@modules) {
        eval {
            $module->Log($e);
        };
        if ($@) {
            # $@ contains the error
            &debug("ERROR!!!", $@);
        }
    }
}


################################
# internal utilities           #
################################

my @msgqueue;
my %recentMessages;
my $timeLastSetAway = 0; # the time since the away flag was last set, so that we don't set it repeatedly.

# Use this routine, always, instead of the standard "privmsg" routine.  This
# one makes sure we don't send more than one message every two seconds or so,
# which will make servers not whine about us flooding the channel.
# messages aren't the only type of flood :-( away is included
sub sendmsg {
    my ($self, $who, $msg, $do) = (@_, 'msg');
    unless ((defined($do) and defined($msg) and defined($who) and ($who ne '')) and
            ((($do eq 'msg') and (not ref($msg))) or
             (($do eq 'me') and (not ref($msg))) or
             (($do eq 'notice') and (not ref($msg))) or
             (($do eq 'ctcpSend') and (ref($msg) eq 'ARRAY') and (@$msg >= 2)) or
             (($do eq 'ctcpReply') and (not ref($msg))))) {
        cluck('Wrong arguments passed to sendmsg() - ignored');
    } else {
        $self->schedule($delaytime / 2, \&drainmsgqueue) unless @msgqueue;
        if ($do eq 'msg' or $do eq 'me' or $do eq 'notice') {
            foreach (splitMessageAcrossLines($msg)) {
                push(@msgqueue, [$who, $_, $do]);
            }
        } else {
            push(@msgqueue, [$who, $msg, $do]);
        }
    }
}

# send any pending messages
sub drainmsgqueue {
    my $self = shift;
    return unless $self->connected;
    my $qln = @msgqueue;
    if (@msgqueue > 0) {
        my ($who, $msg, $do) = getnextmsg();
        unless (weHaveSaidThisTooManyTimesAlready($self, \$who, \$msg, \$do)) {
            my $type;
            if ($do eq 'msg') {
                &debug("->$who: $msg"); # XXX this makes logfiles large quickly...
                $self->privmsg($who, $msg); # it seems 'who' can be an arrayref and it works
                $type = 'Heard';
            } elsif ($do eq 'me') {
                &debug("->$who * $msg"); # XXX
                $self->me($who, $msg);
                $type = 'Saw';
            } elsif ($do eq 'notice') {
                &debug("=notice=>$who: $msg");
                $self->notice($who, $msg);
                # $type = 'XXX';
            } elsif ($do eq 'ctcpSend') {
                { local $" = ' '; &debug("->$who CTCP PRIVMSG @$msg"); }
                my $type = shift @$msg; # @$msg contains (type, args)
                $self->ctcp($type, $who, @$msg);
                # $type = 'XXX';
            } elsif ($do eq 'ctcpReply') {
                &debug("->$who CTCP NOTICE $msg");
                $self->ctcp_reply($who, $msg);
                # $type = 'XXX';
            } else {
                &debug("Unknown action '$do' intended for '$who' (content: '$msg') ignored.");
            }
            if (defined($type)) {
                &doLog(newEvent({
                    'bot' => $self,
                    '_event' => undef,
                    'channel' => &toToChannel($self, $who),
                    'from' => $nicks[$nick],
                    'target' => $who,
                    'user' => undef, # XXX
                    'data' => $msg,
                    'fulldata' => $msg,
                    'to' => $who,
                    'subtype' => undef,
                    'firsttype' => $type,
                    'nick' => $nicks[$nick],
                    'level' => 0,
                    'type' => $type,
                }));
            }
        }
        if (@msgqueue > 0) {
            if ((@msgqueue % 10 == 0) and (time() - $timeLastSetAway > 5 * $delaytime)) {
                &bot_longprocess($self, "Long send queue. There were $qln, and I just sent one to $who.");
                $timeLastSetAway = time();
                $self->schedule($delaytime * 4, # because previous one counts as message, plus you want to delay an extra bit regularly
                    \&drainmsgqueue);
            } else {
                $self->schedule($delaytime, \&drainmsgqueue);
            }
        } else {
            &bot_back($self); # clear away state
        }
    }
}

sub weHaveSaidThisTooManyTimesAlready {
    my($self, $who, $msg, $do) = @_;
    my $key;
    if ($$do eq 'ctcpSend') {
        local $" = ',';
        $key = "$$who,$$do,@{$$msg}";
    } else {
        $key = "$$who,$$do,$$msg";
    }
    my $count = ++$recentMessages{$key};
    if ($count >= $recentMessageCountLimit and
        $count < $recentMessageCountLimit + 1 and
        $$do ne 'ctcpSend') {
        $recentMessages{$key} += $recentMessageCountPenalty;
        my $text = $$msg;
        if (length($msg) > 23) { # arbitrary length (XXX)
            $text = substr($text, 0, 20) . '...';
        }
        $$do = 'me';
        $$msg = "was going to say '$text' but has said it too many times today already";
    } elsif ($count >= $recentMessageCountLimit) {
        if ($$do eq 'msg') {
            &debug("MUTED: ->$$who: $$msg");
        } elsif ($$do eq 'me') {
            &debug("MUTED: ->$$who * $$msg"); # XXX
        } elsif ($$do eq 'notice') {
            &debug("MUTED: =notice=>$$who: $$msg");
        } elsif ($$do eq 'ctcpSend') {
            local $" = ' ';
            &debug("MUTED: ->$$who CTCP PRIVMSG @{$$msg}");
        } elsif ($$do eq 'ctcpReply') {
            &debug("MUTED: ->$$who CTCP NOTICE $$msg");
        } else {
            &debug("MUTED: Unknown action '$$do' intended for '$$who' (content: '$$msg') ignored.");
        }
        return 1;
    }
    return 0;
}

sub lowerRecentMessageCount {
    my $self = shift;
    return unless $self->connected;
    foreach my $key (keys %recentMessages) {
        $recentMessages{$key} -= $recentMessageCountDecrementRate;
        if ($recentMessages{$key} <= 0) {
            delete $recentMessages{$key};
        }
    }
    $self->schedule($delaytime, \&lowerRecentMessageCount);
}

# wrap long lines at spaces and hard returns (\n)
# this is for IRC, not for the console -- long can be up to 255
sub splitMessageAcrossLines {
    my ($str) = @_;
    my $MAXPROTOCOLLENGTH = 255;
    my @output;
    # $str could be several lines split with \n, so split it first:
    foreach my $line (split(/\n/, $str)) {
        while (length($line) > $MAXPROTOCOLLENGTH) {
            # position is zero-based index
            my $pos = rindex($line, ' ', $MAXPROTOCOLLENGTH - 1);
            if ($pos < 0) {
                $pos = $MAXPROTOCOLLENGTH - 1;
            }
            push(@output, substr($line, 0, $pos));
            $line = substr($line, $pos);
            $line =~ s/^\s+//gos;
        }
        push(@output, $line) if length($line);
    }
    return @output;
}

# equivalent of shift or pop, but for the middle of the array.
# used by getnextmsg() below to pull the messages out of the
# msgqueue stack and shove them at the end.
sub yank {
    my ($index, $list) = @_;
    my $result = @{$list}[$index];
    @{$list} = (@{$list}[0..$index-1], @{$list}[$index+1..$#{$list}]);
    return $result;
}

# looks at the msgqueue stack and decides which message to send next.
sub getnextmsg {
    my ($who, $msg, $do) = @{shift(@msgqueue)};
    my @newmsgqueue;
    my $index = 0;
    while ($index < @msgqueue) {
        if ($msgqueue[$index]->[0] eq $who) {
            push(@newmsgqueue, &yank($index, \@msgqueue));
        } else {
            $index++;
        }
    }
    push(@msgqueue, @newmsgqueue);
    return ($who, $msg, $do);
}

my $markedaway = 0;

# mark bot as being away
sub bot_longprocess {
    my $self = shift;
    &debug('[away: '.join(' ',@_).']');
    $self->away(join(' ',@_));
    $markedaway = @_;
}

# mark bot as not being away anymore
sub bot_back {
    my $self = shift;
    $self->away('') if $markedaway;
    $markedaway = 0;
}


# internal routines for IO::Select handling

sub bot_select {
    my ($pipe) = @_;
    $irc->removefh($pipe);
    # enable slurp mode for this function (see man perlvar for $/ documentation)
    local $/;
    undef $/;
    my $data = <$pipe>;
    &debug("child ${$pipe}->{'BotModules_PID'} completed ${$pipe}->{'BotModules_ChildType'}".
           (${$pipe}->{'BotModules_Module'}->{'_shutdown'} ?
            ' (nevermind, module has shutdown)': ''));
    kill 9, ${$pipe}->{'BotModules_PID'}; # ensure child is dead
    # non-blocking reap of any pending zombies
    1 while waitpid(-1,WNOHANG) > 0;
    return if ${$pipe}->{'BotModules_Module'}->{'_shutdown'}; # see unload()
    eval {
        ${$pipe}->{'BotModules_Event'}->{'time'} = time(); # update the time field of the event
        ${$pipe}->{'BotModules_Module'}->ChildCompleted(
            ${$pipe}->{'BotModules_Event'},
            ${$pipe}->{'BotModules_ChildType'},
            $data,
            @{${$pipe}->{'BotModules_Data'}}
        );
    };
    if ($@) {
        # $@ contains the error
        &debug("ERROR!!!", $@);
    }
}


# internal routines for console output, stuff

# print debugging info
sub debug {
    my $line;
    foreach (@_) {
        $line = $_; # can't chomp $_ since it is a hardref to the arguments...
        chomp $line; # ...and they are probably a constant string!
        if (-t) {
            print &logdate() . " ($$) $line";
        }
        if ($LOGGING) {
            # XXX this file grows without bounds!!!
            if (open(LOG, ">>$LOGFILEPREFIX.$$.log")) {
                print LOG &logdate() . " $line\n";
                close(LOG);
                print "\n";
            } else {
                print " [not logged, $!]\n";
            }
        }
    }
}

# logdate: return nice looking date and time stamp
sub logdate {
    my ($sec, $min, $hour, $mday, $mon, $year) = gmtime(shift or time());
    return sprintf("%d-%02d-%02d %02d:%02d:%02d UTC",
                   $year + 1900, $mon + 1, $mday, $hour, $min, $sec);
}

# days: how long ago was that?
sub days {
    my $then = shift;
    # maths
    my $seconds = time() - $then;
    my $minutes = int ($seconds / 60);
    my $hours = int ($minutes / 60);
    my $days = int ($hours / 24);
    # english
    if ($seconds < 60) {
        return sprintf("%d second%s", $seconds, $seconds == 1 ? "" : "s");
    } elsif ($minutes < 60) {
        return sprintf("%d minute%s", $minutes, $minutes == 1 ? "" : "s");
    } elsif ($hours < 24) {
        return sprintf("%d hour%s", $hours, $hours == 1 ? "" : "s");
    } else {
        return sprintf("%d day%s", $days, $days == 1 ? "" : "s");
    }
}

# signal handler
sub killed {
    my($sig) = @_;
    &debug("received signal $sig. shutting down...");
    &debug('This is evil. You should /msg me a shutdown command instead.');
    &debug('WARNING: SHUTTING ME DOWN LIKE THIS CAN CAUSE FORKED PROCESSES TO START UP AS BOTS!!!'); # XXX which we should fix, of course.
    exit(1); # sane exit, including shutting down any modules
}


# internal routines for configuration

my %configStructure; # hash of cfg file keys and associated variable refs

# ok. In strict 'refs' mode, you cannot use strings as refs. Fair enough.
# However, hash keys are _always_ strings. Using a ref as a hash key turns
# it into a string. So we have to keep a virgin copy of the ref around.
#
# So the structure of the %configStructure hash is:
#   "ref" => [ cfgName, ref ]
# Ok?

sub registerConfigVariables {
    my (@variables) = @_;
    foreach (@variables) {
        $configStructure{$$_[0]} = [$$_[1], $$_[0]];
    }
} # are you confused yet?

sub configStructure {
    my (@variables) = @_;
    my %struct;
    @variables = keys %configStructure unless @variables;
    foreach (@variables) {
        confess("Function configStructure was passed something that is either not a ref or has not yet neem registered, so aborted") unless defined($configStructure{$_});
        $struct{$configStructure{$_}[0]} = $configStructure{$_}[1];
    }
    return \%struct;
}


# internal routines for handling the modules

sub getModule {
    my ($name) = @_;
    foreach my $module (@modules) { # XXX this is not cached as a hash as performance is not a priority here
        return $module if $name eq $module->{'_name'};
    }
    return undef;
}

sub LoadModule {
    my ($name) = @_;
    # sanitize the name
    $name =~ s/[^-a-zA-Z0-9]/-/gos;
    # check the module is not already loaded
    foreach (@modules) {
        if ($_->{'_name'} eq $name) {
            return "Failed [0]: Module already loaded. Don't forget to enable it in the various channels (vars $name channels '+#channelname').";
        }
    }
    # read the module in from a file
    my $filename = "./BotModules/$name.bm"; # bm = bot module
    my $result = open(my $file, "< $filename");
    if ($result) {
        my $code = do {
            local $/ = undef; # enable "slurp" mode
            <$file>; # whole file now here
        };
        if ($code) {
#           if ($code =~ /package\s+\QBotModules::$name\E\s*;/gos) { XXX doesn't work reliably?? XXX
                # eval the file
                $code =~ /^(.*)$/os;
                $code = $1; # completely defeat the tainting mechanism.
                # $code = "# FILE: $filename\n".$code; # "# file 1 '$filename' \n" would be good without Carp.pm
                { no warnings; # as per the warning, but doesn't work??? XXX
                eval($code); }
                if ($@) {
                    # $@ contains the error
                    return "Failed [4]: $@";
                } else {
                    # if ok, then create a module
                    my $newmodule;
                    eval("
                        \$newmodule = BotModules::$name->create('$name', '$filename');
                    ");
                    if ($@) {
                        # $@ contains the error
                        return "Failed [5]: $@";
                    } else {
                        # if ok, then add it to the @modules list
                        push(@modules, $newmodule);
                        push(@modulenames, $newmodule->{'_name'});
                        &Configuration::Save($cfgfile, &::configStructure(\@modulenames));
                        # Done!!!
                        return $newmodule;
                    }
                }
#           } else {
#               return "Failed [3]: Could not find valid module definition line.";
#           }
        } else {
            # $! contains the error
            if ($!) {
                return "Failed [2]: $!";
            } else {
                return "Failed [2]: Module file is empty.";
            }
        }
    } else {
        # $! contains the error
        return "Failed [1]: $!";
    }
}

sub UnloadModule {
    my ($name) = @_;
    # remove the reference from @modules
    my @newmodules;
    my @newmodulenames;
    foreach (@modules) {
        if ($name eq $_->{'_name'}) {
            if ($_->{'_static'}) {
                return 'Cannot unload this module, it is built in.';
            }
            $_->unload();
        } else {
            push(@newmodules, $_);
            push(@newmodulenames, $_->{'_name'});
        }
    }
    if (@modules == @newmodules) {
        return 'Module not loaded. Are you sure you have the right name?';
    } else {
        @modules = @newmodules;
        @modulenames = @newmodulenames;
        &Configuration::Save($cfgfile, &::configStructure(\@modulenames));
        return;
    }
}

# password management functions

sub getSalt {
    # straight from man perlfunc
    return join('', ('.', '/', 0..9, 'A'..'Z', 'a'..'z')[rand 64, rand 64]);
}

sub newPassword {
    my($text) = @_;
    return crypt($text, &getSalt());
}

sub checkPassword {
    my($text, $password) = @_;
    return (crypt($text, $password) eq $password);
}

################################
# Base Module                  #
################################

# And now, for my next trick, the base module (duh).

package BotModules;

1; # nothing to see here...

# ENGINE INTERFACE

# create - create a new BotModules object.
# Do not call this yourself. We call it. Ok?
# Do not override this either, unless you know what
# you are doing (I don't, and I wrote it...). If you
# want to add variables to $self, use Initialise.
# The paramter is the name of the module.
sub create {
    my $class = shift;
    my ($name, $filename) = @_;
    my $self = {
        '_name' => $name,
        '_shutdown' => 0, # see unload()
        '_static' => 0, # set to 1 to prevent module being unloaded
        '_variables' => {},
        '_config' => {},
        '_filename' => $filename,
        '_filemodificationtime' => undef,
    };
    bless($self, $class);
    $self->Initialise();
    $self->RegisterConfig();
    return $self;
}

sub DESTROY {
    my $self = shift;
    $self->debug('garbage collected');
}

# called by &::UnloadModule().
# this removes any pointers to the module.
# for example, it stops the scheduler from installing new timers,
# so that the bot [eventually] severs its connection with the module.
sub unload {
    my $self = shift;
    $self->{'_shutdown'} = 1; # see doScheduled and bot_select
}

# configStructure - return the hash needed for Configuration module
sub configStructure {
    my $self = shift;
    return $self->{'_config'};
}

# do - called to do anything (duh) (no, do, not duh) (oh, ok, sorry)
sub do {
    my $self = shift;
    my ($bot, $event, $type, $e) = @_;
    # first, we check that the user is not banned from using this module. If he
    # is, then re give up straight away.
    return 1 if ($self->IsBanned($e));
    # next we check that the module is actually enabled in this channel, and
    # if it is not we quit straight away as well.
    return 1 unless ($e->{'channel'} eq '') or ($self->InChannel($e));
    # Ok, dispatch the event.
    if ($type eq 'Told') {
        return $self->Told($e, $e->{'data'});
    } elsif ($type eq 'Heard') {
        return $self->Heard($e, $e->{'data'});
    } elsif ($type eq 'Baffled') {
        return $self->Baffled($e, $e->{'data'});
    } elsif ($type eq 'Noticed') {
        return $self->Noticed($e, $e->{'data'});
    } elsif ($type eq 'Felt') {
        return $self->Felt($e, $e->{'data'});
    } elsif ($type eq 'Saw') {
        return $self->Saw($e, $e->{'data'});
    } elsif ($type eq 'Invited') {
        return $self->Invited($e, $e->{'data'});
    } elsif ($type eq 'Kicked') {
        return $self->Kicked($e, $e->{'channel'});
    } elsif ($type eq 'ModeChange') {
        return $self->ModeChange($e, $e->{'channel'}, $e->{'data'}, $e->{'from'});
    } elsif ($type eq 'Authed') {
        return $self->Authed($e, $e->{'from'});
    } elsif ($type eq 'SpottedNickChange') {
        return $self->SpottedNickChange($e, $e->{'from'}, $e->{'data'});
    } elsif ($type eq 'SpottedTopicChange') {
        return $self->SpottedTopicChange($e, $e->{'channel'}, $e->{'data'});
    } elsif ($type eq 'SpottedJoin') {
        return $self->SpottedJoin($e, $e->{'channel'}, $e->{'from'});
    } elsif ($type eq 'SpottedPart') {
        return $self->SpottedPart($e, $e->{'channel'}, $e->{'from'});
    } elsif ($type eq 'SpottedKick') {
        return $self->SpottedKick($e, $e->{'channel'}, $e->{'data'});
    } elsif ($type eq 'SpottedQuit') {
        return $self->SpottedQuit($e, $e->{'from'}, $e->{'data'});
    } elsif ($type eq 'CTCPPing') {
        return $self->CTCPPing($e, $e->{'from'}, $e->{'data'});
    } elsif ($type eq 'CTCPVersion') {
        return $self->CTCPVersion($e, $e->{'from'}, $e->{'data'});
    } elsif ($type eq 'CTCPSource') {
        return $self->CTCPSource($e, $e->{'from'}, $e->{'data'});

    # XXX have not implemented mode parsing yet
    } elsif ($type eq 'GotOpped') {
        return $self->GotOpped($e, $e->{'channel'}, $e->{'from'});
    } elsif ($type eq 'GotDeopped') {
        return $self->GotDeopped($e, $e->{'channel'}, $e->{'from'});
    } elsif ($type eq 'SpottedOpping') {
        return $self->SpottedOpping($e, $e->{'channel'}, $e->{'from'});
    } elsif ($type eq 'SpottedDeopping') {
        return $self->SpottedDeopping($e, $e->{'channel'}, $e->{'from'});
    } else {
        $self->debug("Unknown action type '$type'. Ignored.");
        # XXX UModeChange (not implemented yet)
        return 1; # could not do it
    }
}


# MODULE API - use these from the your routines.

# prints output to the console
sub debug {
    my $self = shift;
    foreach my $line (@_) {
        &::debug('Module '.$self->{'_name'}.': '.$line);
    }
}

# saveConfig - call this when you change a configuration option. It resaves the config file.
sub saveConfig {
    my $self = shift;
    &Configuration::Save($cfgfile, $self->configStructure());
}

# registerVariables - Registers a variable with the config system and the var setting system
# parameters: (
#     [ 'name', persistent ? 1:0, editable ? 1:0, $value ],
#               use undef instead of 0 or 1 to leave as is
#               use undef (or don't mention) the $value to not set the value
# )
sub registerVariables {
    my $self = shift;
    my (@variables) = @_;
    foreach (@variables) {
        $self->{$$_[0]} = $$_[3] if defined($$_[3]);
        if (defined($$_[1])) {
            if ($$_[1]) {
                $self->{'_config'}->{$self->{'_name'}.'::'.$$_[0]} = \$self->{$$_[0]};
            } else {
                delete($self->{'_config'}->{$self->{'_name'}.'::'.$$_[0]});
            }
        }
        $self->{'_variables'}->{$$_[0]} = $$_[2] if defined($$_[2]);
    }
}

# internal implementation of the scheduler
sub doScheduled {
    my $bot = shift;
    my ($self, $event, $time, $times, @data) = @_;
    return if ($self->{'_shutdown'}); # see unload()
    # $self->debug("scheduled event occured; $times left @ $time second interval");
    eval {
        $event->{'time'} = time(); # update the time field of the event
        $self->Scheduled($event, @data);
        $self->schedule($event, $time, --$times, @data);
    };
    if ($@) {
        # $@ contains the error
        &::debug("ERROR!!!", $@);
    }
}

# schedule - Sets a timer to call Scheduled later
# for events that should be setup at startup, call this from Schedule().
sub schedule {
    my $self = shift;
    my ($event, $time, $times, @data) = @_;
    return if ($times == 0 or $self->{'_shutdown'}); # see unload()
    $times = -1 if ($times < 0); # pass a negative number to have a recurring timer
    my $delay = $time;
    if (ref($time)) {
        if (ref($time) eq 'SCALAR') {
            $delay = $$time;
        } else {
            return; # XXX maybe be useful?
        }
    }
    # if ($delay < 1) {
    #     $self->debug("Vetoed aggressive scheduling; forcing to 1 second minimum");
    #     $delay = 1;
    # }
    $event->{'bot'}->schedule($delay, \&doScheduled, $self, $event, $time, $times, @data);
}

# spawnChild - spawns a child process and adds it to the list of file handles to monitor
# eventually the bot calls ChildCompleted() with the output of the chlid process.
sub spawnChild {
    my $self = shift;
    my ($event, $command, $arguments, $type, $data) = @_;
    # uses IO::SecurePipe and fork and exec
    # secure, predictable, no dependencies on external code
    # uses fork explicitly (and once implicitly)
    my $pipe = IO::SecurePipe->new();
    if (defined($pipe)) {
        my $child = fork();
        if (defined($child)) {
            if ($child) {
                # we are the parent process
                $pipe->reader();
                ${$pipe}->{'BotModules_Module'} = $self;
                ${$pipe}->{'BotModules_Event'} = $event;
                ${$pipe}->{'BotModules_ChildType'} = $type;
                ${$pipe}->{'BotModules_Data'} = $data;
                ${$pipe}->{'BotModules_Command'} = $command;
                ${$pipe}->{'BotModules_Arguments'} = $arguments;
                ${$pipe}->{'BotModules_PID'} = $child;
                $irc->addfh($pipe, \&::bot_select);
                local $" = ' ';
                $self->debug("spawned $child ($command @$arguments)");
                return 0;
            } else {
                eval {
                    # we are the child process
                    # call $command and buffer the output
                    $pipe->writer(); # get writing end of pipe, ready to output the result
                    my $output;
                    if (ref($command) eq 'CODE') {
                        $output = &$command(@$arguments);
                    } else {
                        # it would be nice if some of this was on a timeout...
                        my $result = IO::SecurePipe->new(); # create a new pipe for $command
                        # call $command (implicit fork(), which may of course fail)
                        $result->reader($command, @$arguments);
                        local $/; # to not affect the rest of the program (what little there is)
                        $/ = \(2*1024*1024); # slurp up to two megabytes
                        $output = <$result>; # blocks until child process has finished
                        close($result); # reap child
                    }
                    print $pipe $output if ($output); # output the lot in one go back to parent
                    $pipe->close();
                };
                if ($@) {
                    # $@ contains the error
                    $self->debug('failed to spawn child', $@);
                }

                # -- #mozwebtools was here --
                #       <dawn> when is that stupid bot going to get checked in?
                #   <timeless> after it stops fork bombing
                #       <dawn> which one? yours or hixies?
                #   <timeless> his, mine doesn't fork
                #   <timeless> see topic
                #       <dawn> are there plans to fix it?
                #   <timeless> yes. but he isn't sure exactly what went wrong
                #   <timeless> i think it's basically they fork for wget
                #       <dawn> why don't you help him?
                #   <timeless> i don't understand forking
                #       <dawn> that didn't stop hixie
                #   <timeless> not to mention the fact that his forking doesn't
                #              work on windows
                #       <dawn> you have other machines. techbot1 runs on windows?
                #   <timeless> yeah it runs on windows
                #       <dawn> oh
                #       <dawn> get a real os, man

                # The bug causing the 'fork bombing' was that I only
                # did the following if $@ was true or if the call to
                # 'reader' succeeded -- so if some other error occured
                # that didn't trip the $@ test but still crashed out
                # of the eval, then the script would quite happily
                # continue, and when it eventually died (e.g. because
                # of a bad connection), it would respawn multiple
                # times (as many times as it had failed to fork) and
                # it would succeed in reconnecting as many times as
                # had been configured nicks...

                eval {
                    $0 =~ m/^(.*)$/os; # untaint $0 so that we can call it below:
                    exec { $1 } ($1, '--abort'); # do not call shutdown handlers
                    # the previous line works because exec() bypasses
                    # the perl object garbarge collection and simply
                    # deallocates all the memory in one go. This means
                    # the shutdown handlers (DESTROY and so on) are
                    # never called for this fork. This is good,
                    # because otherwise we would disconnect from IRC
                    # at this point!
                };

                $self->debug("failed to shutdown cleanly!!! $@");
                exit(1); # exit in case exec($0) failed

            }
        } else {
            $self->debug("failed to fork: $!");
        }
    } else {
        $self->debug("failed to open pipe: $!");
    }
    return 1;
}

# getURI - Downloads a file and then calls GotURI
sub getURI {
    my $self = shift;
    my ($event, $uri, @data) = @_;
    $self->spawnChild($event, 'wget', ['--quiet', '--passive', '--user-agent="Mozilla/5.0 (compatible; mozbot)"',  '--output-document=-', $uri], 'URI', [$uri, @data]);
}

# returns a reference to a module -- DO NOT STORE THIS REFERENCE!!!
sub getModule {
    my $self = shift;
    return &::getModule(@_);
}

# returns a reference to @msgqueue
# manipulating this is probably not a good idea. In particular,
# don't add anything to this array (use the appropriate methods
# instead, those that use &::sendmsg, below).
sub getMessageQueue {
    my $self = shift;
    return \@msgqueue;
}

# returns the value of $helpline
sub getHelpLine {
    return $helpline;
}

# returns a sorted list of module names
sub getModules {
    return sort(@modulenames);
}

# returns a filename with path suitable to use for logging
sub getLogFilename {
    my $self = shift;
    my($name) = @_;
    return "$LOGFILEDIR/$name";
}

# tellAdmin - may try to talk to an admin.
# NO GUARANTEES! This will PROBABLY NOT reach anyone!
sub tellAdmin {
    my $self = shift;
    my ($event, $data) = @_;
    if ($lastadmin) {
        $self->debug("Trying to tell admin '$lastadmin' this: $data");
        &::sendmsg($event->{'bot'}, $lastadmin, $data);
    } else {
        $self->debug("Wanted to tell an admin '$data', but I've never seen one.");
    }
}

# ctcpSend - Sends a CTCP message to someone
sub ctcpSend {
    my $self = shift;
    my ($event, $type, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'target'}, [$type, $data], 'ctcpSend');
}

# ctcpReply - Sends a CTCP reply to someone
sub ctcpReply {
    my $self = shift;
    my ($event, $type, $data) = @_;
    unless (defined($type)) {
        cluck('No type passed to ctcpReply - ignored');
    }
    if (defined($data)) {
        &::sendmsg($event->{'bot'}, $event->{'from'}, "$type $data", 'ctcpReply');
    } else {
        &::sendmsg($event->{'bot'}, $event->{'from'}, $type, 'ctcpReply');
    }
}

# notice - Sends a notice to a channel or person
sub notice {
    my $self = shift;
    my ($event, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'target'}, $data, 'notice');
}

# say - Sends a message to the channel
sub say {
    my $self = shift;
    my ($event, $data) = @_;
    return unless defined $event->{'target'};
    $data =~ s/^\Q$event->{'target'}\E: //gs;
    &::sendmsg($event->{'bot'}, $event->{'target'}, $data);
}

# announce - Sends a message to every channel
sub announce {
    my $self = shift;
    my ($event, $data) = @_;
    foreach (@{$self->{'channels'}}) {
        &::sendmsg($event->{'bot'}, $_, $data);
    }
}

# directSay - Sends a message to the person who spoke
sub directSay {
    my $self = shift;
    my ($event, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'from'}, $data);
}

# channelSay - Sends a message to the channel the message came from, IFF it came from a channel.
sub channelSay {
    my $self = shift;
    my ($event, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'channel'}, $data) if $event->{'channel'};
}

# -- #mozilla was here --
#      <richb> timeless: it's focal review time, and they are working out
#              where to allocate the money.
#      <richb> timeless: needless to say i have a vested interest in this.
#       <leaf> there's money in this?
#   <timeless> richb yes; leaf always
#       <leaf> how come nobody told me?
#   <timeless> because leaf doesn't need money
#   <timeless> for leaf it grows on trees
#       <leaf> *wince*

# emote - Sends an emote to the channel
sub emote {
    my $self = shift;
    my ($event, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'target'}, $data, 'me');
}

# directEmote - Sends an emote to the person who spoke
sub directEmote {
    my $self = shift;
    my ($event, $data) = @_;
    &::sendmsg($event->{'bot'}, $event->{'from'}, $data, 'me');
}

# sayOrEmote - calls say() or emote() depending on whether the string starts with /me or not.
sub sayOrEmote {
    my $self = shift;
    my ($event, $data) = @_;
    if ($data =~ /^\/me\s+/osi) {
        $data =~ s/^\/me\s+//gosi;
        $self->emote($event, $data);
    } else {
        $self->say($event, $data);
    }
}

# directSayOrEmote - as sayOrEmote() but calls the direct versions instead
sub directSayOrEmote {
    my $self = shift;
    my ($event, $data) = @_;
    if ($data =~ /^\/me\s+/osi) {
        $data =~ s/^\/me\s+//gosi;
        $self->directEmote($event, $data);
    } else {
        $self->directSay($event, $data);
    }
}

# isAdmin - Returns true if the person is an admin
sub isAdmin {
    my $self = shift;
    my ($event) = @_;
    return (($event->{'userName'}) and (($event->{'userFlags'} & 1) == 1));
}

# setAway - Set the bot's 'away' flag. A blank message will mark the bot as back.
# Note: If you need this you are doing something wrong!!!
sub setAway {
    my $self = shift;
    my ($event, $message) = @_;
    $event->{'bot'}->away($message);
}

# setNick - Set the bot's nick.
# Note: Best not to use this too much, especially not based on user input,
# as it is not throttled. XXX
sub setNick {
    my $self = shift;
    my ($event, $value) = @_;
    $event->{'bot'}->nick($value);
}

sub mode {
    my $self = shift;
    my ($event, $channel, $mode, $arg) = @_;
    $event->{'bot'}->mode($channel, $mode, $arg);
}

sub invite {
    my $self = shift;
    my ($event, $who, $channel) = @_;
    $event->{'bot'}->invite($who, $channel);
}

# pretty printer for turning lists of varying length strings into
# lists of roughly equal length strings without losing any data
sub prettyPrint {
    my $self = shift;
    my ($preferredLineLength, $prefix, $indent, $divider, @input) = @_;
    # sort numerically descending by length
    @input = sort {length($b) <=> length($a)} @input;
    # if we have a prefix defined, it goes first (duh)
    unshift(@input, $prefix) if defined($prefix);
    my @output;
    my $index;
    while (@input) {
        push(@output, $indent . shift(@input));
        $index = 0;
        while (($index <= $#input) and
               ((length($output[$#output]) + length($input[$#input])) < $preferredLineLength)) {
            # does this one fit?
            if ((length($output[$#output]) + length($input[$index])) < $preferredLineLength) {
                if (defined($prefix)) {
                    # don't stick the divider between the prefix and the first item
                    undef($prefix);
                } else {
                    $output[$#output] .= $divider;
                }
                $output[$#output] .= splice(@input, $index, 1);
            } else {
                $index++;
            }
        }
    }
    return @output;
}

# wordWrap routines which takes a list and wraps it. A less pretty version
# of prettyPrinter, but it keeps the order.
sub wordWrap {
    my $self = shift;
    my ($preferredLineLength, $prefix, $indent, $divider, @input) = @_;
    unshift(@input, $prefix) if defined($prefix);
    $indent = '' unless defined($indent);
    my @output;
    while (@input) {
        push(@output, $indent . shift(@input));
        while (($#input >= 0) and
               ((length($output[$#output]) + length($input[0])) < $preferredLineLength)) {
            $output[$#output] .= $divider . shift(@input);
        }
    }
    return @output;
}

sub unescapeXML {
    my $self = shift;
    my ($string) = @_;
    $string =~ s/&apos;/'/gos;
    $string =~ s/&quot;/"/gos;
    $string =~ s/&lt;/</gos;
    $string =~ s/&gt;/>/gos;
    $string =~ s/&amp;/&/gos;
    return $string;
}

sub days {
    my $self = shift;
    my ($then) = @_;
    return &::days($then);
}

# return the argument if it is a valid regular expression,
# otherwise quotes the argument and returns that.
sub sanitizeRegexp {
    my $self = shift;
    my ($regexp) = @_;
    if (defined($regexp)) {
        eval {
            '' =~ /$regexp/;
        };
        $self->debug("regexp |$regexp| returned error |$@|, quoting...") if $@;
        return $@ ? quotemeta($regexp) : $regexp;
    } else {
        $self->debug("blank regexp, returning wildcard regexp //...");
        return '';
    }
}


# MODULE INTERFACE (override these)

# Initialise - Called when the module is loaded
sub Initialise {
    my $self = shift;
}

# Schedule - Called after bot is set up, to set up any scheduled tasks
# use $self->schedule($event, $delay, $times, $data)
# where $times is 1 for a single event, -1 for recurring events,
# and a +ve number for an event that occurs that many times.
sub Schedule {
    my $self = shift;
    my ($event) = @_;
}

# JoinedIRC - Called before joining any channels (but after module is setup)
# this does not get called for dynamically loaded modules
sub JoinedIRC {
    my $self = shift;
    my ($event) = @_;
}

sub JoinedChannel {
    my $self = shift;
    my ($event, $channel) = @_;
    if ($self->{'autojoin'}) {
        push(@{$self->{'channels'}}, $channel)
          unless ((scalar(grep $_ eq $channel, @{$self->{'channels'}})) or
                  (scalar(grep $_ eq $channel, @{$self->{'channelsBlocked'}})));
        $self->saveConfig();
    }
}

# Called by the Admin module's Kicked and SpottedPart handlers
sub PartedChannel {
    my $self = shift;
    my ($event, $channel) = @_;
    if ($self->{'autojoin'}) {
        my %channels = map { $_ => 1 } @{$self->{'channels'}};
        if ($channels{$channel}) {
            delete($channels{$channel});
            @{$self->{'channels'}} = keys %channels;
            $self->saveConfig();
        }
    }
}

sub InChannel {
    my $self = shift;
    my ($event) = @_;
    return scalar(grep $_ eq $event->{'channel'}, @{$self->{'channels'}});
    # XXX could be optimised - cache the list into a hash.
}

sub IsBanned {
    my $self = shift;
    my ($event) = @_;
    return 0 if scalar(grep { $_ = $self->sanitizeRegexp($_); $event->{'user'} =~ /^$_$/ } @{$self->{'allowusers'}});
    return      scalar(grep { $_ = $self->sanitizeRegexp($_); $event->{'user'} =~ /^$_$/ } @{$self->{'denyusers'}});
}

# Baffled - Called for messages prefixed by the bot's nick which we don't understand
sub Baffled {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# Told - Called for messages prefixed by the bot's nick
sub Told {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# Noticed - Called for notice messages
sub Noticed {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# Heard - Called for all messages
sub Heard {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# Felt - Called for all emotes containing bot's nick
sub Felt {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# -- #mozilla was here --
# * bryner tries to imagine the need for NS_TWIPS_TO_MILES
#<Ben_Goodger> bryner: yeah, that isn't even a metric unit. should
#              be NS_TWIPS_TO_KILOMETERS
#     <bryner> there's that too
#<Ben_Goodger> oh
#<Ben_Goodger> really?
#     <bryner> yep
#<Ben_Goodger> o_O
#     <bryner> for when we use mozilla for surveying and such
#  <pinkerton> lol

# BTW. They aren't kidding. See:
# http://lxr.mozilla.org/seamonkey/search?string=NS_TWIPS_TO_KILOMETERS

# Saw - Called for all emotes
sub Saw {
    my $self = shift;
    my ($event, $message) = @_;
    return 1;
}

# Invited - Called when bot is invited into another channel
sub Invited {
    my $self = shift;
    my ($event, $channel) = @_;
    return 1;
}

# Kicked - Called when bot is kicked out of a channel
sub Kicked {
    my $self = shift;
    my ($event, $channel) = @_;
    return 1;
}

# ModeChange - Called when channel or bot has a mode flag changed
sub ModeChange {
    my $self = shift;
    my ($event, $what, $change, $who) = @_;
    return 1;
}

# GotOpped - Called when bot is opped
sub GotOpped {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# GotDeopped - Called when bot is deopped
sub GotDeopped {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# SpottedNickChange - Called when someone changes their nick
# Remember that you cannot use directSay here, since $event
# has the details of the old nick. And 'say' is useless
# since the channel is the old userhost string... XXX
sub SpottedNickChange {
    my $self = shift;
    my ($event, $from, $to) = @_;
    return 1;
}

# Authed - Called when someone authenticates with us.
# Remember that you cannot use say here, since this
# cannot actually be done in a channel...
sub Authed {
    my $self = shift;
    my ($event, $who) = @_;
    return 1;
}

# SpottedTopicChange - Called when someone thinks someone else said something funny
sub SpottedTopicChange {
    my $self = shift;
    my ($event, $channel, $new) = @_;
    return 1;
}

# SpottedJoin - Called when someone joins a channel
sub SpottedJoin {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# SpottedPart - Called when someone leaves a channel
sub SpottedPart {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# SpottedKick - Called when someone leaves a channel forcibly
sub SpottedKick {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# SpottedQuit - Called when someone leaves a server
# can't use say or directSay: no channel involved, and
# user has quit (obviously). XXX
sub SpottedQuit {
    my $self = shift;
    my ($event, $who, $why) = @_;
    return 1;
}

# CTCPPing - Called when we receive a CTCP Ping.
sub CTCPPing {
    my $self = shift;
    my ($event, $who, $what) = @_;
    return 1;
}

# CTCPVersion - Called when we receive a CTCP Version.
sub CTCPVersion {
    my $self = shift;
    my ($event, $who, $what) = @_;
    return 1;
}

# CTCPSource - Called when we receive a CTCP Source.
sub CTCPSource {
    my $self = shift;
    my ($event, $who, $what) = @_;
    return 1;
}

# SpottedOpping - Called when someone is opped
sub SpottedOpping {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# SpottedDeopping - Called when someone is... deopped, maybe?
sub SpottedDeopping {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    return 1;
}

# Scheduled - Called when a scheduled timer triggers
sub Scheduled {
    my $self = shift;
    my ($event, @data) = @_;
    if (ref($data[0]) eq 'CODE') {
        &{$data[0]}($event, @data);
    } else {
        $self->debug('Unhandled scheduled event... :-/');
    }
}

# ChildCompleted - Called when a child process has quit
sub ChildCompleted {
    my $self = shift;
    my ($event, $type, $output, @data) = @_;
    if ($type eq 'URI') {
        my $uri = shift(@data);
        $self->GotURI($event, $uri, $output, @data);
    }
}

# GotURI - Called when a requested URI has been downloaded
sub GotURI {
    my $self = shift;
    my ($event, $uri, $contents, @data) = @_;
}

# Help - Called to fully explain the module (return hash of command/description pairs)
# the string given for the '' key should be a module description
sub Help {
    my $self = shift;
    my ($event) = @_;
    return {};
}

# RegisterConfig - Called when initialised, should call registerVariables
sub RegisterConfig {
    my $self = shift;
    $self->registerVariables(
      # [ name, save?, settable?, value ]
        ['channels',        1, 1, []],
        ['channelsBlocked', 1, 1, []], # the channels in which this module will not autojoin regardless
        ['autojoin',        1, 1, 1],
        ['allowusers',      1, 1, []],
        ['denyusers',       1, 1, []],
    );
}

# Set - called to set a variable to a particular value.
sub Set {
    my $self = shift;
    my ($event, $variable, $value) = @_;
    if ($self->{'_variables'}->{$variable}) {
        if ((not defined($self->{$variable})) or (not ref($self->{$variable}))) {
            $self->{$variable} = $value;
        } elsif (ref($self->{$variable}) eq 'SCALAR') {
            ${$self->{$variable}} = $value;
        } elsif (ref($self->{$variable}) eq 'ARRAY') {
            if ($value =~ /^([-+])(.*)$/so) {
                if ($1 eq '+') {
                    push(@{$self->{$variable}}, $2);
                } else {
                    # We don't want to change the reference!!!
                    # Other variables might be pointing to there,
                    # it is *those* vars that affect the app.
                    my @oldvalue = @{$self->{$variable}};
                    @{$self->{$variable}} = ();
                    foreach (@oldvalue) {
                        push(@{$self->{$variable}}, $_) unless ($2 eq $_);
                    }
                    # XXX no feedback if nothing is done
                }
            } else {
                return 3; # not the right format dude!
            }
        } elsif (ref($self->{$variable}) eq 'HASH') {
            if ($value =~ /^\+(.)(.*)\1(.*)$/so) {
                $self->{$variable}->{$2} = $3;
                return -2 if $1 =~ /[a-zA-Z]/so;
            } elsif ($value =~ /^\-(.*)$/so) {
                # XXX no feedback if nothing is done
                delete($self->{$variable}->{$1});
            } else {
                return 4; # not the right format dude!
            }
        } else {
            return 1; # please to not be trying to set coderefs or arrayrefs or hashrefs or ...
        }
    } else {
        return 2; # please to not be trying to set variables I not understand!
    }
    $self->saveConfig();
    return 0;
}

# Get - called to get a particular variable
sub Get {
    my $self = shift;
    my ($event, $variable) = @_;
    return $self->{$variable};
}

# Log - Called for every event
sub Log {
    my $self = shift;
    my ($event) = @_;
}


################################
# Admin Module                 #
################################

package BotModules::Admin;
use vars qw(@ISA);
@ISA = qw(BotModules);
1;

# Initialise - Called when the module is loaded
sub Initialise {
    my $self = shift;
    $self->{'_fileModifiedTimes'} = {};
    $self->{'_static'} = 1;
}

# RegisterConfig - Called when initialised, should call registerVariables
sub RegisterConfig {
    my $self = shift;
    $self->SUPER::RegisterConfig(@_);
    $self->registerVariables(
      # [ name, save?, settable?, value ]
        ['allowInviting', 1, 1, 1], # by default, anyone can invite a bot into their channel
        ['allowChannelAdmin', 1, 1, 0], # by default, one cannot admin from a channel
        ['sourceCodeCheckDelay', 1, 1, 20], # by default, wait 20 seconds between source code checks
        ['files', 1, 1, [$0, 'lib/Mails.pm', 'lib/Configuration.pm', 'lib/IO/SecurePipe.pm']], # files to check for source code changes
        ['channels', 0, 0, undef], # remove the 'channels' internal variable...
        ['autojoin', 0, 0, 0], # remove the 'autojoin' internal variable...
        ['errorMessagesMaxLines', 1, 1, 5], # by default, only have 5 lines in error messages, trim middle if more
    );
    # now add in all the global variables...
    foreach (keys %configStructure) {
        $self->registerVariables([$configStructure{$_}[0], 0, 1, $configStructure{$_}[1]]) if (ref($configStructure{$_}[1]) =~ /^(?:SCALAR|ARRAY|HASH)$/go);
    }
}

# saveConfig - make sure we also save the main config variables...
sub saveConfig {
    my $self = shift;
    $self->SUPER::saveConfig(@_);
    &Configuration::Save($cfgfile, &::configStructure());
}

# Set - called to set a variable to a particular value.
sub Set {
    my $self = shift;
    my ($event, $variable, $value) = @_;
    # First let's special case some magic variables...
    if ($variable eq 'currentnick') {
        $self->setNick($event, $value);
        $self->say($event, "Attempted to change nick to '$value'.");
        return -1;
    } elsif ($variable eq 'nicks') {
        if ($value =~ /^([-+])(.*)$/so) {
            if ($1 eq '+') {
                # check it isn't there already and is not ''
                my $value = $2;
                if ($value eq '') {
                    $self->say($event, "The empty string is not a valid nick.");
                    return -1;
                }
                my $thenick = 0;
                $thenick++ while (($thenick < @nicks) and ($value ne $nicks[$thenick]));
                if ($thenick < @nicks) {
                    $self->say($event, "That nick (value) is already on the list of possible nicks.");
                    return -1;
                }
            } else {
                if ($2 eq $nicks[$nick]) {
                    $self->say($event, "You cannot remove the current nick ('$nicks[$nick]') from the list of allowed nicks... Change the 'currentnick' variable first!");
                    return -1;
                }
            }
        }
    }
    return $self->SUPER::Set($event, $variable, $value);
}

# Get - called to get a particular variable.
sub Get {
    my $self = shift;
    my ($event, $variable) = @_;
    # First let's special case some magic variables...
    if ($variable eq 'currentnick') {
        return $event->{'nick'};
    } elsif ($variable eq 'users') {
        my @users = sort keys %users;
        return \@users;
    } else {
        # else, check for known global variables...
        my $configStructure = &::configStructure();
        if (defined($configStructure->{$variable})) {
            return $configStructure->{$variable};
        } else {
            return $self->SUPER::Get($event, $variable);
        }
    }
}

# Schedule - called when bot connects to a server, to install any schedulers
# use $self->schedule($event, $delay, $times, $data)
# where $times is 1 for a single event, -1 for recurring events,
# and a +ve number for an event that occurs that many times.
sub Schedule {
    my $self = shift;
    my ($event) = @_;
    $self->schedule($event, \$self->{'sourceCodeCheckDelay'}, -1, {'action'=>'source'});
    $self->SUPER::Schedule($event);
}

sub Help {
    my $self = shift;
    my ($event) = @_;
    my $result = {
        'auth' => 'Authenticate yourself. Append the word \'quiet\' after your password if you don\'t want confirmation. Syntax: auth <username> <password> [quiet]',
        'password' => 'Change your password: password <oldpassword> <newpassword> <newpassword>',
        'newuser' => 'Registers a new username and password (with no privileges). Syntax: newuser <username> <newpassword> <newpassword>',
    };
    if ($self->isAdmin($event)) {
        $result->{''} = 'The administration module is used to perform tasks that fundamentally affect the bot.';
        $result->{'shutdown'} = 'Shuts the bot down completely.';
        $result->{'shutup'} = 'Clears the output queue (you actually have to say \'shutup please\' or nothing will happen).';
        $result->{'restart'} = 'Shuts the bot down completely then restarts it, so that any source changes take effect.';
        $result->{'cycle'} = 'Makes the bot disconnect from the server then try to reconnect.';
        $result->{'changepassword'} = 'Change a user\'s password: changepassword <user> <newpassword> <newpassword>',
        $result->{'vars'} = 'Manage variables: vars [<module> [<variable> [\'<value>\']]], say \'vars\' for more details.';
        $result->{'join'} = 'Makes the bot attempt to join a channel. The same effect can be achieved using /invite. Syntax: join <channel>';
        $result->{'part'} = 'Makes the bot leave a channel. The same effect can be achieved using /kick. Syntax: part <channel>';
        $result->{'load'} = 'Loads a module from disk, if it is not already loaded: load <module>';
        $result->{'unload'} = 'Unloads a module from memory: load <module>';
        $result->{'reload'} = 'Unloads and then loads a module: reload <module>';
        $result->{'bless'} = 'Sets the \'admin\' flag on a registered user. Syntax: bless <user>';
        $result->{'unbless'} = 'Resets the \'admin\' flag on a registered user. Syntax: unbless <user>';
        $result->{'deleteuser'} = 'Deletes a user from the bot. Syntax: deleteuser <username>',
    }
    return $result;
}

# Told - Called for messages prefixed by the bot's nick
sub Told {
    my $self = shift;
    my ($event, $message) = @_;
    return $self->SUPER::Told(@_) unless $self->{allowChannelAdmin} or $event->{channel} eq '';
    if ($message =~ /^\s*auth\s+($variablepattern)\s+($variablepattern)(\s+quiet)?\s*$/osi) {
        if (not $event->{'channel'}) {
            if (defined($users{$1})) {
                if (&::checkPassword($2, $users{$1})) {
                    $authenticatedUsers{$event->{'user'}} = $1;
                    if (not defined($3)) {
                        $self->directSay($event, "Hi $1!");
                    }
                    &::do($event->{'bot'}, $event->{'_event'}, 'Authed'); # hack hack hack
                } else {
                    $self->directSay($event, "No...");
                }
            } else {
                $self->directSay($event, "You have not been added as a user yet. Try the \'newuser\' command (see \'help newuser\' for details).");
            }
        }
    } elsif ($message =~ /^\s*password\s+($variablepattern)\s+($variablepattern)\s+($variablepattern)\s*$/osi) {
        if (not $event->{'channel'}) {
            if ($authenticatedUsers{$event->{'user'}}) {
                if ($2 ne $3) {
                    $self->say($event, 'New passwords did not match. Try again.');
                } elsif (&::checkPassword($1, $users{$authenticatedUsers{$event->{'user'}}})) {
                    $users{$authenticatedUsers{$event->{'user'}}} = &::newPassword($2);
                    delete($authenticatedUsers{$event->{'user'}});
                    $self->say($event, 'Password changed. Please reauthenticate.');
                    $self->saveConfig();
                } else {
                    delete($authenticatedUsers{$event->{'user'}});
                    $self->say($event, 'That is not your current password. Please reauthenticate.');
                }
            }
        }
    } elsif ($message =~ /^\s*new\s*user\s+($variablepattern)\s+($variablepattern)\s+($variablepattern)\s*$/osi) {
        if (not $event->{'channel'}) {
            if (defined($users{$1})) {
                $self->say($event, 'That user already exists in my list, you can\'t add them again!');
            } elsif ( $2 ne $3 ) {
                $self->say($event, 'New passwords did not match. Try again.');
            } elsif ($1) {
                $users{$1} = &::newPassword($2);
                $userFlags{$1} = 0;
                $self->directSay($event, "New user '$1' added with password '$2' and no rights.");
                $self->saveConfig();
            } else {
                $self->say($event, 'That is not a valid user name.');
            }
        }
    } elsif ($self->isAdmin($event)) {
        if ($message =~ /^\s*(?:shutdown,?\s+please)\s*[?!.]*\s*$/osi) {
            $self->say($event, 'But of course. Have a nice day!');
            my $reason = 'I was told to shutdown by '.$event->{'from'}.'. :-(';
            # XXX should do something like &::do($event->{'bot'}, $event->{'_event'}, 'SpottedQuit'); # hack hack hack
            #     ...but it should have the right channel/nick/reason info
            # XXX we don't unload the modules here?
            $event->{'bot'}->quit($reason);
            exit(0); # prevents any other events happening...
        } elsif ($message =~ /^\s*shutdown/osi) {
            $self->say($event, 'If you really want me to shutdown, use the magic word.');
            $self->schedule($event, 7, 1, 'i.e., please.');
        } elsif ($message =~ /^\s*(?:restart,?\s+please)\s*[?!.]*\s*$/osi) {
            $self->Restart($event, "I was told to restart by $event->{'from'} -- brb");
        } elsif ($message =~ /^\s*restart/osi) {
            $self->say($event, 'If you really want me to restart, use the magic word.');
            $self->schedule($event, 7, 1, 'i.e., please.');
        } elsif ($message =~ /^\s*delete\s*user\s+($variablepattern)\s*$/osi) {
            if (not defined($users{$1})) {
                $self->say($event, "I don't know of a user called '$1', sorry.");
            } else {
                # check user is not last admin
                my $doomedUser = $1;
                my $count;
                if (($userFlags{$doomedUser} & 1) == 1) {
                    # deleting an admin. Count how many are left.
                    $count = 0;
                    foreach my $user (keys %users) {
                        ++$count if ($user ne $doomedUser and
                                     ($userFlags{$user} & 1) == 1);
                    }
                } else {
                    # not deleting an admin. We know there is an admin in there, it's
                    # the user doing the deleting. So we're safe.
                    $count = 1;
                }
                if ($count) {
                    $self->deleteUser($doomedUser);
                    $self->say($event, "User '$doomedUser' deleted.");
                } else {
                    $self->say($event, "Can't delete user '$doomedUser', that would leave you with no admins!");
                }
            }
        } elsif ($message =~ /^\s*change\s*password\s+($variablepattern)\s+($variablepattern)\s+($variablepattern)\s*$/osi) {
            if (not defined($users{$1})) {
                $self->say($event, "I don't know of a user called '$1', sorry.");
            } elsif ($2 ne $3) {
                $self->say($event, 'New passwords did not match. Try again.');
            } else {
                $users{$1} = &::newPassword($2);
                my $count = 0;
                foreach my $user (keys %authenticatedUsers) {
                    if ($authenticatedUsers{$user} eq $1) {
                        delete($authenticatedUsers{$user});
                        ++$count;
                    }
                }
                if ($count) {
                    $self->say($event, "Password changed for user '$1'. They must reauthenticate.");
                } else {
                    $self->say($event, "Password changed for user '$1'.");
                }
                $self->saveConfig();
            }
        } elsif ($message =~ /^\s*(?:shut\s*up,?\s+please)\s*[?!.]*\s*$/osi) {
            my $lost = @msgqueue;
            @msgqueue = ();
            if ($lost) {
                $self->say($event, "Ok, threw away $lost messages.");
            } else {
                $self->say($event, 'But I wasn\'t saying anything!');
            }
        } elsif ($message =~ /^\s*cycle(?:\s+please)?\s*[?!.]*\s*$/osi) {
            my $reason = 'I was told to cycle by '.$event->{'from'}.'. BRB!';
            # XXX should do something like &::do($event->{'bot'}, $event->{'_event'}, 'SpottedQuit'); # hack hack hack
            #     ...but it should have the right channel/nick/reason info
            # XXX we don't unload the modules here?
            $event->{'bot'}->quit($reason);
            &Configuration::Get($cfgfile, &::configStructure());
        } elsif ($message =~ /^\s*join\s+([&#+][^\s]+)(?:\s+please)?\s*[?!.]*\s*$/osi) {
            $self->Invited($event, $1);
        } elsif ($message =~ /^\s*part\s+([&#+][^\s]+)(?:\s+please)?\s*[?!.]*\s*$/osi) {
            $event->{'bot'}->part("$1 :I was told to leave by $event->{'from'}. :-(");
        } elsif ($message =~ /^\s*bless\s+('?)($variablepattern)\1\s*$/osi) {
            if (defined($users{$2})) {
                $userFlags{$2} = $userFlags{$2} || 1;
                $self->saveConfig();
                $self->say($event, "Ok, $2 is now an admin.");
            } else {
                $self->say($event, 'I don\'t know that user. Try the \'newuser\' command (see \'help newuser\' for details).');
            }
        } elsif ($message =~ /^\s*unbless\s+('?)($variablepattern)\1\s*$/osi) {
            if (defined($users{$2})) {
                $userFlags{$2} = $userFlags{$2} &~ 1;
                $self->saveConfig();
                $self->say($event, "Ok, $2 is now a mundane luser.");
            } else {
                $self->say($event, 'I don\'t know that user. Check your spelling!');
            }
        } elsif ($message =~ /^\s*load\s+('?)($variablepattern)\1\s*$/osi) {
            $self->LoadModule($event, $2, 1);
        } elsif ($message =~ /^\s*reload\s+('?)($variablepattern)\1\s*$/osi) {
            $self->ReloadModule($event, $2, 1);
        } elsif ($message =~ /^\s*unload\s+('?)($variablepattern)\1\s*$/osi) {
            $self->UnloadModule($event, $2, 1);
        } elsif ($message =~ /^\s*vars(?:\s+($variablepattern)(?:\s+($variablepattern)(?:\s+'(.*)')?)?|(.*))?\s*$/osi) {
            $self->Vars($event, $1, $2, $3, $4);
        } else {
            return $self->SUPER::Told(@_);
        }
    } else {
        return $self->SUPER::Told(@_);
    }
    return 0; # if made it here then we did it!
}

sub Scheduled {
    my $self = shift;
    my ($event, $type) = @_;
    if ((ref($type) eq 'HASH') and ($type->{'action'} eq 'source')) {
        $self->CheckSource($event);
    } elsif (ref($type)) {
        $self->SUPER::Scheduled(@_);
    } else {
        $self->directSay($event, $type);
    }
}

# remove any (other) temporary administrators when an admin authenticates
sub Authed {
    my $self = shift;
    my ($event, $who) = @_;
    if ($self->isAdmin($event)) {
        foreach (keys %userFlags) {
            if ((($userFlags{$_} & 2) == 2) and ($authenticatedUsers{$event->{'user'}} ne $_)) {
                $self->deleteUser($_);
                $self->directSay($event, "Temporary administrator '$_' removed from user list.");
            }
        }
    }
    return $self->SUPER::Authed(@_); # this should not stop anything else happening
}

# SpottedQuit - Called when someone leaves a server
sub SpottedQuit {
    my $self = shift;
    my ($event, $who, $why) = @_;
    delete($authenticatedUsers{$event->{'user'}});
    # XXX this doesn't deal with a user who has authenticated twice.
    return $self->SUPER::SpottedQuit(@_);
}

sub CheckSource {
    my $self = shift;
    my ($event) = @_;
    foreach my $file (@{$self->{'files'}}) {
        my $lastModifiedTime = $self->{'_fileModifiedTimes'}->{$file};
        my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime, $blksize, $blocks)
            = stat($file);
        $self->{'_fileModifiedTimes'}->{$file} = $mtime;
        if (defined($lastModifiedTime) and ($mtime > $lastModifiedTime)) {
            $self->debug("Noticed that source code of $file had changed");
            # compile new bot using perl -cwT XXX
            if (1) { # XXX replace 1 with "did compile succeed" test
                $self->Restart($event, 'someone seems to have changed my source code. brb, unless I get a compile error!');
            } else {
                # tellAdmin that it did not compile XXX
                # debug that it did not compile
            }
        }
    }
    my @updatedModules;
    foreach my $module (@modules) {
        if ($module->{'_filename'}) {
            my $lastModifiedTime = $module->{'_fileModificationTime'};
            my ($dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime, $mtime, $ctime, $blksize, $blocks)
                = stat($module->{'_filename'});
            $module->{'_fileModificationTime'} = $mtime;
            if (defined($lastModifiedTime) and ($mtime > $lastModifiedTime)) {
                push(@updatedModules, $module->{'_name'});
            }
        }
    }
    foreach my $module (@updatedModules) {
        $self->ReloadModule($event, $module, 0);
    }
}

sub Restart {
    my $self = shift;
    my ($event, $reason) = @_;
    # XXX should do something like &::do($event->{'bot'}, $event->{'_event'}, 'SpottedQuit'); # hack hack hack
    #     ...but it should have the right channel/nick/reason info
    #     ...and it is broken if called from CheckSource, which is a
    #     scheduled event handler, since $event is then a very basic
    #     incomplete hash.
    # XXX we don't unload modules here?
    $event->{'bot'}->quit($reason);
    # Note that `exec' will not call our `END' blocks, nor will it
    # call any `DESTROY' methods in our objects. So we fork a child to
    # do that first.
    my $parent = $$;
    my $child = fork();
    if (defined($child)) {
        if ($child) {
            # we are the parent process who is
            # about to exec($0), so wait for
            # child to shutdown.
            $self->debug("spawned $child to handle shutdown...");
            waitpid($child, 0);
        } else {
            # we are the child process who is
            # in charge of shutting down cleanly.
            $self->debug("initiating shutdown for parent process $parent...");
            exit(0);
        }
    } else {
        $self->debug("failed to fork: $!");
    }
    $self->debug("About to defer to a new $0 process...");
    # we have done our best to shutdown, so go for it!
    eval {
        $0 =~ m/^(.*)$/os; # untaint $0 so that we can call it below (as $1)
        if ($CHROOT) {
            exec { $1 } ($1, '--assume-chrooted', $cfgfile);
        } else {
            exec { $1 } ($1, $cfgfile);
        }
        # I am told (by some nice people in #perl on Efnet) that our
        # memory is all cleared up for us. So don't worry that even
        # though we don't call DESTROY in _this_ instance, we leave
        # memory behind.
    };
    $self->debug("That failed!!! Bailing out to prevent all hell from breaking loose! $@ :-|");
    exit(1); # we never get here unless exec fails
}

# handles the 'vars' command
sub Vars {
    my $self = shift;
    my ($event, $modulename, $variable, $value, $nonsense) = @_;
    if (defined($modulename)) {
        my $module = $self->getModule($modulename);
        if (defined($module)) {
            if (defined($variable)) {
                if (defined($value)) {
                    my $result = $module->Set($event, $variable, $value);
                    if ((not defined($result)) or ($result == 0)) {
                        $self->say($event, "Variable '$variable' in module '$modulename' has changed.");
                    } elsif ($result == 1) {
                        $self->say($event, "Variable '$variable' is of type ".ref($module->{$variable}).' and I do not know how to set that kind of variable!');
                    } elsif ($result == 2) { # we don't know that variable!
                        if ($module->{$variable}) { # well we do, but only to read
                            $self->say($event, "Variable '$variable' in module '$modulename' is read-only, sorry.");
                        } else { # not known
                            $self->say($event, "Module '$modulename' does not have a variable '$variable' as far as I can tell.");
                        }
                    } elsif ($result == 3) {
                        $self->say($event, "Variable '$variable' is a list. To add to a list, please use the '+' symbol before the value (vars <module> <variable> '+<value>'). To remove from a list, use the '-' symbol (vars <module> <variable> '-<value>').");
                    } elsif ($result == 4) {
                        $self->say($event, "Variable '$variable' is a hash. To add to a hash, please use the '+' symbol before the '|key|value' pair (vars <module> <variable> '+|<key>|<value>').  The separator symbol ('|' in this example) could be anything. To remove from a list, use the '-' symbol (vars <module> <variable> '-<key>').");
                    } elsif ($result == -1) {
                        # already reported success
                    } elsif ($result == -2) {
                        $self->say($event, "Variable '$variable' in module '$modulename' has changed, but may not be what you expect since it appears to me that you used a letter to delimit the sections. I hope that is what you meant to do...");
                    } elsif ($result > 0) { # negative = success
                        $self->say($event, "Variable '$variable' in module '$modulename' could not be set for some reason unknown to me.");
                    }
                } else { # else give variable's current value
                    $value = $module->Get($event, $variable);
                    if (defined($value)) {
                        my $type = ref($value);
                        if ($type eq 'SCALAR') {
                            $self->say($event, "Variable '$variable' in module '$modulename' is set to: '$$value'");
                        } elsif ($type eq 'ARRAY') {
                            # XXX need a 'maximum number of items' feature to prevent flooding ourselves to pieces (or is shutup please enough?)
                            if (@$value) {
                                local $" = '\', \'';
                                $self->say($event, "Variable '$variable' in module '$modulename' is a list with the following values: '@$value'");
                            } else {
                                $self->say($event, "Variable '$variable' in module '$modulename' is an empty list.");
                            }
                        } elsif ($type eq 'HASH') {
                            # XXX need a 'maximum number of items' feature to prevent flooding ourselves to pieces (or is shutup please enough?)
                            $self->say($event, "Variable '$variable' in module '$modulename' is a hash with the following values:");
                            foreach (sort keys %$value) {
                                $self->say($event, "  '$_' => '".($value->{$_}).'\' ');
                            }
                            $self->say($event, "End of dump of variable '$variable'.");
                        } else {
                            $self->say($event, "Variable '$variable' in module '$modulename' is set to: '$value'");
                        }
                    } else { # we don't know that variable
                        if ($module->{'_variables'}->{$variable}) { # well we do, but only to write
                            $self->say($event, "Variable '$variable' in module '$modulename' is write-only, sorry.");
                        } else { # not known
                            $self->say($event, "Module '$modulename' does not have a variable '$variable' as far as I can tell.");
                        }
                    }
                }
            } else { # else list variables
                my @variables;
                # then enumerate its variables
                foreach my $variable (sort keys %{$module->{'_variables'}}) {
                    push(@variables, $variable) if $module->{'_variables'}->{$variable};
                }
                # then list 'em
                if (@variables) {
                    local $" = '\', \'';
                    $self->say($event, "Module '$modulename' has the following published variables: '@variables'");
                } else {
                    $self->say($event, "Module '$modulename' has no settable variables.");
                }
            }
        } else { # complain no module
            $self->say($event, "I didn't recognise that module name ('$modulename'). Try just 'vars' on its own for help.");
        }
    } elsif ($nonsense) {
        $self->say($event, 'I didn\'t quite understand that. Try just \'vars\' on its own for help.');
        $self->say($event, 'If you are trying to set a variable, don\'t forget the quotes around the value!');
    } else { # else give help
        $self->say($event, 'The \'vars\' command gives you an interface to the module variables in the bot.');
        $self->say($event, 'To list the variables in a module: vars <module>');
        $self->say($event, 'To get the value of a variable: vars <module> <variable>');
        $self->say($event, 'To set the value of a variable: vars <module> <variable> \'<value>\'');
        $self->say($event, 'Note the quotes around the value. They are required. If the value contains quotes itself, that is fine.');
    }
}

# This is also called when we are messaged a 'join' command
sub Invited {
    my $self = shift;
    my ($event, $channelName) = @_;
    # $channelName is the name as requested and as should be /joined.
    # This is important so that case is kept in the list of channels
    # on the server should the bot join first.
    my $channel = lc($channelName);
    if (grep $_ eq $channel, @channels) {
        $self->directSay($event, "I thought I was already *in* channel $channel! Oh well.");
    }
    if ($self->isAdmin($event) || $self->{'allowInviting'}) {
        $self->debug("Joining $channel, since I was invited.");
        if (defined($channelKeys{$channel})) {
            $event->{'bot'}->join($channel, $channelKeys{$channel});
        } else {
            $event->{'bot'}->join($channel);
        }
    } else {
        $self->debug($event->{'from'}." asked me to join $channel, but I refused.");
        $self->directSay($event, "Please contact one of my administrators if you want me to join $channel.");
        $self->tellAdmin($event, "Excuse me, but ".$event->{'from'}." asked me to join $channel. I thought you should know.");
    }
    return $self->SUPER::Invited($event, $channel);
}

sub Kicked {
    my $self = shift;
    my ($event, $channel) = @_;
    $self->debug("kicked from $channel by ".$event->{'from'});
    $self->debug('about to autopart modules...');
    foreach (@modules) {
        $_->PartedChannel($event, $channel);
    }
    return $self->SUPER::Kicked($event, $channel);
}

sub SpottedPart {
    my $self = shift;
    my ($event, $channel, $who) = @_;
    if (lc $who eq lc $event->{'nick'}) {
        $self->debug("parted $channel");
        $self->debug('about to autopart modules...');
        foreach (@modules) {
            $_->PartedChannel($event, $channel);
        }
    }
    return $self->SUPER::SpottedPart($event, $channel, $who);
}

sub PartedChannel {
    my $self = shift;
    my ($event, $channel) = @_;
    $channel = lc($channel);
    my %channels = map { $_ => 1 } @channels;
    delete($channels{$channel});
    @channels = keys %channels;
    &Configuration::Save($cfgfile, &::configStructure(\@channels));
    return $self->SUPER::PartedChannel($event, $channel);
}

sub LoadModule {
    my $self = shift;
    my ($event, $name, $requested) = @_;
    my $newmodule = &::LoadModule($name);
    if (ref($newmodule)) {
        # configure module
        $newmodule->{'channels'} = [@channels];
        &Configuration::Get($cfgfile, $newmodule->configStructure());
        eval {
            $newmodule->Schedule($event);
        };
        if ($@) {
            $self->debug("Warning: An error occured while loading the module:\n$@");
            if ($requested) {
                $self->say($event, "Warning: an error occured while loading module '$name'. Ignored.");
            }
        }
        $newmodule->saveConfig();
        $self->debug("Successfully loaded module '$name'.");
        if ($requested) {
            $self->say($event, "Loaded module '$name'.");
        }
    } else {
        if ($requested) { # it failed, $newmodule contains error message
            my @errors = split(/[\n\r]/os, $newmodule);
            if (scalar(@errors) > $self->{'errorMessagesMaxLines'}) {
                # remove lines from the middle if the log is too long
                @errors = (@errors[0..int($self->{'errorMessagesMaxLines'} / 2)-1], '...', @errors[-(int($self->{'errorMessagesMaxLines'} / 2))..-1]);
            }
            local $" = "\n";
            $self->say($event, "@errors");
        }
        $self->debug($newmodule);
    }
}

sub UnloadModule {
    my $self = shift;
    my ($event, $name, $requested) = @_;
    my $result = &::UnloadModule($name);
    if (defined($result)) { # failed
        if ($requested) {
            $self->say($event, $result);
        } else {
            $self->debug($result);
        }
    } else {
        if ($requested) {
            $self->say($event, "Unloaded module '$name'.");
        } else {
            $self->debug("Successfully unloaded module '$name'.");
        }
    }
}

sub ReloadModule {
    my $self = shift;
    # XXX there used to be a memory leak around this code. It seems to be fixed
    # now. However if your bot process suddenly balloons to 90M+, here would be a good
    # place to start looking. Of course if that happens and you never reloaded modules
    # then it is also a good time to remove this comment... ;-)
    $self->UnloadModule(@_);
    $self->LoadModule(@_);
}

sub deleteUser {
    my $self = shift;
    my ($who) = @_;
    delete($userFlags{$who});
    delete($users{$who});
    # if they authenticated, remove the entry to prevent dangling links
    foreach my $user (keys %authenticatedUsers) {
        if ($authenticatedUsers{$user} eq $who) {
            delete($authenticatedUsers{$user});
        }
    }
    $self->saveConfig();
}


################################
# Startup (aka main)           #
################################

package main;

# -- #mozilla was here --
#       <zero> is the bug with zilla hanging on startup on every
#              platform fixed in today's nightlies?
#       <leaf> no
#      <alecf> heh
#       <leaf> NEVER
#       <leaf> we're shipping with it.
#    <andreww> helps hide our other bugs

# Do this at the very end, so we can intersperse "my" initializations outside
# of routines above and be assured that they will run.

&debug('starting up command loop...');

END { &debug('perl is shutting down...'); }

$irc->start();

# -- #mozilla was here --
#      <alecf> Maybe I'll file a bug about netcenter and that will
#              get some attention
#      <alecf> "Browser won't render home.netscape.com.. because it
#              won't start up"
#    <andreww> alecf how about "cant view banner ads - wont start up"
#      <alecf> even better
#  <pinkerton> all bugs are dependent on this one!

# *** Disconnected from irc.mozilla.org
