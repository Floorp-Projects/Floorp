#!/usr/bonsaitools/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.

use lib '../bonsai';
require 'globals.pl';
require 'lloydcgi.pl';
require 'imagelog.pl';
require 'header.pl';
$|=1;

# Hack this until I can figure out how to do get default root. -slamm
$default_root = '/cvsroot';

# Show 12 hours by default
#
$nowdate = time;
if (not defined($maxdate = $form{maxdate})) {
  $maxdate = $nowdate;
}
if ($form{showall}) {
  $mindate = 0;
}
else {
  $hours = 12;
  $hours = $form{hours} if $form{hours};
  $mindate = $maxdate - ($hours*60*60);
}

$colormap = {
  success    => '00ff00',
  busted     => 'red',
  building   => 'yellow',
  testfailed => 'orange'
};

$tree = $form{tree};

if (exists $form{rebuildguilty} or exists $form{showall}) {
  system ("./buildwho.pl -days 7 $tree > /dev/null");
  undef $form{rebuildguilty};
}

&show_tree_selector, exit if $form{tree} eq '';
&do_quickparse,      exit if $form{quickparse};
&do_express,         exit if $form{express};
&do_rdf,             exit if $form{rdf};
&do_flash,           exit if $form{flash};
&do_panel,           exit if $form{panel};

&load_data;
&print_page_head;
&print_table_header;
&print_table_body;
&print_table_footer;
exit;

# end of main
#=====================================================================

sub make_tree_list {
  my @result;
  while(<*>) {
    if( -d $_ && $_ ne 'data' && $_ ne 'CVS' && -f "$_/treedata.pl") {
      push @result, $_;
    }
  }
  return @result;
}

sub show_tree_selector {

  EmitHtmlHeader("tinderbox");

  print "<P><TABLE WIDTH=\"100%\">";
  print "<TR><TD ALIGN=CENTER>Select one of the following trees:</TD></TR>";
  print "<TR><TD ALIGN=CENTER>\n";
  print " <TABLE><TR><TD><UL>\n";
  
  my @list = make_tree_list();

  foreach (@list) {
    print "<LI><a href=showbuilds.cgi?tree=$_>$_</a>\n";
  }
  print "<//UL></TD></TR></TABLE></TD></TR></TABLE>";
  
  print "<P><TABLE WIDTH=\"100%\">";
  print "<TR><TD ALIGN=CENTER><a href=admintree.cgi>";
  print "Administer</a> one of the following trees:</TD></TR>";
  print "<TR><TD ALIGN=CENTER>\n";
  print " <TABLE><TR><TD><UL>\n";
  
  foreach (@list) {
    print "<LI><a href=admintree.cgi?tree=$_>$_</a>\n";
  }
  print "<//UL></TD></TR></TABLE></TD></TR></TABLE>";
}

sub print_page_head {

  print "Content-type: text/html",
        ($nowdate eq $maxdate ? "\nRefresh: 900" : ''),
        "\n\n<HTML>\n";

  # Get the message of the day only on the first pageful
  do "$tree/mod.pl" if $nowdate eq $maxdate;

  $treename = $tree . ($tree2 ne '' ? " and $tree2" : '');

  my ($minute, $hour) = (localtime)[1,2];
  my $now = sprintf("%02d:%02d", $hour, $minute);

  EmitHtmlTitleAndHeader("tinderbox: $treename", "tinderbox",
                         "tree: $treename ($now)");

  &print_javascript;
  print "$message_of_day\n";
  
  # Quote and Lengend
  #
  unless ($form{nocrap}) {
    my ($imageurl,$imagewidth,$imageheight,$quote) = &get_image;
    print qq{
      <table width="100%" cellpadding=0 cellspacing=0>
        <tr>
          <td valign=bottom>
            <p><center><a href=addimage.cgi><img src="$imageurl"
              width=$imagewidth height=$imageheight><br>
              $quote</a><br>
            </center>
            <p>
          <td align=right valign=bottom>
            <table cellspacing=0 cellpadding=1 border=0><tr><td align=center>
              <TT>L</TT></td><td>= Show Build Log
            </td></tr><tr><td align=center>
              <img src="star.gif"></td><td>= Show Log comments
            </td></tr><tr><td colspan=2>
              <table cellspacing=1 cellpadding=1 border=1>
                <tr bgcolor="$colormap->{success}"><td>Successful Build
                <tr bgcolor="$colormap->{building}"><td>Build in Progress
                <tr bgcolor="$colormap->{testfailed}"><td>Successful Build,
                                                          but Tests Failed
                <tr bgcolor="$colormap->{busted}"><td>Build Failed
              </table>
            </td></tr></table>
          </td>
        </tr>
      </table>
    };
  }
  if ($bonsai_tree) {
    print "The tree is currently <font size=+2>";
    print (&tree_open ? 'OPEN' : 'CLOSED');
    print "</font>\n";
  }
}

sub print_table_body {
  for (my $tt=1; $tt <= $time_count; $tt++) {
    last if $build_time_times->[$tt] < $mindate;
    print_table_row($tt);    
  }
}

sub print_table_row {
  my ($tt) = @_;

  # Time column
  # 
  my $query_link = '';
  my $end_query  = '';
  my $pretty_time = &print_time($build_time_times->[$tt]);

  ($hour) = $pretty_time =~ /(\d\d):/;

  if ($tree2 eq '' and ($lasthour != $hour or &has_who_list($tt))) {
    $query_link = &query_ref($td1, $build_time_times->[$tt]);
    $end_query  = '</a>';
  }
  if ($lasthour == $hour) {
    $pretty_time =~ s/^.*&nbsp;//;
  } else {
    $lasthour = $hour;
  }

  my $hour_color = '';
  $hour_color = ' bgcolor=#e7e7e7' if $build_time_times->[$tt] % 7200 <= 3600;
  print "<tr align=center><td align=right $hour_color>",
        "$query_link\n$pretty_time$end_query</td>\n";

  if ($tree2 ne '') {
    print "<td align=center bgcolor=beige>\n";
    $query_link = &query_ref( $td1, $build_time_times->[$tt]);
    print "$query_link</a></td>\n";
  }

  # Guilty
  #
  print '<td align=center>';
  for $who (sort keys %{$who_list->[$tt]} ){
    $qr = &who_menu($td1, $build_time_times->[$tt],
                    $build_time_times->[$tt-1],$who);
    $who =~ s/%.*$//;
    print "  ${qr}$who</a>\n";
  }
  print '</td>';

  if ($tree2 ne '') {
    print "<td align=center bgcolor=beige >\n";
    $qr = &query_ref( $td2, $build_time_times->[$tt]);
    print "${qr}</a></td>\n";
    
    print "<td align=center>\n";
    
    for $who (sort keys %{$who_list2->[$tt]} ){
      $qr = &who_menu($td2, $build_time_times->[$tt],
                      $build_time_times->[$tt-1],$who);
      print "  ${qr}$who</a>\n";
    }
    print '</td>';
  }
    
  # Build Status
  #
  for ($bn=1; $bn <= $name_count; $bn++) {
    if (not defined($br = $build_table->[$tt][$bn])) {
      # No build data for this time
      print "<td></td>\n";
      next;
    }
    next if $br == -1;  # rowspan has covered this row

    $hasnote = $br->{hasnote};
    $noteid = $hasnote ? $br->{noteid} : 0;
    $rowspan = $br->{rowspan};
    $rowspan = $mindate_time_count - $tt + 1
      if $tt + $rowspan - 1 > $mindate_time_count;
    $color = $colormap->{$br->{buildstatus}};
    $status = $br->{buildstatus};
    print "<td align=center rowspan=$rowspan bgcolor=${color}>\n";
      
    $logfile = $br->{logfile};
    $errorparser = $br->{errorparser};
    $buildname = $br->{buildname};
    if( $tree2 ne "" ){
      $buildname =~ s/^[^ ]* //;
    }
    $buildtime = $br->{buildtime};
    $buildtree = $br->{td}->{name};
      
    print "<tt>\n";
        
    # Build Note
    # 
    if ($hasnote) {
      print "<a href='' onClick=\"return js_what_menu(event,$noteid,$bn,",
            "'$logfile','$buildtime');\"><img src=star.gif border=0></a>\n";
    }
        
    # Build Log
    # 
    $buildname = &url_encode($buildname);
    print "<A HREF='showlog.cgi?logfile=$logfile\&tree=$buildtree",
      "&errorparser=$errorparser&buildname=$buildname&buildtime=$buildtime' ",
      "onClick=\"return log_popup(event,$bn,'$logfile','$buildtime');\">";
    print "L</a>";
      
    # What Changed
    #
# This gives bogus information because the CVS mirror is
# out of sync with the build times
#                 if( $br->{previousbuildtime} ){
# 		  my ($previous_br) = $build_table->[$tt+$rowspan][$bn];
# 		  my ($previous_rowspan) = $previous_br->{rowspan};
# 		  if (&has_who_list($tt+$rowspan,
# 				   $tt+$rowspan+$previous_rowspan-1)) {
# 		    $qr = &query_ref($br->{td}, 
# 				     $br->{previousbuildtime},
# 				     $br->{buildtime});
# 		    print "\n$qr";
# 		    print "C</a>";
# 		  }
#                 }

    if ($br->{binaryname} ne '') {
      $binfile = "$buildtree/bin/$buildtime/$br->{buildname}/"
                ."$br->{binaryname}";
      $binfile =~ s/ //g;
      print " <a href=$binfile>B</a>";
    }
    print "</tt>\n</td>";
  }
  print "</tr>\n";
}

sub print_table_header {
  my $ii, $nspan;

  print "<table border=1 bgcolor='#FFFFFF' cellspacing=1 cellpadding=1>\n";

  print "<tr align=center>\n";
  print "<td rowspan=1><font size=-1>Click time to <br>see changes <br>",
        "since time</font></td>";
  $nspan = ($tree2 ne '' ? 4 : 1);
  print "<td colspan=$nspan><font size=-1>",
        "Click name to see what they did</font>";
  print "<br><font size=-2><a href=showbuilds.cgi",make_cgi_args(),
        "&rebuildguilty=1>Rebuild guilty list</a></td>";

  for ($ii=1; $ii <= $name_count; $ii++) {

    my $bn = $build_name_names->[$ii];
    $bn =~ s/Clobber/Clbr/g;
    $bn =~ s/Depend/Dep/g;

    if( $form{narrow} ){
      $bn =~ s/([^:])/$1<br>/g;
      $bn = "<tt>$bn</tt>";
    }
    else {
      $bn = "<font face='Arial,Helvetica' size=-2>$bn</font>";
    }

    my $last_status = &last_status($ii);
    if ($last_status eq 'busted') {
      if ($form{nocrap}) {
        print "<th rowspan=2 bgcolor=$colormap->{busted}>$bn</th>";
      } else {
        print "<td rowspan=2 bgcolor=000000 background=1afi003r.gif>";
        print "<font color=white>$bn</font></td>";
      }
    }
    else {
      print "<th rowspan=2 bgcolor=$colormap->{$last_status}>$bn</th>";
    }
  }
  print "</tr><tr>\n";
  print "<b><TH>Build Time</th>\n";

  if ($tree2 ne '') {
    print "<TH colspan=2>$td1->{name}</th>\n";
    print "<TH colspan=2>$td2->{name}</th>\n";
  }
  else {
    print "<TH>Guilty</th>\n";
  }
  print "</b></tr>\n";
}

sub print_table_footer {
  print "</table>\n";

  my $nextdate = $maxdate - $hours*60*60;
  print "<a href='showbuilds.cgi",
        "?tree=$tree&hours=$hours&maxdate=$nextdate&nocrap=1'>",
        "Show next $hours hours</a>";

  if (open(FOOTER, "<$data_dir/footer.html")) {
    while (<FOOTER>) {
      print $_;
    }
    close FOOTER;
  }
  print "<p><a href=admintree.cgi?tree=$tree>",
        "Administrate Tinderbox Trees</a><br>";
  print "<br><br>";
}

sub query_ref {
  my ($td, $mindate, $maxdate, $who) = @_;
  my $output = '';

  $output = "<a href=../bonsai/cvsquery.cgi?module=$td->{cvs_module}";
  $output .= "&branch=$td->{cvs_branch}"   if $td->{cvs_branch} ne 'HEAD';
  $output .= "&cvsroot=$td->{cvs_root}"    if $td->{cvs_root} ne $default_root;
  $output .= "&date=explicit&mindate=$mindate";
  $output .= "&maxdate=$maxdate"           if $maxdate ne '';
  $output .= "&who=$who"                   if $who ne '';
  $output .= ">";
}

sub query_ref2 {
  my ($td, $mindate, $maxdate, $who) = @_;
  return "../bonsai/cvsquery.cgi?module=$td->{cvs_module}"
        ."&branch=$td->{cvs_branch}&cvsroot=$td->{cvs_root}"
        ."&date=explicit&mindate=$mindate&maxdate=$maxdate&who="
        . url_encode($who);
}

sub who_menu {
  my ($td, $mindate, $maxdate, $who) = @_;
  my $treeflag;

  $qr = "../registry/who.cgi?email=$e"
       ."&t0=". &url_encode("What did $who check into the source tree")
       ."&u0=". &url_encode( &query_ref2($td,$mindate,$maxdate,$who))
       ."&t1=". &url_encode("What has $who been checking in in the last day")
       ."&u1=". &url_encode( &query_ref2($td,$mindate,$maxdate,$who));

  return "<a href='$qr' onclick=\"return js_who_menu($td->{num},'$who',"
        ."event,$mindate,$maxdate);\">";
}

# Check to see if anyone checked in during time slot.
#   ex.  has_who_list(1);    # Check for checkins in most recent time slot.
#   ex.  has_who_list(1,5);  # Check range of times.
sub has_who_list {
  my ($time1, $time2) = @_;

  if (not defined(@who_check_list)) {
    # Build a static array of true/false values for each time slot.
    $who_check_list[$time_count] = 0;
    my ($t) = 1;
    for (; $t<=$time_count; $t++) {
      $who_check_list[$t] = 1 if each %{$who_list->[$t]};
    }
  }
  if ($time2) {
    for ($ii=$time1; $ii<=$time2; $ii++) {	 
      return 1 if $who_check_list[$ii]
    }
    return 0
  } else {
    return $who_check_list[$time1]; 
  }
}

sub tree_open {
  my $done, $line, $a, $b;
  open(BID, "<../bonsai/data/$bonsai_tree/batchid")
    or print "can't open batchid<br>";
  ($a,$b,$bid) = split / /, <BID>;
  close(BID);
  open(BATCH, "<../bonsai/data/$bonsai_tree/batch-${bid}")
    or print "can't open batch-${bid}<br>";;
  $done = 0;
  while (($line = <BATCH>) and not $done){ 
    if ($line =~ /^set treeopen/) {
      chop $line;
      ($a,$b,$treestate) = split / /, $line ;
      $done = 1;
    }
  }
  close(BATCH);
  return $treestate;
}

sub print_javascript {

  print <<'__ENDJS';
    <script>

    if (parseInt(navigator.appVersion) < 4) {
      window.event = 0;
    }

    function js_who_menu(tree,n,d,mindate,maxdate) {
      var who_link = "../registry/who.cgi?email=" + escape(n)
                   + "&t0=" + escape("Last check-in")
                   + "&u0=" + escape(js_qr(tree,mindate,maxdate,n))
                   + "&t1=" + escape("Check-ins within 24 hours")
                   + "&u1=" + escape(js_qr24(tree,n));

      var version = parseInt(navigator.appVersion);
      if (version < 4 || version >= 5) {
        document.location = who_link;
        return false;
      }

      var l = document.layers['popup'];
      l.src = who_link;
      l.top = d.target.y - 6;
      l.left = d.target.x - 6;
      if (l.left + l.clipWidth > window.width) {
        l.left = window.width - l.clipWidth;
      }
      l.visibility="show";
      return false;
    }

    function log_params(buildindex,logfile,buildtime) {
      var tree = trees[buildindex];
      var buildname = build[buildindex];
      var errorparser = error[buildindex];

      return "tree=" + tree
             + "&errorparser=" + errorparser
             + "&buildname="   + escape(buildname)
             + "&buildtime="   + buildtime
             + "&logfile="     + logfile;
    }

    function log_url(buildindex,logfile,buildtime) {
      return "showlog.cgi?" + log_params(buildindex,logfile,buildtime);
    }

    function comment_url (buildindex,logfile,buildtime) {
      return "addnote.cgi?" + log_params(buildindex,logfile,buildtime);
    }

    function js_what_menu(d,noteid,buildindex,logfile,buildtime) {
      var version = parseInt(navigator.appVersion);
      if (version < 4 || version >= 5) {
        document.location = log_url(buildindex,logfile,buildtime);
        return false;
      }

      var l = document.layers['popup'];
      l.document.write("<table border=1 cellspacing=1><tr><td>"
                       + note[noteid] + "</tr></table>");
      l.document.close();

      l.top = d.y-10;
      var zz = d.x;
      if (zz + l.clip.right > window.innerWidth) {
        zz = (window.innerWidth-30) - l.clip.right;
        if (zz < 0) {
          zz = 0;
        }
      }
      l.left = zz;
      l.visibility="show";
      return false;
    }

    note = new Array();
    trees = new Array();
    build = new Array();
    error = new Array();

    function log_popup(e,buildindex,logfile,buildtime)
    {
      var logurl = log_url(buildindex,logfile,buildtime);
      var commenturl = comment_url(buildindex,logfile,buildtime);
      var version = parseInt(navigator.appVersion);

      if (version < 4 || version >= 5) {
        document.location = logurl;
        return false;
      }
      var q = document.layers["logpopup"];
      q.top = e.target.y - 6;

      var yy = e.target.x;
      if ( yy + q.clip.right > window.innerWidth) {
        yy = (window.innerWidth-30) - q.clip.right;
        if (yy < 0) {
          yy = 0;
        }
      }
      q.left = yy;
      q.visibility="show"; 
      q.document.write("<TABLE BORDER=1><TR><TD><B>"
        + build[buildindex] + "</B><BR>"
        + "<A HREF=\"" + logurl + "\">View Brief Log</A><BR>"
        + "<A HREF=\"" + logurl + "&fulltext=1" + "\">View Full Log</A><BR>"
        + "<A HREF=\"" + commenturl + "\">Add a Comment</A><BR>"
	+ "</TD></TR></TABLE>");
      q.document.close();
      return false;
    }

    function js_qr(tree,mindate, maxdate, who) {
      if (tree == 0) {
        return '../bonsai/cvsquery.cgi?module=${cvs_module}'
               + '&branch=${cvs_branch}&cvsroot=${cvs_root}&date=explicit'
               + '&mindate=' + mindate + '&maxdate=' +maxdate + '&who=' + who;
      }
__ENDJS

  print <<'__ENDJS' if $tree2 ne '';
      else {
        return '../bonsai/cvsquery.cgi?module=$td2->{cvs_module}'
             + '&branch=$td2->{cvs_branch}&cvsroot=$td2->{cvs_root}'
             + '&date=explicit'
             + '&mindate=' + mindate + '&maxdate=' +maxdate + '&who=' + who;
      }
__ENDJS

  print <<'__ENDJS';
    }

    function js_qr24(tree,who) {
      if (tree == 0 ){
        return '../bonsai/cvsquery.cgi?module=${cvs_module}'
             + '&branch=${cvs_branch}&cvsroot=${cvs_root}&date=day&who=' + who;
      }
__ENDJS

  print <<'__ENDJS' if $tree2 ne '';
      else {
        return '../bonsai/cvsquery.cgi?module=$td2->{cvs_module}'
               + '&branch=$td2->{cvs_branch}&cvsroot=$td2->{cvs_root}'
               + '&date=day&who=' +who;
      }
__ENDJS

  print <<'__ENDJS';
    }
__ENDJS

  $ii = 0;
  while ($ii < @note_array) {
    $ss = $note_array[$ii];
    while ($ii < @note_array && $note_array[$ii] eq $ss) {
      print "note[$ii] = ";
      $ii++;
    }
    $ss =~ s/\\/\\\\/g;
    $ss =~ s/\"/\\\"/g;
    $ss =~ s/\n/\\n/g;
    print "\"$ss\";\n";
  }

  for ($ii=1; $ii <= $name_count; $ii++) {
    if (defined($br = $build_table->[1][$ii]) and $br != -1) {
      $bn = $build_name_names->[$ii];
      print "trees[$ii]='$br->{td}{name}';\n";
      print "build[$ii]='$bn';\n";
      print "error[$ii]='$br->{errorparser}';\n";
    }
  }

  print <<'__ENDJS';
    </script>
    <layer name="popup" onMouseOut="this.visibility='hide';" 
           left=0 top=0 bgcolor="#ffffff" visibility="hide">
    </layer>

    <layer name="logpopup" onMouseOut="this.visibility='hide';"
           left=0 top=0 bgcolor="#ffffff" visibility="hide">
    </layer>
__ENDJS
}

sub loadquickparseinfo {
  my ($tree, $build, $times) = (@_);

  do "$tree/ignorebuilds.pl";
    
  use Backwards;

  my ($bw) = Backwards->new("$form{tree}/build.dat") or die;
    
  my $latest_time = 0;
  my $tooearly = 0;
  while( $_ = $bw->readline ) {
    chop;
    my ($buildtime, $buildname, $buildstatus) = (split /\|/)[1,2,4];
    
    if ($buildstatus =~ /^success|busted|testfailed$/) {

      # Ignore stuff in the future.
      next if $buildtime > $maxdate;

      $latest_time = $buildtime if $buildtime > $latest_time;

      # Ignore stuff more than 12 hours old
      if ($buildtime < $latest_time - 12*60*60) {
        # Hack: A build may give a bogus time. To compensate, we will
        # not stop until we hit 20 consecutive lines that are too early.

        last if $tooearly++ > 20;
        next;
      }
      $tooearly = 0;

      next if exists $ignore_builds->{$buildname};
      next if exists $build->{$buildname}
              and $times->{$buildname} >= $buildtime;
      
      $build->{$buildname} = $buildstatus;
      $times->{$buildname} = $buildtime;
    }
  }
}

sub do_express {
  print "Content-type: text/html\nRefresh: 900\n\n<HTML>\n";

  my %build, %times;
  loadquickparseinfo($form{tree}, \%build, \%times);

  my @keys = sort keys %build;
  my $keycount = @keys;
  my $tm = &print_time(time);
  print "<table border=1 cellpadding=1 cellspacing=1><tr>";
  print "<th align=left colspan=$keycount>";
  print "<a href=showbuilds.cgi?tree=$form{tree}";
  print "&hours=$form{hours}" if $form{hours};
  print "&nocrap=1" if $form{nocrap};
  print ">$tree as of $tm</a></tr><tr>\n";
  foreach my $buildname (@keys) {
    if ($build{$buildname} eq 'busted') {
      if ($form{nocrap}) {
        print "<td bgcolor=$colormap->{busted}>";
      } else {
        print "<td bgcolor=000000 background=1afi003r.gif>";
        print "<font color=white>\n";
      }
    } else {
      print "<td bgcolor='$colormap->{$build{$buildname}}'>";
    }
    print "$buildname</td>";
  }
  print "</tr></table>\n";
}

# This is essentially do_express but it outputs a different format
sub do_panel {
  # Refresh the tinderbox sidebar panel every minute.
  print "Content-type: text/html\nRefresh: 60\n\n<HTML>\n";

  my %build, %times;
  loadquickparseinfo($form{tree}, \%build, \%times);
  
  my @keys = sort keys %build;
  my $keycount = @keys;
  
  my ($minute,$hour,$mday,$mon) = (localtime)[1..4];
  my $tm = sprintf("%d/%d&nbsp;%d:%02d",$mon+1,$mday,$hour,$minute);
  
  print q(
    <head>
      <style>
        body, td { 
          font-family: Verdana, Sans-Serif;
          font-size: 8pt;
        }
      </style>
    </head>
    <body BGCOLOR="#FFFFFF" TEXT="#000000" 
          LINK="#0000EE" VLINK="#551A8B" ALINK="#FF0000">
  );
  print "<a href=showbuilds.cgi?tree=$form{tree}";
  print "&hours=$form{hours}" if $form{hours};
  print "&nocrap=1" if $form{nocrap};
  print ">$tree";
  
  $bonsai_tree = '';
  require "$tree/treedata.pl";
  if ($bonsai_tree ne "") {
    print " is ", tree_open() ? "OPEN" : "CLOSED";
  }
  print " as of $tm</a><br>";
  
  print "<table border=0 cellpadding=1 cellspacing=1>";
  for $buildname (@keys) {
    print "<tr><td bgcolor='";
    print $colormap->{$build{$buildname}};
    print "'>$buildname</td></tr>";
  }
  print "</table></body>";
}

sub do_flash {
  print "Content-type: text/rdf\n\n";

  my %build, %times;
  loadquickparseinfo($form{tree}, \%build, \%times);

  my @keys = keys %build;

  # The Flash spec says we need to give ctime.
  use POSIX;
  my $tm = POSIX::ctime(time());
  chop $tm;
  
  my ($mac,$unix,$win) = (0,0,0);

  for (@keys) {
    #print $_ . ' ' .  $build{$_} . "\n";
    next if $build{$_} eq 'success';
    $mac = 1, next if /Mac/;
    $win = 1, next if /Win/;
    $unix = 1;
  }

  print qq{
    <RDF:RDF xmlns:RDF='http://www.w3.org/1999/02/22-rdf-syntax-ns#' 
             xmlns:NC='http://home.netscape.com/NC-rdf#'>
    <RDF:Description about='NC:FlashRoot'>
  };

  my $busted = $mac + $unix + $win;
  if ($busted) {

    # Construct a legible sentence; e.g., "Mac, Unix, and Windows
    # are busted", "Windows is busted", etc. This is hideous. If
    # you can think of something better, please fix it.

    my $description;
    if ($mac) {
      $text .= 'Mac' . ($busted > 2 ? ', ' : ($busted > 1 ? ' and ' : ''));
    }
    if ($unix) {
      $text .= 'Unix' . ($busted > 2 ? ', and ' : ($win ? ' and ' : ''));
    }
    if ($win) {
      $text .= 'Windows';
    }
    $text .= ($busted > 1 ? ' are ' : ' is ') . 'busted.';
    
    print qq{
      <NC:child>
        <RDF:Description ID='flash'>
          <NC:type resource='http://www.mozilla.org/RDF#TinderboxFlash' />
          <NC:source>$tree</NC:source>
          <NC:description>$text</NC:description>
          <NC:timestamp>$tm</NC:timestamp>
        </RDF:Description>
      </NC:child>
    };
  }

  print qq{
    </RDF:Description>
    </RDF:RDF>
  };
}

sub do_quickparse {
  print "Content-type: text/plain\n\n";

  if ($tree eq '') {
    foreach my $tt (make_tree_list()) {
      print "$tt\n";
    }
    exit;
  }
  my @treelist = split /,/, $tree;
  foreach my $t (@treelist) {
    $bonsai_tree = "";
    require "$t/treedata.pl";
    if ($bonsai_tree ne "") {
      my $state = tree_open() ? "Open" : "Close";
      print "State|$t|$bonsai_tree|$state\n";
    }
    my %build, %times;
    loadquickparseinfo($t, \%build, \%times);
    
    foreach my $buildname (sort keys %build) {
      print "Build|$t|$buildname|$build{$buildname}\n";
    }
  }
}

sub do_rdf {
  print "Content-type: text/plain\n\n";

  my $mainurl = "http://$ENV{SERVER_NAME}$ENV{SCRIPT_NAME}?tree=$tree";
  my $dirurl = $mainurl;

  $dirurl =~ s@/[^/]*$@@;

  my %build, %times;
  loadquickparseinfo($tree, \%build, \%times);

  my $image = "channelok.gif";
  my $imagetitle = "OK";
  foreach my $buildname (sort keys %build) {
    if ($build{$buildname} eq 'busted') {
      $image = "channelflames.gif";
      $imagetitle = "Bad";
      last;
    }
  }
  print qq{<?xml version="1.0"?>
    <rdf:RDF 
         xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
         xmlns="http://my.netscape.com/rdf/simple/0.9/">
    <channel>
      <title>Tinderbox - $tree</title>
      <description>Build bustages for $tree</description>
      <link>$mainurl</link>
    </channel>
    <image>
      <title>$imagetitle</title>
      <url>$dirurl/$image</url>
      <link>$mainurl</link>
    </image>
  };    
    
  $bonsai_tree = '';
  require "$tree/treedata.pl";
  if ($bonsai_tree ne '') {
    my $state = tree_open() ? "OPEN" : "CLOSED";
    print "<item><title>The tree is currently $state</title>",
          "<link>$mainurl</link></item>\n";
  }

  foreach my $buildname (sort keys %build) {
    if ($build{$buildname} eq 'busted') {
      print "<item><title>$buildname is in flames</title>",
            "<link>$mainurl</link></item>\n";
    }
  }
  print "</rdf:RDF>\n";
}
