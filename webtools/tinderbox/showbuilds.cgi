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

use lib "../bonsai";

require 'globals.pl';
require 'lloydcgi.pl';
require 'imagelog.pl';
require 'header.pl';
$|=1;

#require 'utils.pl';
#$default_root = pickDefaultRepository();
# Terrible hack until I can figure out how to do this properly. -slamm
$default_root = '/cvsroot';

#
# show 12 hours by default
#
$nowdate = time;
if (not defined($maxdate = $form{maxdate})) {
  $maxdate = $nowdate;
}
if($form{'showall'} != 0 ){
    $mindate = 0;
}
else {
    $hours = 12;
    $hours = $form{hours} if $form{hours} ne "";
    $mindate = $maxdate - ($hours*60*60);
}

$colormap = {
                success => '00ff00',
                busted => 'red',
                building => 'yellow',
		testfailed => 'orange'
            };

#
# Debug hack 
#
#$form{'tree'} = DogbertTip;
$tree = $form{'tree'};

if (exists $form{'rebuildguilty'} || exists $form{'showall'}) {
    system ("./buildwho.pl -days 7 $tree > /dev/null");
    undef $form{'rebuildguilty'};
}

if ($form{'quickparse'}) {
    print "Content-type: text/plain\n\n";
    &do_quickparse;
    exit();
}

if ($form{'rdf'}) {
    print "Content-type: text/plain\n\n";
    &do_rdf;
    exit();
}


if ($nowdate eq $maxdate) {
  print "Content-type: text/html\nRefresh: 900\n\n<HTML>\n";
} else {
  print "Content-type: text/html\n\n<HTML>\n";
}
if( $form{'tree'} eq '' ){
    &show_tree_selector;
    exit;
}
else {
    if( $form{'express'} ) {
        &do_express;
    }
    else {
        &load_data;
        &load_javascript;
        &display_page_head;
        &display_build_table;
    }
}



sub make_tree_list {
    my @result;
    while(<*>) {
        if( -d $_ && $_ ne 'data' && $_ ne 'CVS' && -f "$_/treedata.pl"){
            push @result, $_;
        }
    }
    return @result;
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
    print "<TR><TD ALIGN=CENTER><a href=admintree.cgi>Administer</a> one of the following trees:</TD></TR>";
    print "<TR><TD ALIGN=CENTER>\n";
    print " <TABLE><TR><TD><UL>\n";

    foreach (@list) {
        print "<LI><a href=admintree.cgi?tree=$_>$_</a>\n";
    }
    print "<//UL></TD></TR></TABLE></TD></TR></TABLE>";

}



sub display_page_head {

# srand;
# $when = 60*10 + int rand(60*40);
# <META HTTP-EQUIV=\"REFRESH\" CONTENT=\"$when\">

    
if( -r "$tree/mod.pl" ){
    require "$tree/mod.pl";
}
else {
    $message_of_day = "";
}

$treename = $tree . ($tree2 ne "" ? " and $tree2" : "" );

    my ($sec,$minute,$hour) = localtime(time());
    my $now = sprintf("%02d:%02d", $hour, $minute);
    EmitHtmlTitleAndHeader("tinderbox: $treename", "tinderbox",
                           "tree: $treename ($now)");

    print "$script_str\n";
    # Only show the message of the day on the first pageful
    print "$message_of_day\n" if $maxdate eq $nowdate;

    if (!$form{'nocrap'}) {
        my ($imageurl,$imagewidth,$imageheight,$quote) = &get_image;
        print qq{
            <table width="100%">
            <tr>
            <td valign=bottom>
            <p><center><a href=addimage.cgi><img src="$imageurl"
            width=$imagewidth height=$imageheight><br>
            $quote</a><br>
            </center>
            <p>
            <td align=right valign=bottom>
            <table><tr><td>
            <TT>L</TT> = Show Build Log<br>
            <TT><img src="star.gif">L</TT> = Show Log comments<br>
            <TT>C</TT> = Show changes that occured since the last build<br>
            <TT>B</TT> = Download binary generated by the build<br>
            <table cellspacing=2 border>
            <tr bgcolor="00ff00"><td>Built successfully
            <tr bgcolor="yellow"><td>Currently Building
            <tr bgcolor="orange"><td>Build Successful, Build Tests Failed
            <tr bgcolor="red"><td>Build failed
            </table>
            </td></tr></table>
            </table>
            };
      }
    if($bonsai_tree){
        print "The tree is currently <font size=+2>";
        if( &tree_open ){
            print "OPEN";
        }
        else {
            print "<FONT COLOR=\"#FF0000\">CLOSED</FONT>";
        }
        print "</font>\n";
    }
}

sub display_build_table {
    &display_build_table_header;
    &display_build_table_body;
    &display_build_table_footer;

}

sub display_build_table_body {
    my($t);

    # Set by display_build_table_row() for display_continue_row()
    %last_color = ();

    $t = 1;
    while( $t <= $time_count ){
        last if $build_time_times->[$t] < $mindate;
        display_build_table_row( $t );    
        $t++; 
    }
    display_continue_row($t) if $t <= $time_count;
}

sub display_build_table_row {
    my ($t) = @_;
    my ($tt, $hour_color);
    $tt = &print_time($build_time_times->[$t]);

    my ($qr) = '';
    my ($er) = '';

    if ($build_time_times->[$t] % 7200 > 3600) {
        $hour_color = "";
    } else {
        $hour_color = " bgcolor=#e7e7e7";
    }

    ($hour) = $tt =~ /(\d\d):/;
    if ($tree2 eq '' and ($lasthour != $hour or $who_check_list[$t])) {
      $qr = &query_ref( $td1, $build_time_times->[$t]);
      $er = "</a>";
    }
    if ($lasthour == $hour) {
      $tt =~ s/^.*&nbsp;//;
    } else {
      $lasthour = $hour;
    }

    # Time column
    # 
    print "<tr align=center>\n";
    print "<td align=right $hour_color>${qr}\n${tt}${er}</td>\n";

    if( $tree2 ne "" ){
        print "<td align=center bgcolor=beige>\n";
        $qr = &query_ref( $td1, $build_time_times->[$t]);
        print "${qr}</a></td>\n";
    }

    print "<td>";
    for $who (sort keys %{$who_list->[$t]} ){
        $qr = &who_menu( $td1, $build_time_times->[$t],$build_time_times->[$t-1],$who);
	$who =~ s/%.*$//;
        print "  ${qr}$who</a>\n";
    }
    print "</td>";

    if( $tree2 ne "" ){
        print "<td align=center bgcolor=beige >\n";
        $qr = &query_ref( $td2, $build_time_times->[$t]);
        print "${qr}</a></td>\n";

        print "<td align=center>\n";

        for $who (sort keys %{$who_list2->[$t]} ){
            $qr = &who_menu( $td2, $build_time_times->[$t],$build_time_times->[$t-1],$who);
            print "  ${qr}$who</a>\n";
        }
        print "</td>";
    }
    
    $bn = 1;
    while( $bn <= $name_count ){
        if( defined($br = $build_table->[$t][$bn])){
            if( $br != -1 ){

                $hasnote = $br->{hasnote};
                $noteid = $hasnote ? $br->{noteid} : 0;
                $rowspan = $br->{rowspan};
                $rowspan = $mindate_time_count - $t + 1
                  if $t + $rowspan - 1 > $mindate_time_count;
                $color = $colormap->{$br->{buildstatus}};
                $last_color{$bn} = $color;
                $status = $br->{buildstatus};
                print "<td rowspan=$rowspan bgcolor=${color}>\n";

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
                if( $hasnote ){
                    print "<a href='' onClick=\"return js_what_menu(event,$noteid,'$logfile','$errorparser','$buildname','$buildtime');\">";
                    print "<img src=star.gif border=0></a>\n";
                }

		# Build Log
		# 
		
                #print "<A HREF='http:// Build Log' ".
		$buildname = &url_encode($buildname);
		print "<A HREF='showlog.cgi?logfile=$logfile\&tree=$buildtree&errorparser=$errorparser&buildname=$buildname&buildtime=$buildtime' ".
                      "onClick=\"return log_popup(event,$bn,'$logfile','$buildtime');\">";

                print "L</a>";

		# What Changed
		#
                if( $br->{previousbuildtime} ){
		  my ($previous_br) = $build_table->[$t+$rowspan][$bn];
		  my ($previous_rowspan) = $previous_br->{rowspan};
		  if (has_who_list($t+$rowspan,
				   $t+$rowspan+$previous_rowspan-1)) {
		    $qr = &query_ref($br->{td}, 
				     $br->{previousbuildtime},
				     $br->{buildtime});
		    print "\n$qr";
		    print "C</a>";
		  }
                }

                if( $br->{binaryname} ne '' ){
                    $binfile = "$buildtree/bin/$buildtime/$br->{buildname}/$br->{binaryname}";
                    $binfile =~ s/ //g;
                    print " <a href=$binfile>B</a>";
                }
                print "</tt>\n";
		print "</td>";
            }
        }
        else  {
            print "<td></td>\n";
        }
        $bn++;
    }
    
    print "</tr>\n";
}

sub display_continue_row {
  local($t) = @_;

  print '<tr><td align=center colspan=2>';
  my $nextdate = $maxdate - $hours*60*60;
  print "<a href='showbuilds.cgi"
    ."?tree=$tree&hours=$hours&maxdate=$nextdate&nocrap=1'>"
    ."Show next $hours hours</a>";
  print '</td>';

  $bn = 1;
  while ($bn <= $name_count) {
    if (defined($br = $build_table->[$t][$bn]) and $br != -1) {
      print '<td></td>';
    } else {
      print '<td bgcolor="'.$last_color{$bn}
            .'" background="cont.gif">&nbsp</td>';
    }
    $bn++;
  }
}

sub display_build_table_header {
    my($i,$nspan);

#    print "<table border=0 bgcolor='#303030' cellspacing=0 cellpadding=0><tr valign=middle align=center><td>\n";
    print "<table border=1 bgcolor='#FFFFFF' cellspacing=1 cellpadding=1>\n";

    print "<tr align=center>\n";
    print "<td rowspan=1><font size=-1>Click time to <br>see changes <br>since time</font></td>";
    $nspan = ( $tree2 ne "" ? 4 : 1);
    print "<td colspan=$nspan><font size=-1>Click name to see what they did</font>";
    print "<br><font size=-2><a href=showbuilds.cgi" . make_cgi_args() .
        "&rebuildguilty=1>Rebuild guilty list</a></td>";
    #print "<td colspan=$name_count><font size=-1>Burning builds are busted</font>";
    #print "</tr>\n";
    
    #print "<tr>\n";
    $i = 1;
    while ($i <= $name_count){

        $bn = $build_name_names->[$i];
        $bn =~ s/Clobber/Clbr/g;
        $bn =~ s/Depend/Dep/g;
        $t = &last_good_time($i);

        if( $form{'narrow'} ){
            $bn =~ s/([^:])/$1<br>/g;
            $bn = "<tt>$bn</tt>";
        }
        else {
            $bn = "<font face='Arial,Helvetica' size=-2>$bn</font>";
        }

        if( $t->{isbusted} ){
            if ($form{'nocrap'}) {
                print "<th rowspan=2 bgcolor=FF0000>$bn</th>";
            } else {
                print "<td rowspan=2 bgcolor=000000 background=1afi003r.gif>";
                print "<font color=white>$bn</font></td>";
            }
            #print "<img src=reledanim.gif>\n";
        }
        else {
            print "<th rowspan=2 bgcolor=00ff00>$bn</th>";
        }
        $i++;
    }
    print "</tr>\n";

    print "<tr>\n";
    print "<b><TH>Build Time</th>\n";
    if( $tree2 ne "" ){
        print "<TH colspan=2>$td1->{name}</th>\n";
        print "<TH colspan=2>$td2->{name}</th>\n";
    }
    else {
        print "<TH>Guilty</th>\n";
    }
    print "</b></tr>\n";

}

sub display_build_table_footer {
    print "</table>\n";
    if (open(FOOTER, "<$data_dir/footer.html")) {
        while (<FOOTER>) {
            print $_;
        }
        close FOOTER;
    }
    print "<p><a href=admintree.cgi?tree=$tree>Administrate Tinderbox Trees</a><br>";

    print "<br><br>";
}


sub query_ref {
    my( $td, $mindate, $maxdate, $who ) = @_;
    my( $output ) = '';

    $output = "<a href=../bonsai/cvsquery.cgi?module=$td->{cvs_module}";
    $output .= "&branch=$td->{cvs_branch}" if $td->{cvs_branch} ne 'HEAD';
    $output .= "&cvsroot=$td->{cvs_root}" if $td->{cvs_root} ne $default_root;
    $output .= "&date=explicit&mindate=$mindate";
    $output .= "&maxdate=$maxdate" if $maxdate ne '';
    $output .= "&who=$who" if $who ne '';
    $output .= ">";
}

sub query_ref2 {
  my( $td, $mindate, $maxdate, $who ) = @_;
  return "../bonsai/cvsquery.cgi?module=$td->{cvs_module}&branch=$td->{cvs_branch}&cvsroot=$td->{cvs_root}&date=explicit&mindate=$mindate&maxdate=$maxdate&who=".url_encode($who);
}

sub who_menu {
    my( $td, $mindate, $maxdate, $who ) = @_;
    my $treeflag;

    #$qr = "http:// checkins for $who";
    $qr = "../registry/who.cgi?email=$e"
        ."&t0=".&url_encode("What did $who check into the source tree")
        ."&u0=".&url_encode( &query_ref2($td,$mindate,$maxdate,$who))
        ."&t1=".&url_encode("What has $who been checking in in the last day")
        ."&u1=".&url_encode( &query_ref2($td,$mindate,$maxdate,$who));

    return "<a href='$qr' onclick=\"return js_who_menu($td->{num},'$who',event,$mindate,$maxdate);\">";
}


sub tree_open {
    my($done, $line, $a, $b);
    open( BID, "<../bonsai/data/$bonsai_tree/batchid") || print "can't open batchid<br>";
    ($a,$b,$bid) = split(/ /,<BID>);
    close( BID );
    open( BATCH, "<../bonsai/data/$bonsai_tree/batch-${bid}") || print "can't open batch-${bid}<br>";;
    $done = 0;
    while( ($line = <BATCH>) && !$done ){ 
        if($line =~ /^set treeopen/) {
            chop( $line );
            ($a,$b,$treestate) = split(/ /, $line );
            $done = 1;
        }
    }
    close( BATCH );
    return $treestate;
}

sub load_javascript {

$script_str =<<'ENDJS';
<script>

if( parseInt(navigator.appVersion) < 4 ){
    window.event = 0;
}

function js_who_menu(tree,n,d,mindate,maxdate) {
    var who_link = "../registry/who.cgi?email=" + escape(n)
        + "&t0=" + escape("Last check-in")
        + "&u0=" + escape(js_qr(tree,mindate,maxdate,n))
        + "&t1=" + escape("Check-ins within 24 hours")
        + "&u1=" + escape(js_qr24(tree,n));

    if( parseInt(navigator.appVersion) < 4 ){
       document.location = who_link;
       return false;
    }

    var l = document.layers['popup'];
    l.src = who_link;
ENDJS
    #l.document.write(
    #    "<table border=1 cellspacing=1><tr><td>" + 
    #    js_qr(mindate,maxdate,n) + "What did " + n + " <b>check in to the source tree</b>?</a><br>" +
    #    js_qr24(n) +"What has " + n + " <b>been checking in over the last day</b>?</a> <br>" +
    #    "<a href=https:#endor.mcom.com/ds/dosearch/endor.mcom.com/uid%3D" +n + "%2Cou%3DPeople%2Co%3DNetscape%20Communications%20Corp.%2Cc%3DUS>" +
    #        "Who is <b>" + n + "</b> and how do <b>I wake him/her up</b></a>?<br>" +
    #    "<a href='mailto:" + n + "?subject=Whats up with...'>Send mail to <b>" + n + "</b></a><br>" +
    #    "<a href=http:#dome/locator/findUser.cgi?email="+n+">Where is <b>"+n+"'s office?</b></a>" +
    #    "</tr></table>");
    #l.document.close();

    #alert( d.y );
$script_str .=<<'ENDJS';
    l.top = d.target.y - 6;
    l.left = d.target.x - 6;
    if( l.left + l.clipWidth > window.width ){
        l.left = window.width - l.clipWidth;
    }
    l.visibility="show";
    return false;
}

function js_what_menu(d,noteid,logfile,errorparser,buildname,buildtime) {
    if( parseInt(navigator.appVersion) < 4 ){
        return true;
    }

    var l = document.layers['popup'];
    l.document.write(
        "<table border=1 cellspacing=1><tr><td>" + 
        note[noteid] + 
        "</tr></table>");
    l.document.close();

    l.top = d.y-10;
    var zz = d.x;
    if( zz + l.clip.right > window.innerWidth ){
        zz = (window.innerWidth-30) - l.clip.right;
        if( zz < 0 ){
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

</script>

<layer name="popup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

<layer name="logpopup"  onMouseOut="this.visibility='hide';" left=0 top=0 bgcolor="#ffffff" visibility="hide">
</layer>

<SCRIPT>
function log_popup(e,buildindex,logfile,buildtime)
{
    var tree = trees[buildindex];
    var buildname = build[buildindex];
    var errorparser = error[buildindex];

    var urlparams = "tree=" + tree
           + "&errorparser=" + errorparser
	   + "&buildname=" + escape(buildname)
           + "&buildtime=" + buildtime
           + "&logfile=" + logfile;
    var logurl = "showlog.cgi?" + urlparams;
    var commenturl = "addnote.cgi?" + urlparams;

    if( parseInt(navigator.appVersion) < 4 ){
      document.location = logurl;
      return false;
    }

    var q = document.layers["logpopup"];
    q.top = e.target.y - 6;

    var yy = e.target.x;
    if( yy + q.clip.right > window.innerWidth ){
        yy = (window.innerWidth-30) - q.clip.right;
        if( yy < 0 ){
            yy = 0;
        }
    }
    q.left = yy;
    q.visibility="show"; 
    q.document.write("<TABLE BORDER=1><TR><TD><B>" + 
        buildname + "</B><BR>" +
        "<A HREF=\"" + logurl + "\">View Brief Log</A><BR>" +
        "<A HREF=\"" + logurl + "&fulltext=1" + "\">View Full Log</A><BR>" +
        "<A HREF=\"" + commenturl + "\">Add a Comment</A><BR>" +
	"</TD></TR></TABLE>");
    q.document.close();
    return false;
}
ENDJS

$script_str .= "
function js_qr(tree,mindate, maxdate, who ){
    if (tree == 0 ){
        return '../bonsai/cvsquery.cgi?module=${cvs_module}&branch=${cvs_branch}&cvsroot=${cvs_root}&date=explicit&mindate=' 
            + mindate + '&maxdate=' +maxdate + '&who=' + who ;
    }";
$script_str .= "
    else {
        return '../bonsai/cvsquery.cgi?module=$td2->{cvs_module}&branch=$td2->{cvs_branch}&cvsroot=$td2->{cvs_root}&date=explicit&mindate=' 
            + mindate + '&maxdate=' +maxdate + '&who=' + who ;
    }" if $tree2 ne '';
$script_str .= "
}

function js_qr24(tree,who){
    if (tree == 0 ){
        return '../bonsai/cvsquery.cgi?module=${cvs_module}&branch=${cvs_branch}&cvsroot=${cvs_root}&date=day' 
            + '&who=' +who;
    }";
$script_str .= "
    else{
        return '../bonsai/cvsquery.cgi?module=$td2->{cvs_module}&branch=$td2->{cvs_branch}&cvsroot=$td2->{cvs_root}&date=day' 
            + '&who=' +who;
    }" if $tree2 ne '';
$script_str .= "
}
";

$ii = 0;
while( $ii < @note_array ){
  $ss = $note_array[$ii];
  while( $ii < @note_array && $note_array[$ii] eq $ss ){
    $script_str .= "note[$ii] = ";
    $ii++;
  }
  $ss =~ s/\\/\\\\/g;
  $ss =~ s/\"/\\\"/g;
  $ss =~ s/\n/\\n/g;
  $script_str .= "\"$ss\";\n";
}

$ii = 1;
while ($ii <= $name_count) {
  if (defined($br = $build_table->[1][$ii])) {
    if ($br != -1) {
      $bn = $build_name_names->[$ii];
      $script_str .= "trees[$ii]='$br->{td}{name}';\n";
      $script_str .= "build[$ii]='$bn';\n";
      $script_str .= "error[$ii]='$br->{errorparser}';\n";
    }
  }
  $ii++;
}


$script_str .= "</SCRIPT>\n";

}

sub do_express {
    my($mailtime, $buildtime, $buildname, $errorparser, $buildstatus,
       $logfile);
    my $buildrec;
    my %build;
    my %times;

    { 
      use Backwards;
    
      my ($bw) = Backwards->new("$form{tree}/build.dat") or die;

      my $latest_time=0;
      my $tooearly = 0;
      while( $_ = $bw->readline ) {
	chop;
	($mailtime, $buildtime, $buildname, $errorparser, $buildstatus, $logfile) = 
	  split( /\|/ );
	if( $buildstatus eq 'success' or $buildstatus eq 'busted' ) {
	  if ($latest_time == 0) {
	    $latest_time = $buildtime;
	  }
	  # Ignore stuff in the future.
	  next if $buildtime > $maxdate;
	  # Ignore stuff more than 12 hours old
	  if ($buildtime < $latest_time - 12*60*60) {
            # Occasionally, a build might show up with a bogus time.  So,
            # we won't judge ourselves as having hit the end until we
            # hit a full 20 lines in a row that are too early.
            if ($tooearly++ > 20) {
              last;
            }
            next;
          }
	  next if defined $build{$buildname};
	  
	  $build{$buildname} = $buildstatus;
	  $times{$buildname} = $buildtime;
	}
      }
    }

    @keys = sort keys %build;
    $keycount = @keys;
    $treename = $form{tree};
    $tm = &print_time(time);
    print "<table border=1 align=center><tr><th colspan=$keycount><a href=showbuilds.cgi?tree=$treename";
    print "&hours=$form{'hours'}" if $form{'hours'};
    print "&nocrap=1" if $form{'nocrap'};
    print ">$tree as of $tm</a></tr>"
          ."<tr>\n";
    for $buildname (@keys ){
        if( $build{$buildname} eq 'success' ){
            print "<td bgcolor=00ff00>";
        }
        else {
            if ($form{'nocrap'}) {
                print "<td bgcolor=FF0000>";
            } else {
                print "<td bgcolor=000000 background=1afi003r.gif>";
                print "<font color=white>\n";
            }
        }
        print "$buildname</td>";
    }
    print "</tr></table>\n";
}


sub loadquickparseinfo {
    my ($t, $build, $times) = (@_);
    open(BUILDLOG, "<$t/build.dat" ) || die "Bad treename $t";
    while( <BUILDLOG> ){
        chop;
        my ($mailtime, $buildtime, $buildname, $errorparser,
            $buildstatus, $logfile) = split( /\|/ );
        if( $buildstatus eq 'success' || $buildstatus eq 'busted'){
            $build->{$buildname} = $buildstatus;
            $times->{$buildname} = $buildtime;
        }
    }
    close( BUILDLOG );
    
    if( -r "$t/ignorebuilds.pl" ){
        require "$t/ignorebuilds.pl";
        foreach my $z (keys(%$ignore_builds)) {
            delete $build->{$z}; # We're supposed to ignore this
                                # build entirely.
        }
    }
    my @keys = sort keys %build;
    
    my $maxtime = 0;
    for my $buildname (@keys) {
        if ($maxtime < $times->{$buildname}) {
            $maxtime = $times->{$buildname};
        }
    }
    for my $buildname (@keys ){
        if ($times->{$buildname} < $maxtime - 12*60*60) {
            # This build is more than 12 hours old.  Ignore it.
            delete $times->{$buildname};
            delete $build->{$buildname};
        }
    }
}

sub do_quickparse {
    if ($tree eq '') {
        foreach my $t (make_tree_list()) {
            print "$t\n";
        }
        exit;
    }
    my @treelist = split(/,/, $tree);
    foreach my $t (@treelist) {
        $bonsai_tree = "";
        require "$t/treedata.pl";
        if ($bonsai_tree ne "") {
            my $state = tree_open() ? "Open" : "Close";
            print "State|$t|$bonsai_tree|$state\n";
        }
        my %build;
        my %times;
        loadquickparseinfo($t, \%build, \%times);

        for my $buildname (sort(keys %build) ){
            print "Build|$t|$buildname|$build{$buildname}\n";
        }
    }
}

sub do_rdf {
    my $mainurl = "http://$ENV{SERVER_NAME}$ENV{SCRIPT_NAME}?tree=$tree";
    my $dirurl = $mainurl;
    $dirurl =~ s@/[^/]*$@@;
    my %build;
    my %times;
    loadquickparseinfo($tree, \%build, \%times);
    my $image = "channelok.gif";
    my $imagetitle = "OK";
    for my $buildname (sort(keys %build)) {
        if ($build{$buildname} eq 'busted') {
            $image = "channelflames.gif";
            $imagetitle = "Bad";
            last;
        }
    }
    print qq{
<?xml version="1.0"?>
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
    
    $bonsai_tree = "";
    require "$tree/treedata.pl";
    if ($bonsai_tree ne "") {
        my $state = tree_open() ? "OPEN" : "CLOSED";
        print "<item><title>The tree is currently $state</title><link>$mainurl</link></item>\n";
    }

    for my $buildname (sort(keys %build)) {
        if ($build{$buildname} eq 'busted') {
            print "<item><title>$buildname is in flames</title><link>$mainurl</link></item>\n";
        }
    }
    print "</rdf:RDF>\n";
}
