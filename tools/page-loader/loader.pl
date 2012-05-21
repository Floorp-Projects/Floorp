#!/usr/bin/perl
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

use strict;
use CGI::Request;
use CGI::Carp qw(fatalsToBrowser);
use Time::HiRes qw(gettimeofday tv_interval);
use POSIX qw(strftime);
use DBI;

# list of test pages, JS to insert, httpbase, filebase, etc.
use PageData;

use vars qw(%params $req $cgi $dbh $pagedata
            $gStartNow $gStartNowStr
            $gResponseNow $gLogging);

$gStartNow    = [gettimeofday];  # checkpoint the time
$gStartNowStr = strftime "%Y%m%d%H%M%S", localtime;
$gLogging     = 1;

$req = new CGI::Request; # get the HTTP/CGI request
$cgi = $req->cgi;

$pagedata = PageData->new;

setDefaultParams();

#XXXdebugcrap
#warn $params{index}, " ", $params{maxidx};

if (!defined($req->param('delay'))) {
    # give the user a form to pick options (but note that going
    # to "loader.pl?delay=1000" immediately starts the test run
    outputForm();
}
elsif (!$req->param('id')) {
    initialize();          # do redirect to start the cycle
}
elsif ($params{index} > $params{maxidx}) {
    redirectToReport();   # the test is over; spit out a summary
    markTestAsComplete(); # close the meta table entry
}
elsif (!isRequestStale()) {
    outputPage();          # otherwise, keep dishing out pages
    updateDataBase();      # client has the response; now write out stats to db
}

# cleanup
$req = undef;
$dbh->disconnect() if $dbh; # not strictly required (ignored in some cases anyways)

#logMessage(sprintf("Page load server responded in %3d msec, total time %3d msec, pid: %d",
#                   1000*tv_interval($gStartNow, $gResponseNow), 1000*tv_interval($gStartNow), $$))
#     if $gResponseNow; # log only when a test page has been dished out

exit 0;

#######################################################################

sub logMessage {
    print STDERR strftime("[%a %b %d %H:%M:%S %Y] ", localtime), @_, "\n"
         if $gLogging;
}


sub isRequestStale {
    my $limit = 30*60; # 30 minutes, although if we never stalled on mac I'd make it 3 minutes
    my $ts    = decodeHiResTime($params{s_ts});
    my $delta = tv_interval($ts, $gStartNow);
    return undef if $delta < $limit;
    # otherwise, punt this request
    print "Content-type: text/html\n\n";
    print <<"ENDOFHTML";
<html><head><title>Page Loading Times Test</title></head><body>
<p><b>The timestamp on the request is too old to continue:<br>
s_ts=$params{s_ts} was $delta seconds ago. Limit is $limit seconds.</b></p>
</body></html>
ENDOFHTML
     return 1; # it's stale
}


sub initialize {
    updateMetaTable();
    createDataSetTable();

    # start the test by bouncing off of an echo page
    my $script = $cgi->var("SCRIPT_NAME");
    my $server = $cgi->var("SERVER_NAME");
    my $proto  = $ENV{SERVER_PORT} == 443 ? 'https://' : 'http://';
    my $me     = $proto . $server . $script;
    $script    =~ /^(.*\/).*$/;
    my $loc    = "Location: ". $proto . $server . $1 . "echo.pl?";
    for (qw(id index maxcyc delay replace nocache timeout)) {
        $loc .= "$_=$params{$_}\&";
    }
    $loc .= "url=" . $me;
    print $loc, "\n\n";
}


sub redirectToReport {
    # n.b., can also add '&sort=1' to get a time sorted list
    my $proto  = $ENV{SERVER_PORT} == 443 ? 'https://' : 'http://';
    my $loc = "Location: " . $proto . $cgi->var("SERVER_NAME");
    $cgi->var("SCRIPT_NAME") =~ /^(.*\/).*$/;
    $loc  .= $1 . "report.pl?id=" . $params{id};
    # To use for a tinderbox, comment out the line above and uncomment this:
    # $loc  .= $1 . "dump.pl?id=" . $params{id} . "&purge=1";
    print $loc, "\n\n";
}


sub generateTestId {
    # use the epoch time, in hex, plus a two-character random.
    return sprintf "%8X%02X", time(), int(256*rand());
}


sub setDefaultParams {
    $params{id}      = $req->param('id') || generateTestId(); # "unique" id for this run
    $params{index}   = $req->param('index')   || 0; # request index for the test
    $params{maxcyc}  = defined($req->param('maxcyc')) ? 
        $req->param('maxcyc') : 3;                    # max visits (zero-based count)
    $params{delay}   = $req->param('delay')   || 1000; # setTimeout on the next request (msec)
    $params{replace} = $req->param('replace') || 0; # use Location.replace (1) or Location.href (0)
    $params{nocache} = $req->param('nocache') || 0; # serve content via uncacheable path
    $params{c_part}  = $req->param('c_part')  || 0; # client time elapsed; page head to onload (msec)
    $params{c_intvl} = $req->param('c_intvl') || 0; # client time elapsed; onload to onload event (msec)
    $params{c_ts}    = $req->param('c_ts')    || 0; # client timestamp (.getTime()) (msec)
    $params{content} = $req->param('content') || "UNKNOWN"; # name of content page for this data
    $params{s_ts}    = $req->param('s_ts')    || undef; # server timestamp; no default
    $params{timeout} = $req->param('timeout') || 30000; # msec; timer will cancel stalled page loading 
    $params{maxidx}  = ($params{maxcyc}+1) * $pagedata->length; # total pages loads to be done
    $params{curidx}  = $params{index} % $pagedata->length;  # current request index into page list
    $params{curcyc}  = int(($params{index}-1) / $pagedata->length); # current "cycle" (visit)
}


sub outputPage {
    my $relpath = $pagedata->url($params{curidx});
    my $file = $pagedata->filebase . $relpath;
    open (HTML, "<$file") ||
        die "Can't open file: $file,  $!";

    my $hook = "<script xmlns='http://www.w3.org/1999/xhtml'>\n";
    $hook .= "var g_moztest_Start = (new Date()).getTime();\n";
    $hook .= "var g_moztest_ServerTime='" . encodeHiResTime($gStartNow) . "';\n";
    $hook .= "var g_moztest_Content='" . $pagedata->name($params{curidx}) . "';\n";
    $hook .= $pagedata->clientJS;                   # ... and the main body
    $hook .= "var g_moztest_safetyTimer = ";
    $hook .= "window.setTimeout(moztest_safetyValve, " . $params{timeout} . ");";
    $hook .= "</script>\n";

    my $basepath = $pagedata->httpbase;
    $basepath =~ s/^http:/https:/i
	 if $ENV{SERVER_PORT} == 443;
    #warn "basepath: $basepath";
    $basepath =~ s#^(.*?)(/base/)$#$1/nocache$2# if ($params{nocache});
    $hook .= "<base href='". $basepath . $relpath .
      "' xmlns='http://www.w3.org/1999/xhtml' />";

    my $magic   = $pagedata->magicString;
    my $content = "";
    while (<HTML>) {
        s/$magic/$hook/;
        $content .= $_;
    }

    my $contentTypeHeader;
    my $mimetype = $pagedata->mimetype($params{curidx});
    my $charset = $pagedata->charset($params{curidx});
    if ($charset) {
	$contentTypeHeader = qq{Content-type: $mimetype; charset="$charset"\n\n};
    } else {
	$contentTypeHeader = qq{Content-type: $mimetype\n\n};
    }
    #warn $contentTypeHeader; #XXXjrgm testing...
	
    # N.B., these two cookie headers are obsolete, since I pass server info in
    # JS now, to work around a bug in winEmbed with document.cookie. But
    # since I _was_ sending two cookies as part of the test, I have to keep
    # sending two cookies (at least for now, and it's not a bad thing to test)
    #XXX other headers to test/use?

    $gResponseNow = [gettimeofday];  # for logging
    { # turn on output autoflush, locally in this block
        print "Set-Cookie: moztest_SomeRandomCookie1=somerandomstring\n";
        print "Set-Cookie: moztest_SomeRandomCookie2=somerandomstring\n";
        print $contentTypeHeader;
        local $| = 1; 
        print $content;
    }

    return;
}


sub encodeHiResTime {
    my $timeref = shift;
    return unless ref($timeref);
    return $$timeref[0] . "-" . $$timeref[1];
}


sub decodeHiResTime {
    my $timestr = shift;
    return [ split('-', $timestr) ];
}


sub elapsedMilliSeconds {
    my ($r_time, $timestr) = @_;
    return "NaN" unless $timestr;
    my $delta = tv_interval( [ split('-', $timestr) ], $r_time );
    my $delta = int(($delta*1000) - $params{delay});  # adjust for delay (in msec)
    return $delta;
}


sub updateDataBase {
    connectToDataBase(); # (may already be cached)
    updateMetaTable();
    updateDataSetTable() unless $params{c_part} == -1; # the initial request
}


sub connectToDataBase {
    # don't reconnect if already connected. (Other drivers provide this
    # for free I think, but not this one).
    if (!ref($dbh)) {
        $dbh = DBI->connect("DBI:CSV:f_dir=./db", {RaiseError => 1, AutoCommit => 1})
             || die "Cannot connect: " . $DBI::errstr;
    }
}


#
# Holds the individual page load data for this id.
#
# (Of course, this should really be a single table for all datasets, but
# that was becoming punitively slow with DBD::CSV. I could have moved to
# a "real" database, but I didn't want to make that a requirement for
# installing this on another server and using this test (e.g., install a
# few modules and you can run this; no sql installation/maintenance required).
# At some point though, I may switch to some sql db, but hopefully still allow
# this to be used with a simple flat file db. (Hmm, maybe I should try a *dbm
# as a compromise (disk based but indexed)).
#
sub createDataSetTable {
  my $table = "t" . $params{id};
  return if -f "db/$table"; # don't create it if it exists
  logMessage("createDataSetTable:\tdb/$table");
  connectToDataBase();      # cached

  my ($sth, $sql);
  $sql = qq{
      CREATE TABLE $table
          (DATETIME CHAR(14),
           ID CHAR(10),
           INDEX INTEGER,
           CUR_IDX INTEGER,
           CUR_CYC INTEGER,
           C_PART INTEGER,
           S_INTVL INTEGER,
           C_INTVL INTEGER,
           CONTENT CHAR(128)
           )
          };
  $sth = $dbh->prepare($sql);
  $sth->execute();
  $sth->finish();
  return 1;
}


#
# holds the information about all test runs
#
sub createMetaTable {
  my $table = shift;
  return if -f "db/$table"; # don't create it if it exists
  logMessage("createMetaTable:\tdb/$table");

  my ($sth, $sql);

  $sql = qq{
      CREATE TABLE $table
          (DATETIME CHAR(14),
           LASTPING CHAR(14),
           ID CHAR(8),
           INDEX INTEGER,
           CUR_IDX INTEGER,
           CUR_CYC INTEGER,
           CUR_CONTENT CHAR(128),
           STATE INTEGER,
           BLESSED INTEGER,
           MAXCYC INTEGER,
           MAXIDX INTEGER,
           REPLACE INTEGER,
           NOCACHE INTEGER,
           DELAY INTEGER,
           REMOTE_USER CHAR(16),
           HTTP_USER_AGENT CHAR(128),
           REMOTE_ADDR CHAR(15),
           USER_EMAIL CHAR(32),
           USER_COMMENT CHAR(256)
           )
          };
  $sth = $dbh->prepare($sql);
  $sth->execute();
  $sth->finish();
  warn 'created meta table';
  return 1;
}


sub updateMetaTable {

    connectToDataBase(); # if not already connected

    my $table = "tMetaTable";
    createMetaTable($table); # just returns if already created

    my ($sth, $sql);

    $sql = qq{
        SELECT INDEX, MAXCYC, MAXIDX, REPLACE, NOCACHE,
               DELAY, REMOTE_USER, HTTP_USER_AGENT, REMOTE_ADDR
          FROM $table
          WHERE ID = '$params{id}'
           };
    $sth = $dbh->prepare($sql);
    $sth->execute();

    my @dataset = ();
    while (my @data = $sth->fetchrow_array()) {
        push @dataset, {index           => shift @data,
                        maxcyc          => shift @data,
                        maxidx          => shift @data,
                        replace         => shift @data,
                        nocache         => shift @data,
                        delay           => shift @data,
                        remote_user     => shift @data,
                        http_user_agent => shift @data,
                        remote_addr     => shift @data
                        };
    }
    $sth->finish();
    warn "More than one ID: $params{id} ??" if scalar(@dataset) > 1;

    if (scalar(@dataset) == 0) {
        # this is a new dataset and id
        initMetaTableRecord($table);
        return;
    }

    #XXX need to check that values are sane, and not update if they don't
    # match certain params. This should not happen in a normal test run.
    # However, if a test url was bookmarked or in history, I might get bogus
    # data collected after the fact. But I have a stale date set on the URL,
    # so that is good enough for now.
    # my $ref = shift @dataset; # check some $ref->{foo}

    $sql = qq{
        UPDATE $table
          SET LASTPING = ?,
              INDEX    = ?,
              CUR_IDX  = ?,
              CUR_CYC  = ?,
              CUR_CONTENT = ?,
              STATE    = ?
          WHERE ID = '$params{id}'
        };
    $sth = $dbh->prepare($sql);
    $sth->execute($gStartNowStr,
                  $params{index}-1,  # (index-1) is complete; (index) in progress
                  ($params{curidx}-1) % $pagedata->length,  
                  $params{curcyc},
                  $params{content},
                  'OPEN'
                  );
    $sth->finish();

}


sub markTestAsComplete {
    connectToDataBase(); # if not already connected
    my $table = "tMetaTable";
    createMetaTable($table); # just returns if already created
    my ($sth, $sql);
    #XXX should probably check if this ID exists first
    $sql = qq{
        UPDATE $table
          SET STATE = "COMPLETE"
          WHERE ID = '$params{id}'
        };
    $sth = $dbh->prepare($sql);
    $sth->execute();
    $sth->finish();
}


sub initMetaTableRecord {
    # we know this record doesn't exist, so put in the initial values
    my $table = shift;
    my ($sth, $sql);
    $sql = qq{
        INSERT INTO $table
             (DATETIME,
              LASTPING,
              ID,
              INDEX,
              CUR_IDX,
              CUR_CYC,
              CUR_CONTENT,
              STATE,
              BLESSED,
              MAXCYC,
              MAXIDX,
              REPLACE,
              NOCACHE,
              DELAY,
              REMOTE_USER,
              HTTP_USER_AGENT,
              REMOTE_ADDR,
              USER_EMAIL,
              USER_COMMENT
              )
          VALUES (?,?,?,?,
                  ?,?,?,?,
                  ?,?,?,?,
                  ?,?,?,?,
                  ?,?,?)
        };
    $sth = $dbh->prepare($sql);
    $sth->execute($gStartNowStr,
                  $gStartNowStr,
                  $params{id},
                  $params{index}-1,
                  ($params{curidx}-1) % $pagedata->length,
                  $params{curcyc},
                  $params{content},
                  "INIT",
                  0,
                  $params{maxcyc},
                  $params{maxidx},
                  $params{replace},
                  $params{nocache},
                  $params{delay},
                  $cgi->var("REMOTE_USER"),
                  $cgi->var("HTTP_USER_AGENT"),
                  $cgi->var("REMOTE_ADDR"),
                  "",
                  ""
                  );
    $sth->finish();
}


sub updateDataSetTable {
    my $table = shift;
    my $table = "t" . $params{id};

    my ($sth, $sql);
    $sql = qq{
        INSERT INTO $table
            (DATETIME,
             ID,
             INDEX,
             CUR_IDX,
             CUR_CYC,
             C_PART,
             S_INTVL,
             C_INTVL,
             CONTENT        
             )
          VALUES (?,?,?,?,
                  ?,?,?,?,?)
        };

    my $s_intvl = elapsedMilliSeconds( $gStartNow, $params{s_ts} );

    $sth = $dbh->prepare($sql);
    $sth->execute($gStartNowStr,            
                  $params{id},
                  $params{index}-1,
                  ($params{curidx}-1) % $pagedata->length,
                  $params{curcyc},
                  $params{c_part},
                  $s_intvl,
                  $params{c_intvl},
                  $req->param('content'),
                  );
    $sth->finish();

}


sub outputForm {
    my @prog = split('/', $0); my $prog = $prog[$#prog];
    print "Content-type: text/html\n\n";
    my $bgcolor = $ENV{SERVER_PORT} == 443 ? '#eebb66' : '#ffffff';
    print <<"ENDOFHTML";
<html>
<head>
  <title>Page Loading Times Test</title>
</head>
<body bgcolor="$bgcolor">
  <h3>Page Loading Times Test</h3>

<p>Questions: <a href="mailto:jrgm\@netscape.com">John Morrison</a>

ENDOFHTML
    print "&nbsp;&nbsp;-&nbsp;&nbsp;";
    my $script = $cgi->var("SCRIPT_NAME");
    my $server = $cgi->var("SERVER_NAME");
    # pick the "other" protocol (i.e., test is inverted)
    my $proto  = $ENV{SERVER_PORT} == 443 ? 'http://' : 'https://';
    my $other  = $proto . $server . $script;
    if ($ENV{SERVER_PORT} == 443) {
	print "[&nbsp;<a href='$other'>With no SSL</a>&nbsp;|&nbsp;<b>With SSL</b>&nbsp;]<br>";
    } else {
	print "[&nbsp;<b>With no SSL</b>&nbsp;|&nbsp;<a href='$other'>With SSL</a>&nbsp;]<br>";
    }
    print <<"ENDOFHTML";

  <form method="get" action="$prog" >
    <table border="1" cellpadding="5" cellspacing="2">
      <tr>
        <td valign="top">
          Page-load to Page-load Delay (msec):<br>
          (Use 1000. Be nice.)
        </td>
        <td valign="top">
          <select name="delay">
          <option value="0">0
          <option value="500">500
          <option selected value="1000">1000
          <option value="2000">2000
          <option value="3000">3000
          <option value="4000">4000
          <option value="5000">5000
          </select>
        </td>
      </tr>
      <tr>
        <td valign="top">
          Number of test cycles to run:<br>
              <br>
        </td>
        <td valign="top">
          <select name="maxcyc">
          <option value="0">1
          <option value="1">2
          <option value="2">3
          <option value="3">4
          <option value="4" selected>5
          <option value="5">6
          <option value="6">7
          </select>
        </td>
      </tr>
      <tr>
        <td valign="top">
          How long to wait before cancelling (msec):<br>
          (Don't change this unless on a very slow link, or very slow machine.)
        </td>
        <td valign="top">
          <select name="timeout">
          <option value="15000">15000
          <option selected value="30000">30000
          <option value="45000">45000
          <option value="60000">60000
          <option value="90000">90000
          </select>
        </td>
      </tr>
      <tr>
        <td valign="top">
          <input type="reset" value="reset">
        </td>
        <td valign="top">
          <input type="submit" value="submit">
        </td>
      </tr>
    </table>

<hr>
<p>
  You can visit the content that will be loaded, minus the embedded
  javascript, by clicking on any of the links below.
</p>

    <table border="1" cellpadding="5" cellspacing="2">
ENDOFHTML

    my $i;
    print "<tr>\n";
    my $base = $pagedata->httpbase;
    $base =~ s/^http:/https:/i 
        if $ENV{SERVER_PORT} == 443;
    for ($i=0; $i<$pagedata->length; $i++) {
        print "<td nowrap><a href='", $base, $pagedata->url($i), "'>";
        print $pagedata->name($i);
        print "</a>\n";
        print "</tr><tr>\n" if (($i+1)%4 == 0);
    }
    print "</tr>" if (($i+1)%4 != 0);
    print "</table></form></body></html>\n";
    return;
}

