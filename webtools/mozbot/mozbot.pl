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
# grabs mozillaZine headlines. fetches slashdot.org
# news. bot will auto-op based on nick and remote host.
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

use strict;
use diagnostics;
use lib ".";
use Net::IRC;
use LWP::Simple;
use Tinderbox;
use Carp;
use Chatbot::Eliza;

$|++;

my $VERSION = "1.30"; # keep me in sync with the mozilla.org cvs repository
my $debug = 1; # debug output also includes warnings, errors

my %msgcmds = (
               "url" => \&bot_urls,
               );

my %pubcmds = (
               "(help|about)" => \&bot_about,
               "(hi|hello|lo|sup)" => \&bot_hi,
               "moon" => \&bot_moon,
               "up" => \&bot_up,
               "trees" => \&bot_tinderbox,
               "(slashdot|sd|\/\.)" => \&bot_slashdot,
               "(freshmeat|fm)" => \&bot_freshmeat,,
               "(mozillazine|zine|mz)" => \&bot_mozillazine,
               "(mozillaorg|mozilla|mo)" => \&bot_mozillaorg,
               "debug" => \&bot_debug,
               "stocks" => \&bot_stocks,
               );

my %admincmds = (
                 "bless" => \&bot_bless,
                 "unbless" => \&bot_unbless,
                 "shutdown" => \&bot_shutdown,
                 "say" => \&bot_say,
                 "list" => \&bot_list,
                 );

@::origargv = @ARGV;

my $server = shift;
my $port = shift;
my $nick = shift;
my $channel = shift;

my $stockf = "stocklist";
my %stocklist = ();
my %stockvals = ();
my %stockhist;

$server = $server               || "irc.mozilla.org";
$port = $port                   || "6667";
$nick = $nick                   || "mozbot";
$channel = $channel             || "#mozilla";

&debug ("mozbot $VERSION starting up");

LoadStockList();

&create_pid_file;

# read admin list 
my %admins = ( "sar" => "netscape.com", "terry" => "netscape.com" );
my $adminf = ".mozbot-admins";
&fetch_admin_conf (\%admins);

my $uptime = 0;

$::moon = "./moon";
$::moon = (-f $::moon) ? $::moon : ""; 
delete $pubcmds{'moon'} if (! $::moon);

my $phase;
my $last_moon = 0;

# leave @trees empty if you don't want tinderbox details

my @trees = qw (SeaMonkey);
if ($nick =~ /grend/) {
    @trees = qw (Grendel);
}
my $trees;
my $status;
my $last_tree;
my %broken;
my @urls;

my $greet = 0;
my @greetings = 
	( 
	"g'day", "bonjour", "guten tag", "moshi, moshi",
	"hello", "hola", "hi", "buono giorno", "aloha",
	"hey", "'sup", "lo", "howdy", "saluton", "hei",
	"hallo", "word", "yo yo yo", "rheeet", "bom dia",
	"ciao"
	);

# leave $slashdot tuned to undef if you don't want slashdot
# headlines checked every two hours

my $slashdot = "http://slashdot.org/ultramode.txt";
my @slashdot;
my $last_slashdot = 0;

# leave $freshmeat tuned to undef if you don't want freshmeat
# headlines checked every two hours

my $freshmeat = "http://freshmeat.net/files/freshmeat/recentnews.txt";
my @freshmeat;
my $last_freshmeat = 0;

# leave $mozillazine undef'd if you don't want mozillazine
# headlines checked every eight hours

my $mozillazine = "http://www.mozillazine.org/index.html";
my @mozillazine;
my $last_mozillazine = 0;


my $mozillaorg = "http://www.mozilla.org/news.txt";
my @mozillaorg;
my $last_mozillaorg = 0;


my $irc = new Net::IRC or confess "$0: duh?";

my $bot = $irc->newconn
  (
  Server => $server,
  Port => $port,
  Nick => $nick,
  Ircname => "mozilla.org bot/thing $VERSION",
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
$bot->add_handler ('public', \&on_public);
$bot->add_handler ('join',   \&on_join);

&debug ("scheduling stuff");
$bot->schedule (0, \&tinderbox);
$bot->schedule (0, \&checksourcechange);
$bot->schedule (0, \&mozillazine);
$bot->schedule (0, \&mozillaorg);
$bot->schedule (0, \&slashdot);
# $bot->schedule (0, \&freshmeat);
$bot->schedule (0, \&stocks);

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


sub do_command {
    my ($hashref, $nick, $cmd, $rest) = (@_);
    foreach my $m (keys %$hashref) {
        if ($cmd =~ m/^$m$/) {
            &{$hashref->{$m}} ($nick, $cmd, $rest);
            return 1;
        }
    }
    return 0;
}



# on_msg: private message received via /msg

sub on_msg {
    my ($self, $event) = @_;
    my ($nick) = $event->nick;
    my ($arg) = $event->args;
    my @arglist = split(' ', $arg);
    my $cmd = shift @arglist;
    my $rest = join(' ', @arglist);
    $::speaker = $nick;         # Hack!!!


    if (exists $admins{$nick}) {
        if (do_command(\%admincmds, $nick, $cmd, $rest)) {
            return;
        }
    }
    if (do_command(\%pubcmds, $nick, $cmd, $rest)) {
        return;
    }
    if (do_command(\%msgcmds, $nick, $cmd, $rest)) {
        return;
    }
    do_unknown($nick, $cmd, $rest);
}



sub bot_bless {
    my ($nick, $cmd, $rest) = (@_);
    my ($who, $where) = split(' ', $rest);
    if (! $who or ! $where) {
        $bot->privmsg ($nick, "usage: bless [ user ] [ host ] " . 
						"(example: bless marca netscape.com)");
        return;
    }
    $admins{$who} = $where;
    &debug ("$nick blessed $who ($where)");
    &store_admin_conf (\%admins);
    $bot->privmsg ($nick, 
					"mozbot admins: " . join ' ', (sort keys %admins));
}

sub bot_unbless {
    my ($nick, $cmd, $rest) = (@_);
    my ($who) = ($rest);
    if (exists ($admins{$who})) {
        delete $admins{$who};
        &debug ("$nick unblessed $who");
        &store_admin_conf (\%admins);
        $bot->privmsg ($nick, 
                        "mozbot admins: " . join ' ', (sort keys %admins));
        return;
    }
    $bot->privmsg($nick, "Can only unbless one of: " .
                   join(' ', (sort keys %admins)));
}

sub bot_shutdown {
    my ($nick, $cmd, $rest) = (@_);
    if ($rest ne "yes") {
        $bot->privmsg ($nick, "usage: shutdown yes");
        return;
    }
    &debug ("forced shutdown from $nick");
    $::dontQuitOnSignal++;
    $bot->quit ("$nick told me to shutdown");
    exit (0);
}


sub bot_say {
    my ($nick, $cmd, $rest) = (@_);
    my $text = $rest;
    if ($text =~ m@^/me (.*)@) {
        $bot->me($channel, $1);
    } else {
        $bot->privmsg ($channel, $text);
    }
}


sub bot_list {
    my ($nick, $cmd, $rest) = (@_);
    foreach (sort keys %admins) {
        $bot->privmsg ($nick, "$_ $admins{$_}");
    }
}




sub on_public {
    my ($self, $event) = @_;
    my ($to) = $event->to;
    my ($arg) = $event->args;
    my ($nick, $me) = ($event->nick, $self->nick);
    $::speaker = $nick;         # Hack!!!
    
# catch urls, stick them in a list for mozbot's url command

    if ($arg =~ /(http|ftp|gopher):/i && $nick ne $me) {
        push @urls, "$arg (" . &logdate() . ")";
        while ($#urls > 10) {
            shift @urls;
        }
    }
    if (my ($cmd, $rest) = $arg =~ /^$me[:,]?\s+(\S+)(?:\s+(.*))?$/i) {
        if (do_command(\%pubcmds, $channel, $cmd, $rest)) {
            return;
        } else {
            do_unknown($channel, $cmd, $rest);
        }
    }
}


sub do_unknown {
    my ($nick, $cmd, $rest) = (@_);
    if (!defined $::eliza) {
        $::eliza = new Chatbot::Eliza;
    }
    my $result = $::eliza->transform("$cmd $rest");
    $bot->privmsg($nick, $result);
}


sub saylongline {
    my ($nick, $str, $spacer) = (@_);
    my $MAXPROTOCOLLENGTH = 255;

    while (length ($str) > $MAXPROTOCOLLENGTH) {
        my $pos;
        $pos = rindex($str, $spacer, $MAXPROTOCOLLENGTH - length($spacer));
        if ($pos < 0) {
            $pos = rindex($str, " ", $MAXPROTOCOLLENGTH - 1);
            if ($pos < 0) {
                $pos = $MAXPROTOCOLLENGTH - 1;
            }
        }
        $bot->privmsg($nick, substr($str, 0, $pos));
        $str = substr($str, $pos);
        if (index($str, $spacer) == 0) {
            $str = substr($str, length($spacer));
        }
    }
    if ($str ne "") {
        $bot->privmsg($nick, $str);
    }
}




sub do_headlines {
    my ($nick, $header, $ref) = (@_);
    my $spacer = " ... ";
    my $str = $header . ": " . join($spacer, @$ref);
    saylongline($nick, $str, $spacer);
}

sub reportDiffs {
    my ($name, $url, $ref) = (@_);

    my $firsttime = 0;
    if (!exists $::headCache{$name}) {
        $firsttime = 1;
        $::headCache{$name} = {};
    }
    my $spacer = " ... ";
    my $outstr = "";
    foreach my $i (@$ref) {
        if ($i =~ /^last update/) {
            next;
        }
        if (!exists $::headCache{$name}->{$i}) {
            $::headCache{$name}->{$i} = 1;
            if ($outstr eq "") {
                $outstr = "Just appeared in $name ($url): ";
            } else {
                $outstr .= $spacer;
            }
            $outstr .= $i;
        }
    }
    if (!$firsttime) {
        saylongline($channel, $outstr, $spacer);
    }
}
    
        

        
    
sub bot_debug
	{
	my ($nick, $cmd, $rest) = (@_);
	
	my @list;
	my %last = 
		(
		"slashdot" => $last_slashdot,
		"freshmeat" => $last_freshmeat,
		"mozillazine" => $last_mozillazine,
		"mozillaorg" => $last_mozillaorg,
		"tinderbox" => $last_tree,
		"moon" => $last_moon,
		);
	
	foreach (keys %last)
		{
		if ($last{$_} != 0)
			{
			push @list, "$_ updated: " . &logdate ($last{$_}) . ", " . 
				&days ($last{$_});
			}
		else
			{
			push @list, "$_ never updated!";
			}
		}

	do_headlines ($nick, "Boring Debug Information", \@list);
	}

sub bot_slashdot {
    my ($nick, $cmd, $rest) = (@_);
    do_headlines($nick, "Headlines from Slashdot (http://slashdot.org/)", \@slashdot);
}

sub bot_freshmeat {
    my ($nick, $cmd, $rest) = (@_);
    do_headlines($nick, "Today in freshmeat (http://freshmeat.net/)", \@freshmeat);
}


sub bot_mozillazine{
    my ($nick, $cmd, $rest) = (@_);
    do_headlines($nick,
                 "Headlines from mozillaZine (http://www.mozillazine.org/)",
                 \@mozillazine);
}

sub bot_mozillaorg {
    my ($nick, $cmd, $rest) = (@_);
    do_headlines($nick,
                 "Headlines from mozilla.org (http://www.mozilla.org/)",
                 \@mozillaorg);
}


sub bot_hi {
    my ($nick, $cmd, $rest) = (@_);
    $bot->privmsg($nick, $greetings[$greet++] . " $::speaker");
    $greet = 0 if ($greet > $#greetings);
}

 

sub on_join
	{
  my ($self, $event) = @_;
  my ($channel) = ($event->to)[0];
  my $nick = $event->nick;
	my $userhost = $event->userhost;
	
	# auto-op if user is a mozbot admin and coming in from
	# the right host 

	if (exists $admins{$nick} && $userhost =~ /$admins{$nick}$/i)
		{
		$self->mode ($channel, "+o", $nick);
		&debug ("auto-op for $nick on $channel");
		}
	}

$::dontQuitOnSignal = 0;
sub on_boot
    {
        if (!$::dontQuitOnSignal) {
            die "$0: disconnected from network";
        }
    }


sub listcmds {
    my ($hashref) = (@_);
    my @list;
    foreach my $k (keys %$hashref) {
        if ($k =~ m/^\(([a-z]+)\|/) {
            push @list, $1;
        } else {
            push @list, $k;
        }
    }
    return join(' ', sort(@list));
}





################
# bot commands #
################

# bot_about: it's either an about box or the 
# address of the guy to blame when the bot 
# breaks

sub bot_about {
    my ($nick, $cmd, $rest) = @_;
    $bot->privmsg($::speaker,  "i am mozbot version $VERSION. hack on me! " .
        "harrison\@netscape.com 10/16/98. " .
        "connected to $server since " .
                  &logdate ($uptime) . " (" . &days ($uptime) . "). " .
				"see http://cvs-mirror.mozilla.org/webtools/bonsai/cvsquery.cgi?branch=HEAD&file=mozilla/webtools/mozbot/&date=week " .
                  "for a changelog.");
    $bot->privmsg($::speaker, "Known commands are: " .
                  listcmds(\%pubcmds));
    $bot->privmsg($::speaker, "If you /msg me, I'll also respond to: " .
                  listcmds(\%msgcmds));
    if (exists $admins{$::speaker}) {
        $bot->privmsg($::speaker, "And you're an admin, so you can also do: " .
                      listcmds(\%admincmds));
    }
    if ($nick eq $channel) {
        $bot->privmsg($nick, "[ Directions on talking to me have been sent to $::speaker ]");
    }

}

# bot_moon: goodnight moon

sub get_moon_str
    {
    return "- no moon -" if (! defined $::moon); 
    return $phase if ($phase && (time - $last_moon > (60 * 60 * 24)));
    
    # we only want to run this once/day
    $phase = `$::moon`;
    $last_moon = time;
    return $phase;
    }

sub bot_moon {
    my ($nick, $cmd, $rest) = @_;
    $bot->privmsg($nick, get_moon_str());
}


# bot_up: report uptime

sub bot_up {
    my ($nick, $cmd, $rest) = @_;
    $bot->privmsg($nick,  &logdate ($uptime) . " (" . &days ($uptime) . ")");
}

# bot_urls: show last ten urls caught by mozbot

sub bot_urls {
    my ($nick, $cmd, $rest) = @_;
    if ($#urls == -1) {
        $bot->privmsg($nick, "- mozbot has seen no URLs yet -");
    } else {
        foreach my $m (@urls) {
            $bot->privmsg($nick, $m);
        }
    }
}

# show tinderbox status
#
# this is a messy little function but it works. 

sub bot_tinderbox {
    my ($nick, $cmd, $rest) = @_;
    my $bustage;
    my $buf;
    my @buf;
    my @tree;

    my $terse = (defined $rest && $rest eq "all");
    if ($nick eq $channel) {
        $terse = 1;
    }
    
    # user can supply a list of trees separated
    # by whitespace, default is all trees

    push @tree, $rest ? (split /\s+/, $rest) : @trees;
    
    # loop through requested trees

    push @buf, "Tinderbox status from http://cvs-mirror.mozilla.org/webtools/tinderbox/showbuilds.cgi";

    foreach my $t (@tree)
        {
        $bustage = 0;
        $buf = "$t " . ($$status{$t} ? "<$$status{$t}> " : "") . ": ";
        
        # politely report failures
        if (! exists $$trees{$t})
            {
            $buf .= "unknown tree \"$t\", trees include @trees. ";
            }
        else
            {
            foreach my $e (sort keys %{$$trees{$t}})
                {
                next if ($terse && $$trees{$t}{$e} ne "Horked");
                $buf .= "[$e: $$trees{$t}{$e}] ";
                $bustage++;
                }
            }
        
        $buf .= "- no known bustage -" if (! $bustage);
        
        push @buf, $buf;
        }

    $buf = $buf || 
			"something broke. report a bug here: " .
			"http://bugzilla.mozilla.org/enter_bug.cgi " .
			"with product of Webtools and component set to Mozbot";

    push @buf, "last update: " .
        &logdate ($last_tree) . " (" . &days ($last_tree) . " ago)";


    foreach my $m (@buf) {
         $bot->privmsg($nick, $m);
    }

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

# write admin list 

sub store_admin_conf
	{
	my $admins = shift;
	my $when = localtime (time) . " by $$";

	if (open ADMINS, ">$adminf")
		{
		print ADMINS <<FIN;
# mozbot admin list file
#
# this file is generated. do not edit.
# generated $when 
#
# version: 1.0

FIN

		foreach (sort keys %admins)
			{
			print ADMINS "$_ $admins{$_}\n";
			}
		close ADMINS;
		}
	else
		{
		&debug ("&store_admin_conf $adminf: $!");
		}
	}

# fetch list of admins

sub fetch_admin_conf
	{
	my $admins = shift;

	if (open ADMINS, $adminf)
		{
		while (<ADMINS>)
			{
			chomp;
			next if ($_ =~ /^#/ or ! $_);
			my ($user, $host) = split /\s+/, $_;
			$$admins{$user} = $host;
			}
		&debug ("admins: " . keys %$admins);
		}
	else
		{
		&debug ("&fetch_admin_conf $adminf: $!");
		}

	close ADMINS;
	}

# create a pid file if we can

sub create_pid_file
	{
	my $pid = ".mozbot-pid";

	if (open PID, ">$pid")
    {
    print PID "$$\n";
    close PID;
    }
	else
    {
    &debug ("warning: problem creating pid file: $pid, $!");
    }
	}

sub slashdot
{
	return if (! defined $slashdot);
	&debug ("fetching slashdot headlines");

    $bot->schedule (60*60 + 30, \&slashdot);
    my $output = get $slashdot;
	$last_slashdot = time;
    return if (! $output);
    my @sd = split /\n/, $output;

    @slashdot = ();

    foreach my $i (0 .. $#sd)
    {
        push @slashdot, $sd[$i+1] if ($sd[$i] eq "%%" && $i != $#sd);
    }
	push @slashdot, "last updated: " . &logdate ($last_slashdot);

    reportDiffs("SlashDot", "http://www.slashdot.org/", \@slashdot);

}

sub freshmeat
{
	return if (! defined $freshmeat);
	&debug ("fetching freshmeat headlines");

    $bot->schedule (60*60 + 30, \&freshmeat);
    my $output = get $freshmeat;
	$last_freshmeat = time;
    return if (! $output);
    my @sd = split /\n/, $output;

    @freshmeat = ();

    foreach my $i (0 .. $#sd)
    {
        push @freshmeat, $sd[$i] if ($i % 3 == 0);
    }
	push @freshmeat, "last updated: " . &logdate ($last_freshmeat);

    reportDiffs("FreshMeat", "http://freshmeat.net/", \@freshmeat);

}

# fetches headlines from mozillaZine
#
# this should be a more general feature, to grab
# content. if you feel like it, implement a 
# grabber for slashdot headlines:
#
# http://slashdot.org/ultramode.txt

sub mozillazine
	{
	return if (! defined $mozillazine);
	&debug ("fetching mozillazine headlines");
	
	my $output = get $mozillazine;
	return if (! $output);
	$last_mozillazine = time;
	my @mz = split /\n/, $output;
	
	@mozillazine = ();

	foreach (@mz)
		{
		if (m@<!--head-->([^<>]+)<!--head-end-->@)
			{
			my $h = $1;
            $h =~ s/&nbsp;//g;
			push @mozillazine, $h; 
			}
		}

    $bot->schedule (60 * 60 + 60, \&mozillazine);
	push @mozillazine, "last updated: " . &logdate ($last_mozillazine);
    reportDiffs("mozillaZine", "http://www.mozillazine.org/", \@mozillazine);
	}

sub mozillaorg
{
	return if (! defined $mozillaorg);
	&debug ("fetching mozilla.org headlines");

    $bot->schedule (60*60, \&mozillaorg);
    my $output = get $mozillaorg;
	$last_mozillaorg = time;
    return if (! $output);
    my @m = split /\n/, $output;

    @mozillaorg = ();

    foreach my $i (@m) {
        if ($i =~ /^summary:(.*)$/) {
            push @mozillaorg, $1;
        }
    }
	push @mozillaorg, "last updated: " . &logdate ($last_mozillaorg);

    reportDiffs("mozilla.org", "http://www.mozilla.org/", \@mozillaorg);

}

# fetch tinderbox details

sub tinderbox
    {
    &debug ("fetching tinderbox status");
    my ($newtrees, $newstatus) = Tinderbox::status (\@trees);

		if (! $newtrees)
			{
    	$bot->schedule (90, \&tinderbox);
			&debug ("hmm, couldn't get tinderbox status");
			return;
			}

    $last_tree = time;

		if (defined $status)
			{
			foreach my $s (keys %$newstatus)
				{
				if (defined $$newstatus{$s} && $$status{$s} ne $$newstatus{$s})
					{
					$bot->privmsg ($channel,
						"$s changed state from $$status{$s} to $$newstatus{$s}");
					}
				}
			}

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
		$status = $newstatus;
    
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

    if (defined $lastourdate && 
			($::ourdate > $lastourdate ||
                             $::tinderboxdate > $lasttinderboxdate)) {
        $::dontQuitOnSignal = 1;
        $self->quit("someone seems to have changed my source code.  Be right back");
        &debug ("restarting self");
        exec "$0 @::origargv";
    }
    $bot->schedule (60, \&checksourcechange);
}


sub stocks {
    $bot->schedule(15 * 60, \&stocks);

    my $url = "http://quote.yahoo.com/d/quotes.csv?f=sl1d1t1c1ohgv&e=.csv&s=" .
        join("+", sort(keys %stocklist));
    &debug ("fetching stock quotes");
    my $output = get $url;
    return if (!$output);
    %stockvals = ();
    foreach my $line (split(/\n/, $output)) {
        my @list = split(/,/, $line);
        my $name = shift(@list);
        $name =~ s/"(.*)"/$1/;
        &debug ("parsing stock quote $name ($list[0])");
        $stockvals{$name} = \@list;
        foreach my $ref (@{$stockhist{$name}}) {
            my $oldval = $ref->[0];
            my $newval = $list[0];
            my $ratio = $newval / $oldval;
            if ($ratio > 1.05 || $ratio < 0.95) {
                foreach my $who ($stocklist{$name}) {
                    ReportStock($who, $name, "Large Stock Change");
                }
                $stockhist{$name} = [];
                last;
            }
        }
        if (!exists $stockhist{$name}) {
            $stockhist{$name} = [];
        }
        push (@{$stockhist{$name}}, \@list);
        while (30 < @{$stockhist{$name}}) {
            shift @{$stockhist{$name}};
        }
    }
}


sub LoadStockList {
     %stocklist = ("AOL" => [$channel]);

     if (open(LIST, $stockf)) {
         %stocklist = ();
         while (<LIST>) {
             my @list = split(/\|/, $_);
             my $name = shift(@list);
             $stocklist{$name} = \@list;
         }
     }
 }


sub FracStr {
    my ($num, $needplus) = (@_);
    my $sign;
    if ($num < 0) {
        $sign = "-";
        $num = - $num;
    } else {
         $sign = $needplus ? "+" : "";
     }
    
    my $bdot = int($num);
    my $adot = $num - $bdot;

    if ($adot == 0) {
        return "$sign$bdot";
    }

    my $base = 64;
    $num = int($adot * $base);

    while ($num % 2 == 0 && $base > 1) {
        $base /= 2;
        $num /= 2;
    }

    if ($adot == $num / $base) {
        if ($bdot == 0) {
            $bdot = "";
        } else {
            $bdot .= " ";
        }
        return "$sign$bdot$num/$base";
    }
}



sub ReportStock {
    my ($nick, $name, $title) = (@_);
    if ($title ne "") {
        $title .= ": ";
    }
    my $ref = $stockvals{$name};
    my $a = FracStr($ref->[0], 0);
    my $b = FracStr($ref->[3], 1);
    $bot->privmsg($nick, "$title$name at $a ($b)");
}

sub bot_stocks {
    my ($nick, $cmd, $rest) = (@_);
    foreach my $name (sort(keys %stocklist)) {
        foreach my $who (@{$stocklist{$name}}) {
            if ($who eq $nick || $who eq $channel) {
                ReportStock($nick, $name, "");
            }
        }
    }
}
