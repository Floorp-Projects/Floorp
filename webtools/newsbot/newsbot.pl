#!/usr/bonsaitools/bin/perl -w
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
# The Original Code is NewsBot
# 
# The Initial Developer of the Original Code is Netscape Communications 
# Corporation. Portions created by Netscape are Copyright (C) 1998 
# Netscape Communications Corporation. All Rights Reserved. 
# 
# Contributor(s): Dawn Endico <endico@mozilla.org> 
 
# Harvest pointers to news articles and their summaries from mailbox file
# and write html and rdf files from it.
#
# usage: newsbot [mailfile1, mailfile2...] [rdffile]
# each mail file is standard mbox format such as a sendmail spool file.
# rdffile is where to put the generated rdf.
# output is written to standard output.

require 5.00397;
use strict;
use Mail::Folder::Mbox;
use Mail::Address;

my $rdffile = pop (@ARGV); #name of file to write rdf data

unless (@ARGV) {
  # command line argument should be list of mail files to 
  # process. If none given, use this file.
  my $mailfile = "/var/mail/newsbot";
  die("No mail\n") if (!-f $mailfile);
  push(@ARGV, $mailfile);
}

for my $file (@ARGV) {
  my $folder = new Mail::Folder('AUTODETECT', $file);
  unless ($folder) {
    warn("can't open $folder: $!");
    next;
  }

printheader();

my %articlehash; 
my @articles;
my $index=0;

for my $msg (sort { $a <=> $b } $folder->message_list) {
    my $entity = $folder->get_mime_message($msg);
    my $submitter = $entity->get('From'); chomp($submitter);
    $submitter =~ s/</&lt;/g;
    $submitter =~ s/>/&gt;/g;
    $submitter =~ s/&/&amp;/g;
    my $submitdate = $entity->get('Date'); chomp($submitdate);

    #
    # This is important for weeding out junk. Submissions must be multipart
    # mime messages as created by Messenger when forwarding a news article.
    # the first part is text/html or text/plain. The second part should be
    # type message/rfc822. This format preserves the header information of
    # the original news article (especially the Message-ID). It also makes
    # it more difficult for random junk mailed to newsbot to litter the
    # web page. We don't want someone to post an article, cc newsbot and
    # have a big thread begin where the rest of the messages continue
    # being cc'd to newsbot.
    #
    if ($entity->parts < 2) {
        next;
        }
    my @parts = $entity->parts;
    if ( !($parts[0]->head->mime_type =~ /text\/html/) &&
         !($parts[0]->head->mime_type =~ /text\/plain/) ) {
        next;
        }
    if ( !($parts[1]->head->mime_type =~ /message\/rfc822/)) {
        next;
        }

    my $IO;
    my $summary = "";
    if ($IO = $entity->parts(0)->open("r")) {
        $summary = $summary . $_ while (defined($_ = $IO->getline));
        $IO->close;
        if ( $entity->parts(0)->head->mime_type =~ /text\/plain/ ) {
             # line beginning with -- is a signature seperator. Delete the sig
             $summary =~ s/^--.*//ms;
             $summary =~ s/</&lt;/mg;
             $summary =~ s/>/&gt;/mg;
             $summary =~ s/&/&amp;/g;
             $summary =~ s/(http:\/\/([\S])+)/<A HREF=\"$1\">$1<\/A>/mg;
             $summary =~ s/(ftp:\/\/([\S])+)/<A HREF=\"$1\">$1<\/A>/mg;
             $summary =~ s/&lt;(([\S])+@([\S])+)&gt;/&lt;<A HREF=\"mailto:$1\">$1<\/A>&gt;/mg;
             }
        }
    my $news = "";
    if ($IO = $entity->parts(1)->open("r")) {
        $news = $news . $_ while (defined($_ = $IO->getline));
        $IO->close;
        }
    # check to make sure this is a news article. If not, skip it.
    $news  =~ /^Newsgroups: ([^\n]+)/m ;
    my $newsgroups = $1;
    if (!$newsgroups) {
        next;
    }
    $newsgroups =~ s/(netscape.public.mozilla.([\w-])+)/\n<A HREF="news:\/\/news.mozilla.org\/$1\">\n $1<\/A>/g;
    $news  =~ /^Message-ID: <([^>]+)/m;
    my $MID  = $1;
    $news  =~ /^From: ([^\n]+)/m;
    my $from = $1;
    $news  =~ /^Subject: ([^\n]+)/m;
    my $subject = $1;
    $subject =~ s/</&lt;/g;
    $subject =~ s/>/&gt;/g;
    $subject =~ s/&/&amp;/g;
    $news  =~ /^Date: ([^\n]+)/m;
    my $date = $1;

    my %article;
    if (! %articlehash->{"$MID"}) {
       %articlehash->{"$MID"}=\%article;
       $index += 1;
       $articles[$index]=\%article;
    }

    %article->{'Message-ID'} = $MID;
    %article->{'Subject'} = $subject;
    %article->{'Date'} = $date;
    %article->{'From'} = $from;
    %article->{'Newsgroups'} = $newsgroups;
    %article->{'Summary'} = $summary;
    %article->{'submitter'} = $submitter;
    %article->{'submitdate'} = $submitdate;

    $entity->purge; 
  } #for loop
$folder->close;

for (my $i=$index; $i > 0 ; $i--) {
    printarticle ($articles[$i]);
    }
printfooter();

if ($rdffile) {
    printrdf (\@articles, $rdffile);
    }
}



sub printrdf() {
my ($ref, $rdffile) = @_;
my @articles = @{$ref};

unless (open (RDFFILE,">$rdffile") ){
   die "Couldn\'t open rdf file:\"$rdffile\"\n";
   }
select RDFFILE;

my $header =<<'RDFHEAD';
<?xml version="1.0"?>
<rdf:RDF
xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
xmlns="http://my.netscape.com/rdf/simple/0.9/">

  <channel>
    <title>Mozilla NewsBot</title>
    <link>http://www.mozilla.org/newsbot/</link>
    <description>Pointers to the hottest mozilla newsgroup threads.</description>
  </channel>

  <image>
    <title>Mozilla</title>
    <url>http://www.mozilla.org/images/hack.gif</url>
    <link>http://www.mozilla.org/newsbot/</link>
  </image>

RDFHEAD
print $header;

my $index =  @articles - 1;
# only print newest 15 articles
my $min = 0;
if ($index > 15) {
    $min = $index - 15;
}
for (my $i=$index; $i > $min ; $i--) {
    print ("  <item>\n");
    print ("    <title>" . $articles[$i]->{'Subject'} . "</title>\n");
    print ("    <link>http://www.dejanews.com/[LB=http://www.mozilla.org/]/msgid.xp?MID=&lt;" . $articles[$i]->{'Message-ID'} . "&gt;</link>\n");
    print ("  </item>\n\n");
    }

print "</rdf:RDF>\n";
} #end printrdf()



      
sub printarticle() {

my ($artref) = @_;
my %article = %{$artref};
      
print "<DIV NAME=\"" . %article->{'Message-ID'} . "\" CLASS=\"newsarticle\">\n";
print "<TABLE border=0 width=100%><TR><TD><B><FONT SIZE=+1>\n";
print "<SPAN CLASS=\"subject\">\n";
print %article->{'Subject'} ."\n";
print "</SPAN>\n";
print "</B></FONT>\n";
print "</TD></TR><TR><TD>\n";
print "<SPAN CLASS=\"summary\">\n";
print %article->{'Summary'};
print "</SPAN>\n";
print "</TD></TR><TR ALIGN=RIGHT><TD>\n";
print "<FONT SIZE=-1>\nPosted: " . %article->{'Date'} ."\n</FONT>";
print "<BR>";
print %article->{'Newsgroups'} . "\n";
print "<BR>";
print "<SPAN CLASS=\"articlelink\n\">";
print "<A HREF=\"http://www.dejanews.com/[LB=http://www.mozilla.org/]/msgid.xp?MID=<" . %article->{'Message-ID'} . ">\">\n";
print "</SPAN>\n";
print "View Article</A> -\n";
print "<A HREF=\"http://www.dejanews.com/[LB=http://www.mozilla.org/]/thread/%3c" . %article->{'Message-ID'} ."%3e%231/1\">\n";
print "View Thread\n";
print "</A>\n";
print "<!--Submitted to NewsBot by: " . %article->{'submitter'} . "-->\n";
print "<!--" . %article->{'submitdate'} . "-->\n";
print "</TD></TR>\n";
print "</TABLE>\n";
print "</DIV>\n\n\n";
}



sub printheader() {
my $header =<<'ENDHEAD';
<HTML>
<HEAD>
<TITLE>newsbot</TITLE>
</HEAD>

<BODY>
<H1>newsbot</H1>
Since not everyone has a chance to keep up with all the mozilla
<A HREF="http://www.mozilla.org/community.html">news groups</A>,
newsbot is here to collect pointers to some of the more important 
announcements, discussions, and goings-<WBR>on.
<P>
When you see an article of interest to the general mozilla community 
forward it to <A HREF="mailto:newsbot@mozilla.org">newsbot@mozilla.org</A> 
and write a summary of the article.  Newsbot will add your summary to this
page and make pointers back to the original article and its thread in DejaNews.

<BLOCKQUOTE><FONT SIZE=-1>
For Netscape Communicator users, this means pressing the <I>Forward</I>
button and writing a summary in the message window.  (Forwarding as  "quoted" 
or "inline" confuses newsbot. Be sure to forward as attachment.)  For users 
of other clients, the forwarded message should be a multipart MIME message 
where the first part is text/plain or text/html and contains your summary, 
and the second part is type message/rfc822 and contains the news article.
</FONT></BLOCKQUOTE>

<P>
ENDHEAD
print $header
}



sub printfooter() {
my $footer =<<'ENDFOOT';
<P>
<FONT SIZE=-1>
Send newsbot feedback
to <A HREF="mailto:endico@mozilla.org">Dawn Endico</a>.
</FONT>
</BODY>
</HTML>
ENDFOOT
print $footer;
}
