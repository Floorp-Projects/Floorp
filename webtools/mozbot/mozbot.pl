#!/usr/bonsaitools/bin/perl5 -w
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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>
#                 Risto Kotalampi <risto@kotalampi.com>
#                 Josh Soref <timeless@mac.com>
#
# mozbot.pl harrison@netscape.com 10/14/98
# "irc bot for the gang on #mozilla"
#
# features: reports tinderbox status upon request.
# remembers urls. tells you the phase of the moon.
# grabs mozillaZine headlines. fetches slashdot.org
# news. scans ftp server for new files.
# bot will auto-op based on nick and remote host.
# the bot should now attempt to reconnect to the server if it is disconnected
# if throttling issues arise, a delay could be implemented.
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

#todo
# add newlink urls to news
# whine to console when trying to op w/o ops.
# external configuration.

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
eval 'use Chatbot::Eliza;';
my $translate_babelfish;
eval 'use WWW::Babelfish;';
$translate_babelfish = new WWW::Babelfish unless $@;

use IPC::Open2;
use FileHandle;
use Net::FTP;
use HTML::Entities;

$|++;

my $VERSION = '1.66'; # keep me in sync with the mozilla.org cvs repository
my $debug = 1; # debug output also includes warnings, errors
my $mozbotfilename = $0;
@::origargv = @ARGV;

my $server  = shift || 'irc.mozilla.org';
my $port    = shift || '6667';
my $nick    = shift || 'mozbot';
my $channel = shift || '#mozilla';

my @otherchannels = $nick =~ /exp|new|test/osi ? () : (
  '#mozillazine',
  '#mozwebtools',
  '#qa',
  '#smoketest',
  '#mozilla',
  '#mozbot',
);

my %msgcmds = $nick =~ /exp|new|test/osi ? () : (
  'url' => \&bot_urls,
);

my %pubcmds = (
  "(help|about)" => \&bot_about,
  "(hi|hello|lo|sup)" => \&bot_hi,
  'uuid' => \&bot_uuid,
  'up' => \&bot_up,
  "(bug|bugid|bugzilla)" => \&bot_getbug,
  "(bug-total|bug-summary|bugzilla-total|bugzilla-summary)" => \&bot_getbug,
  "(trees|tree)" => \&bot_tinderbox,
  "(quicktree|qt|treesum|tinderbox)" => \&bot_tindersummary,
  'debug' => \&bot_debug,
  'review' => \&bot_review,
  'approve' => \&bot_approve,
  'sheriff' => \&the_sheriff,
  'comp' => \&bgz_comp,
  'keyword' => \&bgz_kywd,
  'qa' => \&bgz_qa,
  'own' => \&bgz_own,
  'ftp' => \&ftp_stamp,
  'wishlist' => \&wish_list,
  'wish' => \&bot_wish
);

$pubcmds{"(translate|xlate|x)"} = \&bot_translate if $translate_babelfish;

my %admincmds = (
  'bless' => \&bot_bless,
  'unbless' => \&bot_unbless,
  'shutdown' => \&bot_shutdown,
  'say' => \&bot_say,
  'list' => \&bot_list,
  'ftpx' => \&ftp_check,
  'ftpc' => \&ftp_clear,
  'cycle' => \&bot_cycle,
  '(go|join)' => \&bot_forcejoin,
  '(leave|part)' => \&bot_part,
  'restart' => \&bot_restart,
  'tickle' => \&bot_tickle,
  'mode' => \&bot_mode,
  'raw' => \&bot_raw,
  'ucomp' => \&get_products,
  'ukywd' => \&get_keywords,
);

my %rdfcmds;
if ($nick =~ /exp|new|tech/osi) {
} else {
  %rdfcmds = $nick =~ /mozbot|zinebot/osi ? (
    "(mozillaorg|mozilla|mo)" => 'http://www.mozilla.org/news.rdf',
    "(newsbot|nb)" => 'http://www.mozilla.org/newsbot/newsbot.rdf',
    "(xptoolkit|xpfe)" => 'http://www.mozilla.org/xpfe/toolkit.rdf',
    "(mozillazine|zine|mz)" => 'http://www.mozillazine.org/contents.rdf',
  )
    :
  (
    "(slashdot|sd|\/\.)" => 'http://www.slashdot.org/slashdot.rdf',
    "(mozillaorg|mozilla|mo)" => 'http://www.mozilla.org/news.rdf',
    "(newsbot|nb)" => 'http://www.mozilla.org/newsbot/newsbot.rdf',
    "(xptoolkit|xpfe)" => 'http://www.mozilla.org/xpfe/toolkit.rdf',
    "(freshmeat|fm)" => 'http://freshmeat.net/backend/fm.rdf',
    "(mozillazine|zine|mz)" => 'http://www.mozillazine.org/contents.rdf',
  );
}

my %rdf_title;
my %rdf_link;
my %rdf_last;
my %rdf_items;
my %latestbuilds;

my $stockf = "stocklist";
my %stocklist = ();
my %stockvals = ();
my %stockhist;

&debug ("mozbot $VERSION starting up");

LoadStockList() unless $nick =~ /exp|new|tech/osi;
$pubcmds{"(stocks|stock)"} = \&bot_pub_stocks if %stocklist; 
$msgcmds{"(stocks|stock)"} = \&bot_stocks if %stocklist; 
&create_pid_file;

# read admin list 
my %admins = ( 'rko' => 'netscape.com', 'cyeh' => 'netscape.com', 'terry' => 'netscape.com' );
my $adminf = ".$nick-admins";
&fetch_admin_conf (\%admins);

my $uptime = 0;

$::moon = './moon';
$::moon = (-f $::moon) ? $::moon : ''; 
$pubcmds{'moon'} = \&bot_moon  if ($::moon);

$::uuid = './uuidgen/uuidgen';
$::uuid = (-f $::uuid) ? $::uuid : '';
delete $pubcmds{'uuid'} unless $::uuid;

my $phase;
my $last_moon = 0;
my $last_uuid = 0;

# leave @trees empty if you don't want tinderbox details

my @all_trees= (qw(SeaMonkey), qw(SeaMonkey-Ports), qw(SeaMonkey-Embed), qw(Mozilla-0.8-Branch), qw(SeaMonkey-Branch), qw(MozillaTest),
qw (Grendel), qw(Bugzilla), qw(NSS), qw(Testing), );
my @trees= (qw (SeaMonkey), qw(SeaMonkey-Ports));
my %keyw=(
ns6=>qw(branch), pr3=>qw(branch), 
windows=>qw(win),win9x=>qw(win),win32=>qw(win),
netscape=>'moz',mozilla=>'moz',seamonkey=>'moz',standard=>'moz',std=>'moz',
fmacos=>'mac',macintosh=>'mac',
sunos=>'sun',solaris=>'sun',
'HP-UX'=>'unix',AIX=>'unix',
freebsd=>'bsd',openbsd=>'bsd',netbsd=>'bsd',
beos=>'be',bezilla=>'be',
tinderbox=>'wt',webtools=>'wt',bugzilla=>'wt',infobot=>'wt',word=>'wt',techbot=>'wt',nakedmolerat=>'wt',testmanager=>'wt',ostreatus=>'wt',oedipus=>'wt',melaleuca=>'wt',
);
my %treew=(
branch=>qw(SeaMonkey-Branch),
ports=>qw(SeaMonkey-Ports),
be=>qw(MozillaTest),
all=>'SeaMonkey SeaMonkey-Ports SeaMonkey-Branch SeaMonkey-Embed Mozilla-0.8-Branch MozillaTest NSS Grendel Bugzilla NSS Testing',
moz=>qw (SeaMonkey),
sun=>'SeaMonkey SeaMonkey-Ports',
unix=>'SeaMonkey SeaMonkey-Ports',
bsd=>'SeaMonkey SeaMonkey-Ports',
linux=>qw (SeaMonkey),
win=>qw (SeaMonkey),
mac=>qw (SeaMonkey),
be=>qw (MozillaTest),
wt=>qw(Bugzilla),
);
@trees= qw (Grendel) if ($nick =~ /grend/);
my $trees;
my $status;
my $last_tree;
my %broken;
my @urls;
my $ftpsite='ftp.mozilla.org';

my $greet = 0;
my @greetings =
  $nick =~ /tech/osi ? ( 'niihau' ) :
  ( 
  "g'day", 'bonjour', 'guten tag', 'konnichiwa',
  'hello', 'hola', 'hi', 'buono giorno', 'aloha',
  'hey', "'sup", 'lo', 'howdy', 'saluton', 'hei',
  'hallo', 'word', 'yo yo yo', 'rheeet', 'bom dia',
  'ciao'
  );

my %keywords;
my $productIndex;
my @productList;
my %components;

my $irc = new Net::IRC or confess "$0: duh?";
my $OWNER = 'endico@mozilla.org';
my $bot = $irc->newconn
  (
  Server => $server,
  Port => $port,
  Nick => $nick,
  Ircname => "mozilla.org bot/thing $VERSION; $OWNER",
  Username => $nick,
  )
or die "$0: can't connect to $server, port $port";

my $userhost;
my $sheriff='';


&debug ("adding global handlers");
$bot->add_global_handler ([ 251,252,253,254,302,255 ], \&on_startup);
$bot->add_global_handler (376, \&on_connect);
$bot->add_global_handler (433, \&on_nick_taken);
$bot->add_global_handler ([ 'disconnect', 'kill', 474, 465 ], \&on_boot);

&debug ("adding more handlers");
$bot->add_handler ('msg', \&on_msg);
$bot->add_handler ('public', \&on_public);
$bot->add_handler ('join',   \&on_join);
$bot->add_handler ('nick',   \&on_nick_change);

&debug ("scheduling stuff");
$bot->schedule (0, \&tinderbox);
$bot->schedule (0, \&checksourcechange);
$bot->schedule (0, \&stocks) if %stocklist;
$bot->schedule (0, \&ftp_scan);
$bot->schedule (0, \&scheduled_sheriff);
$bot->schedule (0, \&get_keywords);
$bot->schedule (0, \&get_products);

foreach my $i (keys %rdfcmds) {
    $bot->schedule(0, \&rdfchannel, $rdfcmds{$i});
    $pubcmds{$i} = $rdfcmds{$i};
}

&debug ("connecting to $server $port as $nick on $channel");

$::megahal = './megahal/megahal';
$::megahal = (-f $::megahal) ? $::megahal : '';
#david: why does the following line need to be commented out?
#$::megahal_pid;

if ($::megahal) {
  $::WTR = FileHandle->new;
  $::RDR = FileHandle->new;
  $::megahal_pid = &init_megahal;
  &debug ("Initializing MEGAHAL conversation AI\n");
}

# and done.


# Use this routine, always, instead of the standard "privmsg" routine.  This
# one makes sure we don't send more than one message every two seconds or so,
# which will make servers not whine about us flooding the channel.
# messages aren't the only type of flood :-( away is included [or so it seems]
#
# Actually, it seems that we can send two in a row just fine, but at that
# point we should start throttling.

my $lastsenttime = 0;
my @msgqueue = ();
sub sendmsg
{
  my ($who, $msg) = @_;
  $msg=decode_entities $msg;
  my $now = time();
  if ($now > $lastsenttime && 0 == @msgqueue) {
    $bot->privmsg($who, $msg);
    $lastsenttime = $now;
  } else {
    push(@msgqueue, [$who, $msg]);
    if (3 > @msgqueue) {
      $bot->schedule(0, \&drainmsgqueue);
    } else {
      $bot->schedule(20, \&drainmsgqueue);
    }
  }
}

sub drainmsgqueue
{
  my $qln=@msgqueue;
  if (0 < @msgqueue) {
    my ($who, $msg) = (@{shift(@msgqueue)});
    $bot->privmsg($who, $msg);
    $lastsenttime = time();
    if (0 < @msgqueue) {
      if (0==@msgqueue % 5){
        bot_longprocess("Long send queue. There were $qln, and I just sent one to $who.");
        $bot->schedule(4, \&drainmsgqueue);
      } else {
        $bot->schedule(2, \&drainmsgqueue);
      }
    } else {
      bot_back();
    }
  }
}
#bots need a better understanding of server rules.
#one problem is that for a 7/8lines in one case not 1 line is held in queue until flushing
# [especially w/ saylonglines?]




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
  &debug ('startup took ' . (time - $^T) . ' seconds');
  $self->join ($channel);
  $self->join (join(',',@otherchannels));
  $uptime = time;
}

# on_nick_taken: or do something smarter

sub on_nick_taken
{
  die 'hey! somebody took my nick!';
}

sub on_nick_change
{
}

sub not_used {
  my ($self, $event) = @_;
  my ($channel) = ($event->to)[0];
  my $src = $event->nick;
  my $userhost = $event->userhost;
  &debug("$event\n%event");
  $nick=$event->data if ($src eq $nick);
}

sub do_command
{
  my ($hashref, $to, $cmd, $rest) = @_;
  foreach my $m (keys %$hashref) {
    if ($cmd =~ m/^$m$/) {
      my $ref = $hashref->{$m};
      if (ref($ref)) {
        &{$ref} ($to, $cmd, $rest);
      } else {
        bot_rdfchannel($to, $cmd, $rest, $ref);
      }
      return 1;
    }
  }
  return 0;
}

# on_msg: private message received via /msg

sub on_msg
{
  my ($self, $event) = @_;
  my ($to) = $event->nick;
  my ($arg) = $event->args;
  my @arglist = split(' ', $arg);
  my $cmd = shift @arglist;
  my $rest = join(' ', @arglist);
  $userhost = $event->userhost;
  $::speaker = $to;         # Hack!!!
  return
    if (exists $admins{$to} 
    && $userhost =~ /$admins{$to}$/i 
    && do_command(\%admincmds, $to, $cmd, $rest));
  return
    if (do_command(\%msgcmds, $to, $cmd, $rest));
  return
    if (do_command(\%pubcmds, $to, $cmd, $rest));
  do_unknown($to, $cmd, $rest);
}

sub bot_bless
{
  my ($src, $cmd, $rest) = @_;
  my ($who, $where) = split(' ', $rest);
  if (! $who or ! $where) {
    sendmsg($src, 'usage: bless [ user host ] ' . 
      '(example: bless marca netscape.com)');
    return;
  }
  $admins{$who} = $where;
  &debug ("$src blessed $who ($where)");
  &store_admin_conf (\%admins);
  sendmsg($src, "$nick admins: " . join ' ', (sort keys %admins));
}

my %hequivs=(
  '1'=>'*',
  '2'=>'*',
  '3'=>'*',
  '4'=>'+',
  '5'=>'_',
  '6'=>'_'
);
my @heqvs=keys %hequivs;
my %sequivs=(
  'b'=>'*',
  'i'=>'/',
  'u'=>'_',
);
my @seqvs=keys %sequivs;

sub get_sheriff
{
  my $stree=shift||'SeaMonkey';
  my $url = "http://tinderbox.mozilla.org/$stree/sheriff.pl";
  my $output = get $url;
  return "I couldn't read $url" unless $output; 
  $output=~/= \'(.*)\';\n1;$/os; $output=$1;
  $output=~s/\n|\r|\\|<a href="|<\/a>//ogi;
  $output=~s/">/, /og;
  $output=~s/(?:<br>)+/; /ogi; 
  $output=~s#</?([@seqvs])>#$sequivs{$1}#ogi;
  $output=~s/\s?<h([@heqvs])([^>]*)>\s+/ $hequivs{$1}$2/ogi;
  $output=~s/\s+<\/h([@heqvs])>\s?/$hequivs{$1} /ogi;
  if ($output ne $sheriff){
    $sheriff=$output;
    sendmsg($channel, "$stree Sheriff change: $sheriff");
  }
}

sub scheduled_sheriff
{
  get_sheriff '';
  $bot->schedule (3600, \&scheduled_sheriff);
}

sub the_sheriff
{
  my ($to, $cmd, $rest) = @_;
  get_sheriff '';
  sendmsg($to, "I think: $sheriff");
}

sub get_keywords
{
  %keywords=();
  my $output = 
    get 'http://bugzilla.mozilla.org/describekeywords.cgi';
  my @out = split(/<.?TR>/,$output);
  foreach my $line ( @out ) {
    if ($line =~ m#<TH>([^<]*)</TH>[^<]*<TD>([^<]*)</TD>[^<]*<TD[^<]*<A[^>]*>([^<]*)<#sm) {
      $keywords{lc $1}=[$1, $2, $3];
    }
  }       
}

sub get_components
{
  my $prod=$productList[$productIndex++];
  my $output = 
    get 'http://bugzilla.mozilla.org/describecomponents.cgi?product='.$prod;
  my @out = split ( /<TABLE/, $output);
  $output = $out[4];
  @out = split(/<.?tr>/,$output);
  foreach my $line ( @out ) {
    if ($line =~ m#<td[^>]*>([^<]*)</td>[^a]*[^"]*"([^"]*)[^>]*>([^<]*).*<a href="([^"]*)[^>]*>([^<]*)#sm) {
      $components{lc $1}=[$prod, $1, $2, $3, $4, $5];
    }
  }
  $bot->schedule (20, \&get_components)
    if ($productIndex<=$#productList);
}

sub get_products
{
  %components=();
  $productIndex=0;
  my $output = 
    get 'http://bugzilla.mozilla.org/describecomponents.cgi?';
  return if (! $output); 
  my @out=split( /<.?SELECT[^>]*>/, $output );
  $output=$out[1];
  @productList=split( /<[^>]*>/, $output );
  $bot->schedule (20, \&get_components);
}

sub bgz_kywd
{
  my ($to, $cmd, $rest) = @_;
  my @out;
  if (!$rest) {
    saylongline($to, 'Keywords include: '.join(', ',keys %keywords));
  }
  elsif ($keywords{lc $rest}) {
    @out=@{$keywords{lc $rest}};
    sendmsg($to, 'Keyword '.$out[0].
' ('.($out[2]==0?'no bugs':($out[2]==1?'one bug':$out[2].' bugs')).') '.$out[1]);
  } else {
local ($,)=', ';
#@out=(join(' ',keys %keywords) =~ /(\S*$rest\S*)/gi);
#    saylongline($to, $rest.' is like '.join(', ',@out));
my ($key,$value,$comment);
while(($key,$value) = each %keywords){ if($key =~ /$rest/i) {$comment.=($comment?$,:'').$key;}}
    saylongline($to, $rest.' is like '.$comment);
  }
}

sub bgz_comp
{
  my ($to, $cmd, $rest) = @_;
  my @out;
  if (!$rest) {
    saylongline($to, 'Components include: '.join(', ',keys %components));
  }
  elsif ($components{lc $rest}) {
    @out=@{$components{lc $rest}};
    sendmsg($to, 'Component '.$out[1].' of Product '.$out[0].' is owned by '.
    $out[3].($out[2] eq $out[3]?'':' < '.$out[2].' >').' and the qa is '.$out[5].($out[4] eq $out[5]?'':' < '.$out[4].' > '));
  } else {
local ($,)=', ';
#@out=(join(' ',keys %components) =~ /(\S*$rest\S*)/gi);
my ($key,$value,$comment);
while(($key,$value) = each %components){ if($key =~ /$rest/i) {$comment.=($comment?$,:'').$key;}}
    saylongline($to, $rest.' is like '.$comment);
  }
}

sub bgz_qa
{
  my ($to, $cmd, $rest) = @_;
  if ($components{lc $rest}) {
    my @out=@{$components{lc $rest}};
    sendmsg($to, 'QA for Component '.$out[1].' of Product '.$out[0].' is '.
    $out[5].($out[4] eq $out[5]?'':' < '.$out[4].' > '));
  } else {
    sendmsg($to, 'I could not find '.$rest);
  }
}

sub bgz_own
{
  my ($to, $cmd, $rest) = @_;
  if ($components{lc $rest}) {
    my @out=@{$components{lc $rest}};
    sendmsg($to, 'Owner of Component '.$out[1].' of Product '.$out[0].' is '.
    $out[3].($out[2] eq $out[3]?'':' < '.$out[2].' >'));
  } else {
    sendmsg($to, 'I could not find '.$rest);
  }
}

sub ftp_clear
{
  %latestbuilds=();
}

sub day_str
{
  my (@stamp,$ahr,$amn,$asc);
  ($asc, $amn, $ahr, @stamp)=gmtime($_[3]);
  return "$_[4] ($ahr:$amn:$asc) "
    if ($stamp[0]==$_[0] && $stamp[1]==$_[1] && $stamp[2]==$_[2]);
}

sub ftp_stamp
{
  my $to=$_[0];
  my @day=gmtime(time); my @tm=@day[0..2]; @day=@day[3..5];
  my (@filestamp, $filelist, $ahr,$amn,$asc);
  if ($_[2]){
    foreach my $filename (keys %latestbuilds){
      my @ltm=gmtime($latestbuilds{$filename});
      $filelist.="$filename [".($ltm[5]+1900).'-'.($ltm[4]+1)."-$ltm[3] $ltm[2]:$ltm[1]:$ltm[0]]"
        if $filename=~/$_[2]/;
    }
    $filelist=$filelist||'<nothing matched>';
    $filelist="Files matching re:$_[2] [gmt] $filelist";
  } else {
    foreach my $filename (keys %latestbuilds){
      $filelist.=day_str(@day[0..2],$latestbuilds{$filename},$filename);
    }
    if ($filelist){
      $filelist="Files from today [gmt] $filelist";
    } else {
      foreach my $filename (keys %latestbuilds){
        @day=gmtime(time-86400); @day=@day[3..5];
        $filelist.=day_str(@day[0..2],$latestbuilds{$filename},$filename);
      }
      $filelist="Files from yesterday [gmt] $filelist"||
        '<No files in the past two days by gmt, try "ftp ." for a complete filelist';
    }
  }
  sendmsg($to, $filelist);
}

sub ftp_check
{
  my $to=$_[0]||$channel;
  my $buf='';
  my $mdtms;
  my $ftpserver=$_[2]||$ftpsite;
  my $ftp = new Net::FTP($ftpserver, Debug => 0, Passive => 1);
  if ($ftp){
    bot_longprocess("fetching FTP $ftpserver nightly/latest");
    &debug("fetching FTP $ftpserver nightly/latest");
    if ($ftp->login('anonymous','mozbot@localhost')){
      $ftp->cwd('/pub/mozilla/nightly/latest');
      for my $f ($ftp->ls){
        $mdtms=$ftp->mdtm($f);
        if (!$latestbuilds{$f} || $mdtms>$latestbuilds{$f}) {
          $buf.=$f.'; '; 
          $latestbuilds{$f}=$mdtms;
        } 
      }
      $ftp->quit;
      bot_back();
    };
    $buf="New files @ ftp://$ftpserver/pub/mozilla/nightly/latest ".$buf if ($buf);
  } else {
    $buf="I couldn't connect to $ftpserver";
  }
  sendmsg($to, $buf);
}
#DBM or DB_File %hash that's tied to a file
#DBM and DB_File are documented on the
# http://www.perl.com/CPAN-local/doc/manual/html/lib/ page.
# (note there are several DBM links there so be sure to check each
# (= exhaustive look not brief look))
# timeless believes ftp info doesn't need to persist across multiple sessions
# of mozbot 
# Lots of help from

sub ftp_scan
{
  ftp_check $channel;
  $bot->schedule(360,\&ftp_scan);
}

sub wish_list
{
  my ($to)=@_;
  sendmsg($to, 'timeless has a long list, if you want to add to it, make a wish.');
  foreach (split (/\n/,`tail -n 5 RFEs.techbot`)){
    sendmsg($to, $_);
  }
}

sub bot_wish
{
  my ($to, $cmd, $rest) = @_;
  if (open WISHLIST, ">>RFEs.$nick"){
    my $t=localtime(time);
    print WISHLIST "$t $to $::speaker $rest\n";
    close WISHLIST;
  if (exists $admins{$::speaker}) {
      sendmsg($to, 'granted');
    } else {
      sendmsg($to, "wouldn't that be cool? We'll see...");
    }
  }
}

sub bot_unbless
{
  my ($src, $cmd, $rest) = @_;
  my ($who) = ($rest);
  if (exists ($admins{$who})) {
    delete $admins{$who};
    &debug ("$src unblessed $who");
    &store_admin_conf (\%admins);
    sendmsg($src, "$nick admins: " . join ' ', (sort keys %admins));
    return;
  }
  sendmsg($src, 'Can only unbless one of: ' .
  join(' ', (sort keys %admins)));
}

sub bot_shutdown
{
  my ($src, $cmd, $rest) = @_;
  if ($rest ne 'yes') {
    sendmsg($src, 'usage: shutdown yes');
    return;
  }
  &debug ("forced shutdown by $src");
  $::dontQuitOnSignal++;
  $bot->quit ("$src told me to shutdown");
  exit (0);
}

sub bot_tickle
{
  bot_restart($_[0], $_[1], 'yes')
}

sub bot_restart
{
  my ($src, $cmd, $rest) = @_;
  if ($rest ne 'yes') {
    sendmsg($src, "usage: $cmd yes");
    return;
  }
  $::dontQuitOnSignal = 1;
  $bot->quit('hrm, gotta go,... BeRight back');
  &debug ('restarting self');
  exec "$0 @::origargv";
}

sub bot_cycle
{
  my ($src, $cmd, @chans)=@_;
  sendmsg($src, "usage: $cmd {#<channel> [comment]}+")
    unless @chans;
  foreach my $chan (@chans){
    $bot->sl('PART ', $chan);
    $bot->join($chan);
  }
}

sub bot_part
{
  my (@chans,$chan);
  my ($src, $cmd, $chans)=(@_);
  sendmsg($src, "usage: $cmd".' {<channel> [comment]}+')
    unless $chans;
  my $text='';
  @chans=split /\s+/,$chans;
  foreach $chan (reverse @chans){
    if ($chan=~/^[#&+].*$/){
      $bot->sl('PART ', $chan.' :'.$text);
      $text='';
    } else {
      $text=$chan.' '.$text if $chan; 
    }
  }
}

sub bot_forcejoin
{
  my ($src, $cmd, @chans)=@_;
  sendmsg($src, "usage: $cmd".' #<channel>+')
    unless @chans;
  foreach my $chan (@chans){
    $bot->join($chan);
  }
}

sub bot_mode
{
  my ($src, $cmd, @cmd)= @_;
  $bot->sl('MODE ', @cmd);
}

sub bot_raw
{
  my ($src, undef, $cmd, $params)= @_;
  my @params=split / /, $cmd;
  $cmd=shift @params;
  $params=join ' ',@params;
  &debug("$src\t$cmd\t$params\t\n");
  $bot->sl($cmd.' ', $params);
}

sub bot_say
{
  my ($src, $cmd, $rest) = @_;
  my ($tgt, @text) = split /\s+/,$rest;
  join(' ',@text)=~/(\/me\s+|)(.*)/;
  my ($act,$msg)=($1,$2);
  if ($src ne $nick && !$tgt=~/[#&+]/){
    $msg=$tgt.' '.$msg;
    $tgt=$channel;
  }
  if ($act) {
    $bot->me($tgt, $msg);
  } else {
    sendmsg($tgt, $msg);
  }
}


sub bot_list
{
  my ($src, $cmd, $rest) = @_;
  my @list;
  foreach (sort keys %admins) {
    push(@list, "$_ $admins{$_}");
  }
  my $spacer = ' ... ';
  saylongline($src, join($spacer, @list), $spacer);
}

sub on_version
{
  my (undef, $event) = @_;
  $bot->ctcp_reply($event->nick, 'mozbot version '.$VERSION.
    '; http://lxr.mozilla.org/mozilla/source/webtools/mozbot/mozbot.pl');
}

sub on_public
{
  my ($self, $event) = @_;
  my ($to) = $event->to;
  my ($arg) = $event->args;
  my ($src, $me) = ($event->nick, $self->nick);
  $::speaker = $src;         # Hack!!!
    
  if ($msgcmds{'url'}) {
# catch urls, stick them in a list for mozbot's url command
    if ($arg =~ /(http|ftp|gopher):/i && $src ne $me) {
      push @urls, "$arg (" . &logdate() . ')';
      while ($#urls > 10) {
        shift @urls;
      }
    }
  }
  if (my ($cmd, $rest) = $arg =~ /^$me[:,]?\s+(\S+)(?:\s+(.*))?$/i) {
    return if (do_command(\%pubcmds, $to, $cmd, $rest));
    do_unknown($to, $cmd, $rest);
  }
}

sub do_unknown
{
  return if $nick =~ /exp|new|tech/osi;
  my ($to, $cmd, $rest) = @_;
  if (defined $::megahal_pid) {
    my $sentence = $cmd . ' ' . $rest . "\n";
    &debug($sentence);
    print $::WTR $sentence . "\n";
    my $result = ($::RDR)->getline;
    &debug($result . "\n");
    sendmsg($to, $result);
  }
  else {
    $::eliza = new Chatbot::Eliza unless defined $::eliza;
    my $result = $::eliza->transform("$cmd $rest");
    sendmsg($to, $result);
  }
}

sub bot_longprocess
{
  $bot->away(join(' ',@_));
}

sub bot_back
{
  $bot->away('');
}

sub saylongline
{
  my ($to, $str, $spacer) = @_;
  my $MAXPROTOCOLLENGTH = 450;
  while (length ($str) > $MAXPROTOCOLLENGTH) {
    my $pos;
    $pos = rindex($str, $spacer, $MAXPROTOCOLLENGTH - length($spacer));
    if ($pos < 0) {
      $pos = rindex($str, ' ', $MAXPROTOCOLLENGTH - 1);
      $pos = $MAXPROTOCOLLENGTH - 1 if $pos < 0;
    }
    sendmsg($to, substr($str, 0, $pos));
    $str = substr($str, $pos);
    $str = substr($str, length($spacer)) if index($str, $spacer) == 0;
  }
  sendmsg($to, $str) if $str;
}

sub do_headlines
{
  my ($to, $header, $ref) = @_;
  my $spacer = ' ... ';
  my $str = $header . ': ' . join($spacer, @$ref);
  saylongline($to, $str, $spacer);
}

sub reportDiffs
{
  my ($name, $url, $ref) = @_;
  my $firsttime = 0;
  if (!exists $::headCache{$url}) {
    $firsttime = 1;
    $::headCache{$url} = {};
  }
  my $spacer = ' ... ';
  my $outstr = '';
  foreach my $i (@$ref) {
    next if ($i =~ /^last update/);
    if (!exists $::headCache{$url}->{$i}) {
      $::headCache{$url}->{$i} = 1;
      $outstr =(($outstr)?
        "Just appeared in $name - $url : ":
        $spacer).$i;
    }
  }
  saylongline($channel, $outstr, $spacer) unless $firsttime;
}

sub bot_debug
{
  my ($src, $cmd, $rest) = @_;

  my @list;
  my %last = (
    'tinderbox' => $last_tree,
    'moon' => $last_moon,
    'uuid' => $last_uuid,
  );

  foreach (keys %last){
    push @list, ($last{$_} != 0)?
      ("$_ updated: " . &logdate ($last{$_}) . ", " . &days ($last{$_})):
      "$_ never updated!";
  }

  foreach (sort(keys %rdf_last)) {
    push @list, "$_ updated: " . logdate($rdf_last{$_}) . ", " .
      days($rdf_last{$_});
  }

  do_headlines ($src, 'Boring Debug Information', \@list);
}

sub bot_rdfchannel
{
  my ($to, $cmd, $rest, $url) = @_;
  if (defined $rdf_title{$url}) {
    do_headlines($to, "Items in $rdf_title{$url} - $rdf_link{$url} ",
      $rdf_items{$url});
  } else {
    sendmsg($to, "Nothing has been found yet at $url");
  }
}

sub bot_hi
{
  my ($to, $cmd, $rest) = @_;
  sendmsg($to, $greetings[$greet++] . " $::speaker");
  $greet = 0 if ($greet > $#greetings);
}

sub on_join
{
  my ($self, $event) = @_;
  my ($channel) = ($event->to)[0];
  my $src = $event->nick;
  $userhost = $event->userhost;

  # auto-op if user is a mozbot admin and coming in from
  # the right host 

  if (exists $admins{$src} && $userhost =~ /$admins{$src}$/i) {
    $self->mode ($channel, '+o', $src);
    &debug ("auto-op for $src on $channel");
  }
}

$::dontQuitOnSignal = 0;
sub on_boot
{
  if (!$::dontQuitOnSignal) {
    &debug ("$0: disconnected from network");
    exec "$0 @::origargv";
    die '';
  }
}

sub listcmds
{
  my ($hashref) = @_;
  my @list;
  foreach my $k (keys %$hashref) {
    push @list, ($k =~ m/^\(([-a-z]+)\|/)?$1:$k;
  }
  return join(' ', sort(@list));
}

################
# bot commands #
################

# bot_about: it's either an about box or the 
# address of the guy to blame when the bot 
# breaks

sub bot_about
{
  my ($src, $cmd, $rest) = @_;
  sendmsg($::speaker,  "i am mozbot version $VERSION. hack on me! " .
    'timeless@mac.com 3/29/01. ' .
    "connected to $server since " .
    &logdate ($uptime) . ' (' . &days ($uptime) . '). ' .
    'see http://bonsai.mozilla.org/cvsquery.cgi?branch=HEAD&file=mozilla/webtools/mozbot/&date=week ' .
    'for a changelog.');
  sendmsg($::speaker, 'Known commands are: ' .
    listcmds(\%pubcmds)) if keys %pubcmds;
  sendmsg($::speaker, "If you /msg me, I'll also respond to: " .
    listcmds(\%msgcmds)) if keys %msgcmds;
  sendmsg($::speaker, 'You are an admin, so you can do: ' . listcmds(\%admincmds))
    if (keys %admincmds &&
    exists $admins{$::speaker} &&
    $userhost =~ /$admins{$::speaker}$/i);
  sendmsg($src,"[ Directions on talking to me have been sent to $::speaker ]")
    if $src ne $::speaker;
}

# bot_moon: goodnight moon

sub get_moon_str
{
  return '- no moon -' unless defined $::moon; 
  return $phase if ($phase && (time - $last_moon > (60 * 60 * 24)));
  # we only want to run this once/day
  $phase = `$::moon`;
  $last_moon = time;
  return $phase;
}

sub bot_moon
{
  my ($to, $cmd, $rest) = @_;
  sendmsg($to, get_moon_str());
}

sub get_uuid_str
{
  my $this_uuid;
  return '- no uuid -' unless defined $::uuid;
  $this_uuid = `$::uuid`;
  return $this_uuid;
}

sub bot_uuid
{
  my ($to, $cmd, $rest) = @_;
  sendmsg($to, get_uuid_str());
}

# bot_up: report uptime

sub bot_up
{
  my ($to, $cmd, $rest) = @_;
  sendmsg($to,  &logdate ($uptime) . ' (' . &days ($uptime) . ')');
}

# bot_urls: show last ten urls caught by mozbot

sub bot_urls
{
  my ($to, $cmd, $rest) = @_;
  if ($#urls == -1) {
    bot_say($nick, $to, '/me have seen no URLs yet...');
  } else {
    foreach my $m (@urls) {
      sendmsg($to, $m);
    }
  }
}

sub bot_getbug
{
  my ($to,$cmd,$rest) = (@_);
  if ($rest){
    my $url = 'http://bugzilla.mozilla.org/buglist.cgi?bug_id='.join(',',split(' ',$rest));
    bot_longprocess("Retrieving a buglist for $to from $url");
    my $output = get $url;
    return unless $output;
    bot_longprocess("Processing buglist for $to, $rest");
    $output =~ s#</TABLE..TABLE[^>]*>\s+(<TR)#$1#gme;
    (undef, $output) = split /Summary<\/A><\/TH>/,$output;
    ($output, undef) = split /<\/TABLE>/,$output;
#this loses data when it's split into multiple tables
    $output=~s/\n//g;
    my @qp= split /<TR VALIGN=TOP ALIGN=LEFT CLASS=P3 ><TD>/,$output;
    my $buf="$#qp bugs found. ";
    if ($cmd=~/total|summary/){
      sendmsg($to, $buf);
    } else{
      # loop through quickparse output    
      if (@qp>6 && $to=~/^#\w+/) {
        $buf.='5 shown, please message me for the complete list';
        @qp=@qp[1..5];
      }
      sendmsg($to, 'Zarro boogs found.') if (1>@qp);
      sendmsg($to, $buf) if (@qp>2);
      my $osep=$"; $"=' ';
      foreach (@qp){
        if ($_){
          if (my @d=/<A.*?>(.*?)<\/A>.*?<nobr>(.*?)<\/nobr>.*?<nobr>(.*?)<\/nobr>.*?<nobr>(.*?)<\/nobr>.*?<nobr>(.*?)<\/nobr>.*?<nobr>(.*?)<\/nobr>.*?<nobr>(.*?)<\/nobr>.*>(.*)/){
            sendmsg($to, "Bug http://bugzilla.mozilla.org/show_bug.cgi?id=@d");
          }
          #bugid ? px Platform mail status subject
        }
      }
      $"=$osep;
    }
    bot_back();
  } else {
    if ($to=~/^#\w+/) {
      sendmsg($to, "Syntax bugzilla [bugnumber[,]]*[&bugzillaparameter=value]* - details sent to $::speaker");
      sendmsg($::speaker, "bug_status: UNCONFIRMED|NEW|ASSIGNED|REOPENED; *type*=substring|; bugtype: include|exclude; order: Assignee|; chfield[from|to|value] short_desc' long_desc' status_whiteboard' bug_file_loc' keywords'; '_type; email[|type][1|2] [reporter|qa_contact|assigned_to|cc]");
    } else {
      sendmsg($to, "Syntax bugzilla [bugnumber[,]]*[&bugzillaparameter=value]* - details sent to $::speaker");
    }
  }
}

# show tinderbox status
#
# this is a messy little function but it works. 
sub tinder_alias
{
  my $rest;
  my $tcode;
  foreach $tcode (@_){
    $tcode=$keyw{$tcode} if ($keyw{$tcode});
    $tcode=$treew{$tcode} if ($treew{$tcode});
    $rest.=' '.$tcode;
  }
  my %saw;
  return join(' ',grep(!$saw{$_}++, split(/\s+/,$rest)));
}

sub bot_tindersummary
{
  my ($to, $cmd, $rest) = @_;
  my ($pref, $buf, @buf, @tree);
  my %ts_symbols=('S'=>'+','T'=>'?','H'=>'-');
  my ($rg, $ry, $rr, $trg, $try, $trr)=(0,0,0,0,0,0);
  $rest=tinder_alias(split(/\s+/,$rest));
  push @tree, $rest ? (split /\s+/, $rest) : @trees;
  my $osep=$";$"=', ';
  foreach my $t (@tree){
    if (length($t)){
      my @short=($$status{$t}) ? split(//,$$status{$t}):('');
      $pref = "$t $short[0]: ";
      # politely report failures; for summary should I do this in some other fassion?
      if (! exists $$trees{$t}) {
        $buf .= "unknown tree '$t', trees include @all_trees. ";
      } else {
        $rg=0; $ry=0; $rr=0;
        foreach my $e (sort keys %{$$trees{$t}}) {
          my $rval=$$trees{$t}{$e};
          $rg++ if ($rval=~/Success/);
          $ry++ if ($rval=~/Test Failed/);
          $rr++ if ($rval=~/Horked/);
          my @atree=split(/ /,$e);
          my @astatus=split(//,$rval);
          $buf .= "$atree[0]$ts_symbols{$astatus[0]} ";
        }
      }
      $pref .= " $rg$ts_symbols{'S'} $ry$ts_symbols{'T'} $rr$ts_symbols{'H'}: ";
      $trg+=$rg; $try+=$ry; $trr+=$rr;
      sendmsg ($to, $pref.$buf);
      $buf='';
    }
  }
  $"=$osep;
}

sub bot_tinderbox
{
  my ($to, $cmd, $rest) = @_;
  my ($pref, $buf, @buf, @tree);
  my ($rg, $ry, $rr, $trg, $try, $trr)=(0,0,0,0,0,0);
  $rest=tinder_alias(split(/\s+/,$rest));
  push @tree, $rest ? (split /\s+/, $rest) : @trees;
  my $osep=$";$"=', ';
  my $bustage;
  my $terse = (defined $rest && $rest eq 'all')||$to ne $::speaker;
  # user can supply a list of trees separated 
  # by whitespace
  # loop through requested trees
  push @buf, 'Tinderbox status from http://tinderbox.mozilla.org/showbuilds.cgi';
  foreach my $t (@tree){
    if (length($t)){
      $bustage = 0;
      $buf = "$t " . ($$status{$t} ? "<$$status{$t}> " : '') . ': ';
      # politely report failures
      if (! exists $$trees{$t}){
        $buf .= "unknown tree '$t', trees include @all_trees. ";
      } else {
        $rg=0; $ry=0; $rr=0;
        foreach my $e (sort keys %{$$trees{$t}}){
          my $rval=$$trees{$t}{$e};
          $rg++ if ($rval=~/Success/); 
          $ry++ if ($rval=~/Test Failed/);
          $rr++ if ($rval=~/Horked/);
          next if ($terse && $rval eq 'Success');
          $buf .= "[$e: $$trees{$t}{$e}] ";
          $bustage++;
        }
      }
      $buf .= ($bustage)?
        "$rg success, $ry test failures, and $rr horked.":
        '- no known bustage -';
      $trg+=$rg; $try+=$ry; $trr+=$rr;
      push @buf, $buf;
    }
  }
  $buf = $buf || 
    'something broke. report a bug here: ' .
    'http://bugzilla.mozilla.org/enter_bug.cgi ' .
    'with product of Webtools and component set to Mozbot';
  my $summary=($#tree>1)?" - $trg:$try:$trr":'';
  push @buf, 'last update: ' .
          &logdate ($last_tree) . ' (' . &days ($last_tree) . ' ago)'.$summary;
  $"=$osep;
  foreach my $m (@buf) {
          sendmsg ($to, $m);
  }
}

#############
# utilities #
#############

sub debug
{
  return unless $debug;
  foreach (@_) {
    #chomp;
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
    $mon + 1, $mday, $year + 1900, $hour, $min);
}

# days: how long ago was that? 

sub days
{
  my ($then) = shift;
  my $seconds = time - $then;
  my $minutes = int ($seconds / 60);
  my $hours = int ($minutes / 60);
  my $days = int ($hours / 24);
  return (sprintf "%d second%s", $seconds, $seconds == 1 ? '' : 's')
    if ($seconds < 60);
  return (sprintf "%d minute%s", $minutes, $minutes == 1 ? '' : 's')
    if ($minutes < 60);
  return (sprintf "%d hour%s", $hours, $hours == 1 ? '' : 's')
    if ($hours < 24);
  return (sprintf "%d day%s", $days, $days == 1 ? '' : 's');
}

# signal handler

sub killed
{
  if ($::megahal_pid) {
    &debug("Killing megahal.\n");
    kill (2, $::megahal_pid);
  }
  confess "i have received a signal of some manner. good night.\n\n";
}

# write admin list 

sub store_admin_conf
{
  my $admins = shift;
  my $when = localtime (time) . " by $$";

  if (open ADMINS, ">$adminf"){
    print ADMINS <<FIN;
# mozbot admin list file
#
# this file is generated. do not edit.
# generated $when 
#
# version: 1.0

FIN

    foreach (sort keys %admins){
      print ADMINS "$_ $admins{$_}\n";
    }
    close ADMINS;
  } else {
    &debug ("&store_admin_conf $adminf: $!");
  }
}

# fetch list of admins

sub fetch_admin_conf
{
  my $admins = shift;

  if (open ADMINS, $adminf){
    while (<ADMINS>){
      chomp;
      next if ($_ =~ /^#/ or ! $_);
      my ($user, $host) = split /\s+/, $_;
      $$admins{$user} = $host;
    }
    &debug ('admins: ' . keys %$admins);
  } else {
    &debug ("&fetch_admin_conf $adminf: $!");
  }

  close ADMINS;
}

# create a pid file if we can

sub create_pid_file
{
  my $pid = ".$nick-pid";

  if (open PID, ">$pid"){
    print PID "$$\n";
    close PID;
  } else {
    &debug ("warning: problem creating pid file: $pid, $!");
  }
}

sub init_megahal
{
  my $pid;
  chdir('./megahal');
  $pid = open2($::RDR, $::WTR, './megahal');
  chdir('..');
  return($pid);
}

sub rdfchannel
{
  my (undef, $url) = @_;
  bot_longprocess("fetching rdfchannel $url");
  debug("fetching rdfchannel $url");
  $bot->schedule(60*60, \&rdfchannel, $url);
  my $output = get $url;
  $rdf_last{$url} = time();
  return unless $output;
  my $channelpart = '';
  if ($output =~ m@<channel>(.*)</channel>@si){
    $channelpart = $1;
  }
  $output =~ s@<image>.*</image>@@si;
  $rdf_title{$url} = $url;
  if ($channelpart =~ m@<title>(.+?)</title>@si){
    $rdf_title{$url} = trim($1);
  }
  $rdf_link{$url} = $url;
  if ($channelpart =~ m@<link>(.+?)</link>@si) {
    $rdf_link{$url} = trim($1);
  }
  my @list;
  while ($output =~ m@<item>(.*?)<title>(.+?)</title>(.*?)</item>@sig) {
    push(@list, $2);
  }
  $rdf_items{$url} = \@list;
  reportDiffs($rdf_title{$url}, $rdf_link{$url}, \@list);
  bot_back();
}

# fetch tinderbox details

sub tinderbox
{
  bot_longprocess('fetching tinderbox status');
  &debug ('fetching tinderbox status');
  my ($newtrees, $newstatus) = Tinderbox::status (\@all_trees);
  if (! $newtrees){
    $bot->schedule (90, \&tinderbox);
    &debug ('I could not get tinderbox status');
    return;
  }
  $last_tree = time;
  if (defined $status){
    foreach my $s (keys %$newstatus){
      sendmsg($channel,"$s changed state from $$status{$s} to $$newstatus{$s}")
        if (defined $$newstatus{$s} && $$status{$s} ne $$newstatus{$s});
    }
  }
  if (defined $trees) {
    foreach my $t (@trees) {
      foreach my $e (sort keys %{$$newtrees{$t}}) {
        if (!defined $$trees{$t}{$e}) {
          sendmsg($channel, "$t: A new column '$e' has appeared ($$newtrees{$t}{$e})");
        } else {
          sendmsg($channel, "$t: '$e' has changed state from $$trees{$t}{$e} to $$newtrees{$t}{$e}")
            if ($$trees{$t}{$e} ne $$newtrees{$t}{$e});
        }
      }
    }
  }
  $trees = $newtrees;
  $status = $newstatus;
  $bot->schedule (360, \&tinderbox);
  bot_back();
}

# See if someone has changed our source.

sub checksourcechange
{
  my ($self) = @_;
  my $lastourdate = $::ourdate;
  my $lasttinderboxdate = $::tinderboxdate;
  my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
    $atime,$mtime,$ctime,$blksize,$blocks)
    = stat($mozbotfilename);
  $::ourdate = $mtime;
  ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
    $atime,$mtime,$ctime,$blksize,$blocks)
    = stat('./Tinderbox.pm');
  $::tinderboxdate = $mtime;
  if (defined $lastourdate && 
    ($::ourdate > $lastourdate ||
    $::tinderboxdate > $lasttinderboxdate)) {
    $::dontQuitOnSignal = 1;
    $self->quit('someone seems to have changed my source code.  Be right back');
    &debug ('restarting self');
    exec "$0 @::origargv";
  }
  $bot->schedule (60, \&checksourcechange);
}


sub stocks
{
  $bot->schedule(15 * 60, \&stocks);
  my $ratio;
  my $url = 'http://quote.yahoo.com/d/quotes.csv?f=sl1d1t1c1ohgv&e=.csv&s=' .
    join('+', sort(keys %stocklist));
  &debug ("fetching stock quotes  $url");
  my $output = get $url;
  return unless $output;
  %stockvals = ();
  foreach my $line (split(/\n/, $output)) {
    my @list = split(/,/, $line);
    my $name = shift(@list);
    $name =~ s/"(.*)"/$1/;
    &debug ("parsing stock quote $name ($list[0]) $line");
    $stockvals{$name} = \@list;
    foreach my $ref (@{$stockhist{$name}}) {
      my $oldval = $ref->[0];
      my $newval = $list[0];
      if ($oldval){
        $ratio = $newval / $oldval;
        if ($ratio > 1.05 || $ratio < 0.95) {
          foreach my $who ($stocklist{$name}) {
            ReportStock($who, $name, 'Large Stock Change');
          }
          $stockhist{$name} = [];
          last;
        }
      }
    }
    $stockhist{$name} = []
      unless exists $stockhist{$name};
    push (@{$stockhist{$name}}, \@list);
    while (30 < @{$stockhist{$name}}) {
      shift @{$stockhist{$name}};
    }
  }
}

sub LoadStockList
{
  %stocklist = ('AOL'  => [$channel],
    'RHAT' => [$channel],
    '^DJI' => [$channel],
    '^IXIC' => [$channel]);
  if (open(LIST, $stockf)) {
    %stocklist = ();
    while (<LIST>) {
      my @list = split(/\|/, $_);
      my $name = shift(@list);
      $stocklist{$name} = \@list;
    }
  }
}

sub FracStr
{
  my ($num, $needplus) = @_;
  my $sign;
  if ($num < 0) {
    $sign = '-';
    $num = - $num;
  } else {
    $sign = $needplus ? '+' : '';
    $num =~ s/^\+//;
  }
  my $orignum = $num;
  my $bdot = int($num);
  my $adot = $num - $bdot;
  return "$sign$bdot" if ($adot == 0);
  my $base = 64;
  $num = int($adot * $base);
  while ($num % 2 == 0 && $base > 1) {
    $base /= 2;
    $num /= 2;
  }
  if ($adot == $num / $base){
          $bdot=($bdot == 0)?$bdot = '':$bdot .= ' ';
    return "$sign$bdot$num/$base";
  }
  return "$sign$orignum";
}

sub ReportStock
{
  my ($to, $name, $title) = @_;
  $title .= ': ' if $title;
  my $ref = $stockvals{$name};
  my $a = FracStr($ref->[0], 0);
  my $b = FracStr($ref->[3], 1);
  sendmsg($to, "$title$name at $a ($b)");
}

sub bot_stocks
{
  my ($to, $cmd, $rest) = @_;
  foreach my $name (sort(keys %stocklist)) {
    foreach my $who (@{$stocklist{$name}}) {
    ReportStock($to, $name, '')
      if ($who eq $to || $who eq $channel);
    }
  }
}

sub bot_pub_stocks
{
  my ($to, $cmd, $rest) = @_;
  bot_stocks($::speaker, $cmd, $rest);
  sendmsg($to, "[ Stocks sent to $::speaker. In the future, use '/msg mozbot stocks' ]");
}

# Languages to translate between. Must be more than 1!
my %translate_languages = (
  'en' => 'English',
  'fr' => 'French',
  'de' => 'German',
  'it' => 'Italian',
  'es' => 'Spanish',
  'pt' => 'Portuguese',
);

# Define our `internal' variables.
my $translate_languages = join('|', keys %translate_languages); # for `use'
my %translate_languages_all = %translate_languages;
foreach (values %translate_languages) {
  # isn't there a neater way?
  $translate_languages_all{lc $_} = $_;
}
my $translate_languages_all = join('|', keys %translate_languages_all); # for regexp
my $translate_default_language = 'en'; # must be in list above!

sub translate_usage
{
  sendmsg(@_, "Usage: translate (from|to) ($translate_languages) sentence");
}

sub translate_do
{
  my ($lang1, $lang2, $words) = @_;
  return $translate_babelfish->translate(
    'source' => $translate_languages_all{$lang1},
    'destination' => $translate_languages_all{$lang2},
    'text' => $words,
  );
}

sub bot_translate
{
  my ($to, $cmd, $rest) = @_;
  my ($lang1, $lang2, $words, $result) = (
    $translate_default_language,
    $translate_default_language,
  );
  # check syntax
  my ($a,$b,$c,$d,$e,$f);
  if ($rest =~ /^(.+?\s|)\s*?(from|to)\s+?($translate_languages_all)\s+?(from|to)\s+?($translate_languages_all)\s*?(|\s.*?)$/osi) {
    ($a,$b,$c,$d,$e,$f)=($1,$2,$3,$4,$5,$6);
    ($b,$c,$d,$e)=($d,$e,$b,$c) if $a;
    if (lc $b eq 'from'){
      $lang1 = $c;
      $lang2 = $e if (lc $d ne $b);
    } else {
      $lang2 = $c;
      $lang1 = $e if (lc $d ne $b);
    }
    $f=$d.' '.$e.$f if (lc $d eq $b);
    $words=$a.$f;
  } elsif ($rest =~ /^(.*?\s|)\s*?(from|to)\s+?($translate_languages_all)\s*?(\s.*?|)$/osi) {
    ($a,$b,$c,$d)=($1,$2,$3,$4);
    $words=$a.$d;
    $lang1 = $c if $b eq 'from';
    $lang2 = $c if $b eq 'to';
  } else {
    return translate_usage($to);
  }
  return translate_usage($to) unless $words;
  # translate
  if ($lang1 eq $lang2) {
    sendmsg($to, "Did you just ask me to translate from $lang1 to $lang2 because they seem to be the same language...");
  } else {
    $result = translate_do(lc $lang1, lc $lang2, $words);
    if ($result !~ /^ *$/os) {
      sendmsg($to, $result);
    } else {
      my $error = $translate_babelfish->error;
      if ($error =~ /^ *$/os) {
        sendmsg($to, "I'm afraid I cannot translate that from $translate_languages_all{$lang1} to $translate_languages_all{$lang2}.");
      } else {
        sendmsg($to, "Oops. $error");
      }
    }
  }
}

sub bot_review
{
  my ($to, $cmd, $rest) = @_;
  sendmsg($to, "$::speaker, ".(($nick eq 'mozbot')?"I've":'a mozbot has').
    ' reviewed your code, and it looks great.  r=mozbot.');
}

sub bot_approve
{
  my ($to, $cmd, $rest) = @_;
  # be sneaky, and only respond some of the time.
  my $randval = rand();
  if ($randval < .3) {
    if ($randval < .1){
      sendmsg($to, '"as if."');
    } elsif ($randval < .2){
      sendmsg($to, 'rheeeeeeeet!');
    } elsif ($randval < .3) {
      my $approveRand = rand();
      my $approver='mozbot';
      $approver = 'billg@microsoft.com'
        if ($approveRand < .4);
      $approver = 'marca@ncsa.uiuc.edu'
        if ($approveRand < .2);
      sendmsg($to, "$::speaker, your code is an byzantine workaround for a longstanding hack, but we need it to ship.  a=$approver.");
    }
  } else {
    do_unknown($to, $cmd, $rest);
  }
}

sub trim
{
  ($_) = @_;
  s/^\s+//g;
  s/\s+$//g;
  return $_;
}

# Do this at the very end, so we can intersperse "my" initializations outside
# of routines above and be assured that they will run.

$irc->start;
