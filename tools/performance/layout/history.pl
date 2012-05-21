##########################################################################################
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#------------------------------------------------------------------------------
sub debug_print {
  foreach $str (@_){
#    print( $str );
  }
}


#------------------------------------------------------------------------------
# Variables
#------------------------------------------------------------------------------
$Parse_Time_Max=0;
$Content_Time_Max=0;
$Frame_Time_Max=0;
$Style_Time_Max=0;
$Reflow_Time_Max=0;
$Layout_Time_Max=0;
$Total_Time_Max=0;

@RecordList;
@LineList;

#------------------------------------------------------------------------------
# Open the history file and begin by collecting the records into the data-arrays
# and set all of the max-values too
#------------------------------------------------------------------------------
$count=0;
open( HISTORY, "History.txt" ) or die "History file could not be opened\n";
while(<HISTORY>)
{
  my $PullID;
  my $BuildID;
  # - Time variables
  my $Parse_Time=0;
  my $Content_Time=0;
  my $Frame_Time=0;
  my $Style_Time=0;
  my $Reflow_Time=0;
  my $Layout_Time=0;
  my $Total_Time=0;
  # - percentage variables
  my $Parse_Per=0;
  my $Content_Per=0;
  my $Frame_Per=0;
  my $Style_Per=0;
  my $Reflow_Per=0;
  my $Layout_Per=0;

  $i=0;
  $ThisLine = $_;
  chop( $Thisline );
  @LineList = split( /,/, $ThisLine );

  # get each value into a variable
  $PullID = $LineList[$i++];
  $RecordList[$count++] = $PullID;
  debug_print( "PullID : $PullID \n" );
  $BuildID = $LineList[$i++];
  $RecordList[$count++] = $BuildID;
  debug_print( "BuildID : $BuildID \n" );

  $Parse_Time = $LineList[$i++];
  $RecordList[$count++] = $Parse_Time;
  debug_print( "Parse_Time : $Parse_Time \n" );
  $Parse_Per =  $LineList[$i++];
  $RecordList[$count++] = $Parse_Per;
  debug_print( "Parse_Per : $Parse_Per \n" );
  $Content_Time = $LineList[$i++];
  $RecordList[$count++] = $Content_Time;
  debug_print( "Content_Time : $Content_Time \n" );
  $Content_Per = $LineList[$i++];
  $RecordList[$count++] = $Content_Per;
  debug_print( "Content_Per : $Content_Per \n" );
  $Frame_Time = $LineList[$i++];
  $RecordList[$count++] = $Frame_Time;
  debug_print( "Frame_Time : $Frame_Time \n" );
  $Frame_Per = $LineList[$i++];
  $RecordList[$count++] = $Frame_Per;
  debug_print( "Frame_Per : $Frame_Per \n" );
  $Style_Time = $LineList[$i++];
  $RecordList[$count++] = $Style_Time;
  debug_print( "Style_Time : $Style_Time \n" );
  $Style_Per = $LineList[$i++];
  $RecordList[$count++] = $Style_Per;
  debug_print( "Style_Per : $Style_Per \n" );
  $Reflow_Time = $LineList[$i++];
  $RecordList[$count++] = $Reflow_Time;
  debug_print( "Reflow_Time : $Reflow_Time \n" );
  $Reflow_Per = $LineList[$i++];
  $RecordList[$count++] = $Reflow_Per;
  debug_print( "Reflow_Per : $Reflow_Per \n" );
  $Layout_Time = $LineList[$i++];
  $RecordList[$count++] = $Layout_Time;
  debug_print( "Layout_Time : $Layout_Time \n" );
  $Layout_Per = $LineList[$i++];
  $RecordList[$count++] = $Layout_Per;
  debug_print( "Layout_Per : $Layout_Per \n" );
  $Total_Time = $LineList[$i++];
  $RecordList[$count++] = $Total_Time;
  debug_print( "Total_Time : $Total_Time \n" );

  # Now check for max values
  if( $Parse_Time > $Parse_Time_Max ){
    $Parse_Time_Max = $Parse_Time;
    debug_print( "ParseTimeMax: .$Parse_Time_Max\n");
  }
  if( $Content_Time > $Content_Time_Max ){
    $Content_Time_Max = $Content_Time;
    debug_print( "Content_Time_Max: $Content_Time_Max\n");
  }
  if( $Frame_Time > $Frame_Time_Max ){
    $Frame_Time_Max = $Frame_Time;
    debug_print( "Frame_Time_Max: $Frame_Time_Max\n");
  }
  if( $Style_Time > $Style_Time_Max ){
    $Style_Time_Max = $Style_Time;
    debug_print( "Style_Time_Max: $Style_Time_Max\n");
  }
  if( $Reflow_Time > $Reflow_Time_Max ){
    $Reflow_Time_Max = $Reflow_Time;
    debug_print( "Reflow_Time_Max: $Reflow_Time_Max\n");
  }
  if( $Layout_Time > $Layout_Time_Max ){
    $Layout_Time_Max = $Layout_Time;
    debug_print( "Layout_Time_Max: $Layout_Time_Max\n");
  }

  if( $Total_Time > $Total_Time_Max ){
    $Total_Time_Max = $Total_Time;
    debug_print( "Total_Time_Max: $Total_Time_Max\n");
  }
}
close(HISTORY);

for $foo (@RecordList){
#  print( "FOO: $foo \n" );
}
ProcessHeader();
for($index=0; $index<($count/15); $index++)
{
  my $start = 15*$index;
  my $end = $start+15;
  print( "Start: $start  -> End: $end\n");
  my @entry = @RecordList[$start..$end];
  print( "Processing entry $index\n");
  ProcessEntry( @entry );
}
ProcessFooter();

#------------------------------------------------------------------------------
#
sub ProcessHeader {
  debug_print("ProcessHeader\n");
 
  ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst)=localtime;
  %weekday= (
    "1", "$day",
    '2', 'Tuesday',
    '3', 'Wednesday',
    '4', 'Thursday',
    '5', 'Friday',
    '6', 'Saturday',
    '7', 'Sunday',
  );
  $mon += 1;
  $year += 1900;

  open(TRENDTABLE, "> TrendTable.html") or die "Cannot open trend-table file (TrendTable.html) in ProcessHeader\n";
  print(TRENDTABLE "<!-- Generated by history.pl part of the Gecko PerfTools -->\n");
  print(TRENDTABLE "<BODY>\n" );
  print(TRENDTABLE "<H2 align=center><font color='maroon'>Performance History and Trending Table</font></H2><BR>\n" );
  print (TRENDTABLE "<center><font size=-1>");
  print (TRENDTABLE "$weekday{$wday} ");
  print (TRENDTABLE "$mon/$mday/$year ");
  printf (TRENDTABLE "%02d:%02d:%02d", $hour, $min, $sec);
  print (TRENDTABLE "</font></center>");
  print (TRENDTABLE "<BR>");
  print(TRENDTABLE "<!-- First the headings (static) -->\n" );
  print(TRENDTABLE "<TABLE BORDER=1 width=99% BGCOLOR='white'>\n" );
  print(TRENDTABLE "<TR >\n" );
  print(TRENDTABLE "<TD ROWSPAN=2 width=5% align=center valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Pull-ID</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=2 align=center width=5% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Build-ID</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Parsing</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Content Creation</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Frame Creation</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Style Resolution</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Reflow</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Total Layout</B>\n" );
  print(TRENDTABLE "<TD ROWSPAN=1 align=center width=10% valign=top BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<B>Total Time</B>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "</TR>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=50%>Sec</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>%</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  print(TRENDTABLE "<TD>\n" );
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center BGCOLOR='#E0E0E0'>\n" );
  print(TRENDTABLE "<TR><TD BGCOLOR='#FFFFC6' align=left width=100%>Sec</TD></TR></TABLE>\n" );
  print(TRENDTABLE "</TD>\n" );
  close(TRENDTABLE);
}

#------------------------------------------------------------------------------
#
sub ProcessEntry {

  my $PullID;
  my $BuildID;
  # - Time variables
  my $Parse_Time=0;
  my $Content_Time=0;
  my $Frame_Time=0;
  my $Style_Time=0;
  my $Reflow_Time=0;
  my $Layout_Time=0;
  my $Total_Time=0;
  # - percentage variables
  my $Parse_Per=0;
  my $Content_Per=0;
  my $Frame_Per=0;
  my $Style_Per=0;
  my $Reflow_Per=0;
  my $Layout_Per=0;
  # - weight variables
  my $Parse_Weight=0;
  my $Content_Weight=0;
  my $Frame_Weight=0;
  my $Style_Weight=0;
  my $Reflow_Weight=0;
  my $Layout_Weight=0;
  my $Total_Weight=0;

  debug_print( "Process Entry\n" );
  my @EntryLine =@_;

  open(TRENDTABLE, ">> TrendTable.html") or die "Cannot open trend-table file (TrendTable.html) in ProcessHeader\n";
  $i=0;
  $PullID = $EntryLine[$i++];
  debug_print( "PullID: $PullID \n" );
  $BuildID = $EntryLine[$i++];
  debug_print( "BuildID: $BuildID \n" );
  $Parse_Time = $EntryLine[$i++];
  debug_print( "Parse_Time : $Parse_Time \n" );
  $Parse_Per =  $EntryLine[$i++];
  debug_print( "Parse_Per : $Parse_Per \n" );
  $Content_Time = $EntryLine[$i++];
  debug_print( "Content_Time : $Content_Time \n" );
  $Content_Per = $EntryLine[$i++];
  debug_print( "Content_Per : $Content_Per \n" );
  $Frame_Time = $EntryLine[$i++];
  debug_print( "Frame_Time : $Frame_Time \n" );
  $Frame_Per = $EntryLine[$i++];
  debug_print( "Frame_Per : $Frame_Per \n" );
  $Style_Time = $EntryLine[$i++];
  debug_print( "Style_Time : $Style_Time \n" );
  $Style_Per = $EntryLine[$i++];
  debug_print( "Style_Per : $Style_Per \n" );
  $Reflow_Time = $EntryLine[$i++];
  debug_print( "Reflow_Time : $Reflow_Time \n" );
  $Reflow_Per = $EntryLine[$i++];
  debug_print( "Reflow_Per : $Reflow_Per \n" );
  $Layout_Time = $EntryLine[$i++];
  debug_print( "Layout_Time : $Layout_Time \n" );
  $Layout_Per = $EntryLine[$i++];
  debug_print( "Layout_Per : $Layout_Per \n" );
  $Total_Time = $EntryLine[$i++];
  debug_print( "Total_Time : $Total_Time \n" );

  if( $Parse_Time_Max > 0 ){
    $ParseWeight = $Parse_Time / $Parse_Time_Max * 100;
    debug_print( "ParseWeight = $ParseWeight \n" );
  }
  if( $Content_Time_Max > 0 ){
    $ContentWeight = $Content_Time / $Content_Time_Max * 100;
    debug_print( "ContentWeight = $ContentWeight \n" );
  }
  if( $Frame_Time_Max > 0 ){
    $FrameWeight = $Frame_Time / $Frame_Time_Max * 100;
    debug_print( "FrameWeight = $FrameWeight \n" );
  }
  if( $Style_Time_Max > 0 ){
    $StyleWeight = $Style_Time / $Style_Time_Max * 100;
    debug_print( "StyleWeight = $StyleWeight \n" );
  }
  if( $Reflow_Time_Max > 0 ){
    $ReflowWeight = $Reflow_Time / $Reflow_Time_Max * 100;
    debug_print( "ReflowWeight = $ReflowWeight \n" );
  }
  if( $Layout_Time_Max > 0 ){
    $LayoutWeight = $Layout_Time / $Layout_Time_Max * 100;
    debug_print( "LayoutWeight = $LayoutWeight \n" );
  }
  if( $Total_Time_Max > 0 ){
    $TotalWeight = $Total_Time / $Total_Time_Max * 100;
    debug_print( "TotalWeight = $TotalWeight \n" );
  }

  $bldID;
  @bldIDParts = split( /:/, $BuildID );
  $bldID = $bldIDParts[1];
  print(TRENDTABLE "<!-- Next entry... -->\n");
  print(TRENDTABLE "<TR> \n");
  print(TRENDTABLE "<TD ROWSPAN=1>$PullID</TD>\n");
  print(TRENDTABLE "<TD ROWSPAN=1>$bldID</TD>\n");
  print(TRENDTABLE "<!-- Parse Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f", $Parse_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>");
  printf(TRENDTABLE "%4.3f",$Parse_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$ParseWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $ParseWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Content Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f",$Content_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>");
  printf(TRENDTABLE "%4.3f",$Content_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$ContentWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $ContentWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Frames Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f",$Frame_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%> ");
  printf(TRENDTABLE "%4.3f",$Frame_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$FrameWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $FrameWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Style Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f",$Style_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>");
  printf(TRENDTABLE "%4.3f",$Style_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$StyleWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $StyleWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Reflow Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f",$Reflow_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>");
  printf(TRENDTABLE "%4.3f",$Reflow_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$ReflowWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $ReflowWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Layout Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=50%>");
  printf(TRENDTABLE "%4.3f",$Layout_Time);
  print(TRENDTABLE "</TD><TD BGCOLOR='#CFEEF7' align=right width=50%>");
  printf(TRENDTABLE "%4.3f",$Layout_Per);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$LayoutWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $LayoutWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "<!-- Parse Time -->\n");
  print(TRENDTABLE "<TD>\n");
  print(TRENDTABLE "<TABLE BORDER=0 width=100% align=center COLS=2><TR><TD BGCOLOR='#FFFFC6' align=left width=100%>");
  printf(TRENDTABLE "%4.3f",$Total_Time);
  print(TRENDTABLE "</TD></TR><TR><TD colspan=2><table WIDTH=$TotalWeight% bgcolor=blue><tr><td><font align=center size=-2>&nbsp;");
  print(TRENDTABLE "</td></font></tr></table></TD></TR></TABLE>");
  printf(TRENDTABLE "<font size=-1>%4.2f % </font>", $TotalWeight);
  print(TRENDTABLE "</TD>\n");
  print(TRENDTABLE "</TR>\n");

  close(TRENDTABLE);
}

#------------------------------------------------------------------------------
#
sub ProcessFooter {
  debug_print("ProcessHeader\n");
  open(TRENDTABLE, ">> TrendTable.html") or die "Cannot open trend-table file (TrendTable.html) in ProcessFooter\n";

  print(TRENDTABLE "</TABLE>\n");
  print(TRENDTABLE "</BODY>\n");

  close(TRENDTABLE);
} 
