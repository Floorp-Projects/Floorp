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
require 'tbglobals.pl';
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
  $default_hours = 12;
  $hours = $default_hours;
  $hours = $form{hours} if $form{hours};
  $mindate = $maxdate - ($hours*60*60);
}

%colormap = (
  success    => '00ff00',
  busted     => 'red',
  building   => 'yellow',
  testfailed => 'orange'
);

%images = (
  flames    => '1afi003r.gif',
  star      => 'star.gif'
);

$tree = $form{tree};

if (exists $form{rebuildguilty} or exists $form{showall}) {
  system ("./buildwho.pl -days 7 $tree > /dev/null");
  undef $form{rebuildguilty};
}
&show_tree_selector,  exit if $form{tree} eq '';
&do_quickparse,       exit if $form{quickparse};
&do_express,          exit if $form{express};
&do_rdf,              exit if $form{rdf};
&do_static,           exit if $form{static};
&do_flash,            exit if $form{flash};
&do_panel,            exit if $form{panel};
&do_tinderbox,        exit;

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

  print "Content-type: text/html\n\n";

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

sub do_static {
  local *OUT;

  $form{nocrap}=1;

  my @pages = ( ['index.html', 'do_tinderbox'],
                ['flash.rdf',  'do_flash'],
                ['panel.html', 'do_panel'] );
  
  $rel_path = '../';
  while (($key, $value) = each %images) {
    $images{$key} = "$rel_path$value";
  }

  my $oldfh = select;

  foreach $pair (@pages) {
    my ($page, $call) = @{$pair};
    my $outfile = "$form{tree}/$page";

    open(OUT,">$outfile.$$");
    select OUT;

    eval "$call";

    close(OUT);
    system "mv $outfile.$$ $outfile";
  }
  select $oldfh;
}

sub do_tinderbox {
  &load_data;
  &print_page_head;
  &print_table_header;
  &print_table_body;
  &print_table_footer;
}

sub print_page_head {
  print "Content-type: text/html\n\n<HTML>\n" unless $form{static};

  # Get the message of the day only on the first pageful
  do "$tree/mod.pl" if $nowdate eq $maxdate;

  # Get the warnings summary
  do "$tree/warn.pl" if $nowdate eq $maxdate;

  use POSIX qw(strftime);
  # Print time in format, "HH:MM timezone"
  my $now = strftime("%H:%M %Z", localtime);

  EmitHtmlTitleAndHeader("tinderbox: $tree", "tinderbox",
                         "tree: $tree ($now)");

  &print_javascript;

  print "$message_of_day\n";  # from $tree/mod.pl
  print "$warning_summary\n"; # from $tree/warn.pl
  
  # Quote and Lengend
  #
  unless ($form{nocrap}) {
    my ($imageurl,$imagewidth,$imageheight,$quote) = &get_image;
    print qq{
      <table width="100%" cellpadding=0 cellspacing=0>
        <tr>
          <td valign=bottom>
            <p><center><a href=addimage.cgi><img src="$rel_path$imageurl"
              width=$imagewidth height=$imageheight><br>
              $quote</a><br>
            </center>
            <p>
          <td align=right valign=bottom>
            <table cellspacing=0 cellpadding=1 border=0><tr><td align=center>
              <TT>L</TT></td><td>= Show Build Log
            </td></tr><tr><td align=center>
              <img src="$images{star}"></td><td>= Show Log comments
            </td></tr><tr><td colspan=2>
              <table cellspacing=1 cellpadding=1 border=1>
                <tr bgcolor="$colormap{success}"><td>Successful Build
                <tr bgcolor="$colormap{building}"><td>Build in Progress
                <tr bgcolor="$colormap{testfailed}"><td>Successful Build,
                                                          but Tests Failed
                <tr bgcolor="$colormap{busted}"><td>Build Failed
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

  if ($lasthour != $hour or &has_who_list($tt)) {
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
  print "<tr align=center><td align=right$hour_color>",
        "$query_link\n$pretty_time$end_query</td>\n";

  # Guilty
  #
  print '<td>';
  for $who (sort keys %{$who_list->[$tt]} ){
    $qr = &who_menu($td1, $build_time_times->[$tt],
                    $build_time_times->[$tt-1],$who);
    $who =~ s/%.*$//;
    print "  ${qr}$who</a>\n";
  }
  print '</td>';

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
    $color = $colormap{$br->{buildstatus}};
    $status = $br->{buildstatus};
    print "<td rowspan=$rowspan bgcolor=${color}>\n";
      
    $logfile = $br->{logfile};
    $errorparser = $br->{errorparser};
    $buildname = $br->{buildname};
    $buildtime = $br->{buildtime};
    $buildtree = $br->{td}->{name};
      
    print "<tt>\n";
        
    # Build Note
    # 
    $buildname = &url_encode($buildname);
    my $logurl = "${rel_path}showlog.cgi?log=$buildtree/$logfile";

    if ($hasnote) {
      print "<a href='$logurl' onclick=\"return ",
            "note(event,$noteid,'$logfile');\">",
            "<img src='$images{star}' border=0></a>\n";
    }
        
    # Build Log
    # 
    print "<A HREF='$logurl' onclick=\"return log(event,$bn,'$logfile');\">";
    print "L</a>";
      
    # What Changed
    #
    if( $br->{previousbuildtime} ){
      my $previous_br = $build_table->[$tt+$rowspan][$bn];
      my $previous_rowspan = $previous_br->{rowspan};
      if (&has_who_list($tt+$rowspan,
                        $tt+$rowspan+$previous_rowspan-1)) {
        print "\n", &query_ref($br->{td}, 
                               $br->{previousbuildtime},
                               $br->{buildtime});
        print "C</a>";
      }
    }

    if ($br->{binaryname} ne '') {
      $binfile = "$buildtree/bin/$buildtime/$br->{buildname}/"
                ."$br->{binaryname}";
      $binfile =~ s/ //g;
      print " <a href=$rel_path$binfile>B</a>";
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
  print "<td><font size=-1>",
        "Click name to see what they did</font>";
  print "<br><font size=-2>", 
        &open_showbuilds_href(rebuildguilty=>'1'),
        "Rebuild guilty list</a></td>";

  for ($ii=1; $ii <= $name_count; $ii++) {

    my $bn = $build_name_names->[$ii];
    $bn =~ s/Clobber/Clbr/g;
    $bn =~ s/Depend/Dep/g;
    $bn = "<font face='Helvetica,Arial' size=-1>$bn</font>";

    my $last_status = &last_status($ii);
    if ($last_status eq 'busted') {
      if ($form{nocrap}) {
        print "<td rowspan=2 bgcolor=$colormap{busted}>$bn</td>";
      } else {
        print "<td rowspan=2 bgcolor=000000 background='$images{flames}'>";
        print "<font color=white>$bn</font></td>";
      }
    }
    else {
      print "<td rowspan=2 bgcolor=$colormap{$last_status}>$bn</td>";
    }
  }
  print "</tr><tr>\n";
  print "<TH>Build Time</TH>\n";
  print "<TH>Guilty</th>\n";
  print "</tr>\n";
}

sub print_table_footer {
  print "</table>\n";

  my $nextdate = $maxdate - $hours*60*60;
  print &open_showbuilds_href(maxdate=>"$nextdate", nocrap=>'1')
       ."Show next $hours hours</a>";

  print "<p><a href='${rel_path}admintree.cgi?tree=$tree'>",
        "Administrate Tinderbox Trees</a><br>\n";
}

sub open_showbuilds_url {
  my %args = (
        nocrap => "$form{nocrap}",
        @_
  );

  my $url = "${rel_path}showbuilds.cgi?tree=$form{tree}";
  $url .= "&hours=$hours" if $hours ne $default_hours;
  while (my ($key, $value) = each %args) {
    $url .= "&$key=$value" if $value ne '';
  }
  return $url;
}

sub open_showbuilds_href {
  return "<a href=".open_showbuilds_url(@_).">";
}

sub query_ref {
  my ($td, $mindate, $maxdate, $who) = @_;
  my $output = '';

  $output = "<a href=${rel_path}../bonsai/cvsquery.cgi";
  $output .= "?module=$td->{cvs_module}";
  $output .= "&branch=$td->{cvs_branch}"   if $td->{cvs_branch} ne 'HEAD';
  $output .= "&cvsroot=$td->{cvs_root}"    if $td->{cvs_root} ne $default_root;
  $output .= "&date=explicit&mindate=$mindate";
  $output .= "&maxdate=$maxdate"           if $maxdate ne '';
  $output .= "&who=$who"                   if $who ne '';
  $output .= ">";
}

sub query_ref2 {
  my ($td, $mindate, $maxdate, $who) = @_;
  return "${rel_path}../bonsai/cvsquery.cgi?module=$td->{cvs_module}"
        ."&branch=$td->{cvs_branch}&cvsroot=$td->{cvs_root}"
        ."&date=explicit&mindate=$mindate&maxdate=$maxdate&who="
        . url_encode($who);
}

sub who_menu {
  my ($td, $mindate, $maxdate, $who) = @_;
  my $treeflag;

  $qr = "${rel_path}../registry/who.cgi?email=". url_encode($who)
      . "&d=$td->{cvs_module}|$td->{cvs_branch}|$td->{cvs_root}|$mindate|$maxdate";

  return "<a href='$qr' onclick=\"return who(event);\">";
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
  my $line, $treestate;
  open(BID, "<../bonsai/data/$bonsai_tree/batchid.pl")
    or print "can't open batchid<br>";
  $line = <BID>;
  close(BID);
  if ($line =~ m/'(\d+)'/) {
      $bid = $1;
  } else {
      return 0;
  }
  open(BATCH, "<../bonsai/data/$bonsai_tree/batch-${bid}.pl")
    or print "can't open batch-${bid}.pl<br>";
  while ($line = <BATCH>){ 
    if ($line =~ /^\$::TreeOpen = '(\d+)';/) {
        $treestate = $1;
        last;
    }
  }
  close(BATCH);
  return $treestate;
}

sub print_javascript {
  my $script;
  ($script = <<"__ENDJS") =~ s/^    //gm;
    <script>
    if (parseInt(navigator.appVersion) < 4) {
      window.event = 0;
    }

    function who(d) {
      var version = parseInt(navigator.appVersion);
      if (version < 4 || version >= 5) {
        return true;
      }
      var l  = document.layers['popup'];
      l.src  = d.target.href;
      l.top  = d.target.y - 6;
      l.left = d.target.x - 6;
      if (l.left + l.clipWidth > window.width) {
        l.left = window.width - l.clipWidth;
      }
      l.visibility="show";
      return false;
    }
    function log_url(logfile) {
      return "showlog.cgi?log=" + buildtree + "/" + logfile;
    }
    function note(d,noteid,logfile) {
      var version = parseInt(navigator.appVersion);
      if (version < 4 || version >= 5) {
        document.location = log_url(logfile);
        return false;
      }
      var l = document.layers['popup'];
      l.document.write("<table border=1 cellspacing=1><tr><td>"
                       + notes[noteid] + "</tr></table>");
      l.document.close();

      l.top = d.y-10;
      var zz = d.x;
      if (zz + l.clip.right > window.innerWidth) {
        zz = (window.innerWidth-30) - l.clip.right;
        if (zz < 0) { zz = 0; }
      }
      l.left = zz;
      l.visibility="show";
      return false;
    }
    function log(e,buildindex,logfile)
    {
      var logurl = log_url(logfile);
      var commenturl = "addnote.cgi?log=" + buildtree + "/" + logfile;
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
        if (yy < 0) { yy = 0; }
      }
      q.left = yy;
      q.visibility="show"; 
      q.document.write("<TABLE BORDER=1><TR><TD><B>"
        + builds[buildindex] + "</B><BR>"
        + "<A HREF=$rel_path" + logurl + ">View Brief Log</A><BR>"
        + "<A HREF=$rel_path" + logurl + "&fulltext=1"+">View Full Log</A><BR>"
        + "<A HREF=$rel_path" + commenturl + ">Add a Comment</A><BR>"
	+ "</TD></TR></TABLE>");
      q.document.close();
      return false;
    }

    notes = new Array();
    builds = new Array();

__ENDJS
  print $script;
  
  $ii = 0;
  while ($ii < @note_array) {
    $ss = $note_array[$ii];
    while ($ii < @note_array && $note_array[$ii] eq $ss) {
      print "notes[$ii] = ";
      $ii++;
    }
    $ss =~ s/\\/\\\\/g;
    $ss =~ s/\"/\\\"/g;
    $ss =~ s/\n/\\n/g;
    print "\"$ss\";\n";
  }
  for ($ii=1; $ii <= $name_count; $ii++) {
    if (defined($br = $build_table->[1][$ii]) and $br != -1) {
      my $bn = $build_name_names->[$ii];
      print "builds[$ii]='$bn';\n";
    }
  }
  print "buildtree = '$form{tree}';\n";

  # Use JavaScript to refresh the page every 15 minutes
  print "setTimeout('location.reload()',900000);\n" if $nowdate eq $maxdate;

  ($script = <<'__ENDJS') =~ s/^    //gm;
    </script>

    <layer name="popup" onMouseOut="this.visibility='hide';" 
           left=0 top=0 bgcolor="#ffffff" visibility="hide">
    </layer>

    <layer name="logpopup" onMouseOut="this.visibility='hide';"
           left=0 top=0 bgcolor="#ffffff" visibility="hide">
    </layer>
__ENDJS
  print $script;
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
  print &open_showbuilds_href."$tree as of $tm</a></tr><tr>\n";
  foreach my $buildname (@keys) {
    print "<td bgcolor='$colormap{$build{$buildname}}'>$buildname</td>";
  }
  print "</tr></table>\n";
}

# This is essentially do_express but it outputs a different format
sub do_panel {
  print "Content-type: text/html\n\n<HTML>\n" unless $form{static};

  my %build, %times;
  loadquickparseinfo($form{tree}, \%build, \%times);
  
  print q(
    <head>
      <META HTTP-EQUIV="Refresh" CONTENT="300">
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
  # Hack the panel link for now.
  print "<a target='_content' href='http://tinderbox.mozilla.org/seamonkey/'>$tree";
  
  $bonsai_tree = '';
  require "$tree/treedata.pl";
  if ($bonsai_tree ne '') {
    print " is ", tree_open() ? "OPEN" : "CLOSED";
  }
  # Add the current time
  my ($minute,$hour,$mday,$mon) = (localtime)[1..4];
  my $tm = sprintf("%d/%d&nbsp;%d:%02d",$mon+1,$mday,$hour,$minute);
  print " as of $tm</a><br>";
  
  print "<table border=0 cellpadding=1 cellspacing=1>";
  while (my ($name, $status) = each %build) {
    print "<tr><td bgcolor='$colormap{$status}'>$name</td></tr>";
  }
  print "</table></body>";
}

sub do_flash {
  print "Content-type: text/rdf\n\n" unless $form{static};

  my %build, %times;
  loadquickparseinfo($form{tree}, \%build, \%times);

  my ($mac,$unix,$win) = (0,0,0);

  while (my ($name, $status) = each %build) {
    next if $status eq 'success';
    $mac = 1, next if $name =~ /Mac/;
    $win = 1, next if $name =~ /Win/;
    $unix = 1;
  }

  print q{
    <RDF:RDF xmlns:RDF='http://www.w3.org/1999/02/22-rdf-syntax-ns#' 
             xmlns:NC='http://home.netscape.com/NC-rdf#'>
    <RDF:Description about='NC:FlashRoot'>
  };

  my $busted = $mac + $unix + $win;
  if ($busted) {

    # Construct a legible sentence; e.g., "Mac, Unix, and Windows
    # are busted", "Windows is busted", etc. This is hideous. If
    # you can think of something better, please fix it.

    my $text;
    if ($mac) {
      $text .= 'Mac' . ($busted > 2 ? ', ' : ($busted > 1 ? ' and ' : ''));
    }
    if ($unix) {
      $text .= 'Unix' . ($busted > 2 ? ', and ' : ($win ? ' and ' : ''));
    }
    if ($win) {
      $text .= 'Windows';
    }
    $text .= ($busted > 1 ? ' are ' : ' is ') . 'busted';
    
    # The Flash spec says we need to give ctime.
    use POSIX;
    my $tm = POSIX::ctime(time());
    $tm =~ s/^...\s//;   # Strip day of week
    $tm =~ s/:\d\d\s/ /; # Strip seconds
    chop $tm;
  
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
  print q{
    </RDF:Description>
    </RDF:RDF>
  };
}

sub do_quickparse {
  print "Content-type: text/plain\n\n";

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

