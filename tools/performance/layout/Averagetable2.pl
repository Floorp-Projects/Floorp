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
# get the arguments:
#------------------------------------------------------------------------------
@ARGV;
$UrlName = $ARGV[0];
$logFile = $ARGV[1];
$NumOfSites = $ARGV[2];
$buildRoot = $ARGV[3];
$LinkURL = $ARGV[4];
$useClockTime = $ARGV[5];
#$buildIDFile = '< '.$buildRoot.'\bin\chrome\locales\en-US\navigator\locale\navigator.dtd';
#$buildIDFile = "";
debug_print( "Arguments:[ $UrlName | $logFile | $NumOfSites | $buildRoot | $LinkURL | $useClockTime]\n");

#------------------------------------------------------------------------------
# Open the ID file and get the build ID
#------------------------------------------------------------------------------

#open (XUL_FILE, $buildIDFile) or die "Cannot open BuildID file $buildIDFile (AverageTable2.pl)\n";
#$BuildNo = "";
#$LineList;
#while (<XUL_FILE>)
#{
#  $ThisLine = $_;
#  chop ($ThisLine);
#  if (/Build ID/){
#    @LineList = split (/\"/, $ThisLine);
#    $BuildNo = $LineList[1];
#  }
#}
#@LineList = split (/ /, $BuildNo);
#$BuildNo = $LineList[2];
#$BuildNo =~ s/"//g;
#$BuildNo =~ s/[>]//g;
#debug_print ("Build Number: $BuildNo\n");
#close( XUL_FILE );

#------------------------------------------------------------------------------
# Open the logfile (input)
# and the deviation file (output,append)
#------------------------------------------------------------------------------
open (LOG_FILE, "< $logFile") or die "Logfile $logFile could not be opened";

#------------------------------------------------------------------------------
# Deviation file:
# Flat file used to calculate and display deviation between latest and
# week old builds data.
#
# Format of flat file is attributes separated by commas with no spaces as follows:
# BuildNo,date,url,parsingtime,parsingper,contenttime,contentper,frametime,
# frameper,styletime,styleper,reflowtime,reflowper,totallayouttime,totallayoutper,
# totalpageloadtime
($Second, $Minute, $Hour, $DayOfMonth, $Month, $Year, $WeekDay, $DayOfYear, $IsDST) = localtime (time);
$RealMonth = $Month + 1;
$Year += 1900;
$BuildNo = $RealMonth.$DayOfMonth.$Year;
$file = $BuildNo.".dat";
open (DEVIATION, ">> $file") or die "Deviation file could not be opened";

# add entry to the deviation file
($Second, $Minute, $Hour, $DayOfMonth, $Month, $Year, $WeekDay, $DayOfYear, $IsDST) = localtime (time);
$RealMonth = $Month + 1;
$Year += 1900;
$date2 = $RealMonth.$DayOfMonth.$Year;
print (DEVIATION "$BuildNo,$date2,$UrlName,");

#------------------------------------------------------------------------------
# local variables
#------------------------------------------------------------------------------
@List;
$Content_Time = 0;
$Reflow_Time = 0;
$FrameAndStyle_Time = 0;
$Frame_Time = 0;
$Style_Time = 0;
$Parse_Time = 0;
$TotalPageLoad_Time = 0;
$TotalLayout_Time = 0;
$Avg_Content_Time = 0;
$Avg_Reflow_Time = 0;
$Avg_FrameAndStyle_Time = 0;
$Avg_Frame_Time = 0;
$Avg_Style_Time = 0;
$Avg_Parse_Time = 0;
$Avg_TotalPageLoad_Time = 0;
$Avg_TotalLayout_Time = 0;
$Content_Time_Percentage = 0;
$Reflow_Time_Percentage = 0;
$FrameAndStyle_Time_Percentage = 0;
$Frame_Time_Percentage = 0;
$Style_Time_Percentage = 0;
$Parse_Time_Percentage = 0;
$TotalLayout_Time_Percentage = 0;
$Avg_Content_Time_Percentage = 0;
$Avg_Reflow_Time_Percentage = 0;
$Avg_FrameAndStyle_Time_Percentage = 0;
$Avg_Frame_Time_Percentage = 0;
$Avg_Style_Time_Percentage = 0;
$Avg_Parse_Time_Percentage = 0;
$Avg_TotalLayout_Time_Percentage = 0;
$Num_Entries = 0;
$valid = 0;
# $WebShell;
$temp;
$url;
$Content_Flag = 0;
$Reflow_Flag = 0;
$Style_Flag = 0;
$Parse_Flag = 0;

#------------------------------------------------------------------------------
# Management of averages via average.txt file
#  NOTE: the averag.txt file is used to accumulate all performance times 
#        and keep track of the number of entries. When completed, the footer.pl 
#        script does the averaging by dividing the accumulated times by the
#        number of entries
#------------------------------------------------------------------------------

# if first site, delete any old Average file (in case the caller did not)
#
if ( $NumOfSites == 1 ){
  unlink( "Average.txt" );
  debug_print( "Deleting file Average.txt\n" );
}
# load the averages data so we can accumulate it
#
if ( -r "Average.txt" ) {
  open (AVERAGE, "< Average.txt");
  while( <AVERAGE> ){
    $ThisLine = $_;
    chop ($ThisLine);
    
    if( /Num Entries:/ ){
       @list = split( / /, $ThisLine );
      $Num_Entries = $list[2];
      debug_print( "Num Entries: $Num_Entries\n" );
    }
    if( /Avg Parse:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Parse_Time = $list[2];
      debug_print( "Avg Parse: $Avg_Parse_Time\n" );
    }  
    if( /Per Parse:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Parse_Time_Percentage = $list[2];
      debug_print( "Per Parse: $Avg_Parse_Time_Percentage\n" );
    }  
    if( /Avg Content:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Content_Time = $list[2];
      debug_print( "Avg Content: $Avg_Content_Time\n" );
    }  
    if( /Per Content:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Content_Time_Percentage = $list[2];
      debug_print( "Per Content: $Avg_Content_Time_Percentage\n" );
    }  
    if( /Avg Frame:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Frame_Time = $list[2];
      debug_print( "Avg Frame: $Avg_Frame_Time\n" );
    }  
    if( /Per Frame:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Frame_Time_Percentage = $list[2];
      debug_print( "Per Frame: $Avg_Frame_Time_Percentage\n" );
    }  
    if( /Avg Style:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Style_Time = $list[2];
      debug_print( "Avg Style: $Avg_Style_Time\n" );
    }  
    if( /Per Style:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Style_Time_Percentage = $list[2];
      debug_print( "Per Style: $Avg_Style_Time_Percentage\n" );
    }  
    if( /Avg Reflow:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Reflow_Time = $list[2];
      debug_print( "Avg Reflow: $Avg_Reflow_Time\n" );
    }  
    if( /Per Reflow:/ ){
      @list = split( / /, $ThisLine );
      $Avg_Reflow_Time_Percentage = $list[2];
      debug_print( "Per Reflow: $Avg_Reflow_Time_Percentage\n" );
    }  
    if( /Avg TotalLayout:/ ){
      @list = split( / /, $ThisLine );
      $Avg_TotalLayout_Time = $list[2];
      debug_print( "Avg TotalLayout: $Avg_TotalLayout_Time\n" );
    }  
    if( /Per Layout:/ ){
      @list = split( / /, $ThisLine );
      $Avg_TotalLayout_Time_Percentage = $list[2];
      debug_print( "Per Layout: $Avg_TotalLayout_Time_Percentage\n" );
    }  
    if( /Avg PageLoad:/ ){
      @list = split( / /, $ThisLine );
      $Avg_TotalPageLoad_Time = $list[2];
      debug_print( "Avg PageLoad: $Avg_TotalPageLoad_Time\n" );
    }  
  }
  print (AVERAGE "Avg PageLoad: $Avg_TotalPageLoad_Time\n");
  close (AVERAGE);
}

#------------------------------------------------------------------------------
# now run through the log file and process the performance data
#------------------------------------------------------------------------------
$IsValidURL = 0;
while (<LOG_FILE>)
{
  $ThisLine = $_;
  chop ($ThisLine);

  if (/Timing layout/)
  {
#    @List = split (/webshell: /, $ThisLine);
#    $WebShell = $List[1];
#    $WebShell = "(webBrowserChrome=".$WebShell;
#    $WebShell = $WebShell."):";
#    debug_print( "$WebShell\n" );

    @List = split (/'/, $ThisLine);
    $url = $List[1];
    debug_print( "(URI: $url) " );
    if( $url =~ /$LinkURL/ ){
      debug_print( "$url is the one!\n" );
      $IsValidURL = 1;
    } else {
      debug_print( "Skipping URL $url\n" );
      $IsValidURL = 0;
    }
  }
  if (/Content/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      if($useClockTime){
        @clockTimeList = split(/:/, $List[6]);
        $Content_Time += $clockTimeList[2];
      } else {
        $Content_Time += $List[9];
      }
      $Content_Flag = 1;
      debug_print( "Content Time: $Content_Time\n" );
    }
  }
  if (/Reflow/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      if($useClockTime){
        @clockTimeList = split(/:/, $List[5]);
        $Reflow_Time += $clockTimeList[2];
      } else {
        $Reflow_Time += $List[8];
      }
      $Reflow_Flag = 1;
      debug_print( "Reflow Time: $Reflow_Time\n" );
    }
  }
  if (/Frame construction plus/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      if($useClockTime){
        @clockTimeList = split(/:/, $List[9]);
        $FrameAndStyle_Time += $clockTimeList[2];
      } else {
        $FrameAndStyle_Time += $List[12];
      }
      debug_print( "Frame and Style Time: $FrameAndStyle_Time\n" );
    }
  }
  if (/Style/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      if($useClockTime){
        @clockTimeList = split(/:/, $List[6]);
        $Style_Time += $clockTimeList[2];
      } else {
        $Style_Time += $List[9];
      }
      $Style_Flag = 1;
      debug_print( "Style Time: $Style_Time\n" );
    }
  }
  if (/Parse/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      if($useClockTime){
        @clockTimeList = split(/:/, $List[5]);
        $Parse_Time += $clockTimeList[2];
      } else {
        $Parse_Time += $List[8];
      }
      $Parse_Flag = 1;
      debug_print( "Parse Time: $Parse_Time\n" );
    }
  }
  if (/Total/){
    if ($IsValidURL == 1){
      @List = split (/ /, $ThisLine);
      $temp = $List[6];
      if (
#          ($temp == $WebShell) && 
          ($Parse_Flag == 1) && 
          ($Content_Flag == 1) && 
          ($Reflow_Flag == 1) && 
          ($Style_Flag == 1)){
        $TotalPageLoad_Time = $List[12];
        debug_print( "Total Page Load_Time Time: $TotalPageLoad_Time\n" );
        $Content_Flag = 0;
        $Reflow_Flag = 0;
        $Style_Flag = 0;
        $Parse_Flag = 0;
      }
    }
  }
}

#------------------------------------------------------------------------------
# Calculate the significant time values
#------------------------------------------------------------------------------
$Frame_Time = $FrameAndStyle_Time - $Style_Time;
if($Frame_Time < 0.0){
  print( "\n***** ERROR: negative FrameTime *****\n");
  $Frame_Time = 0;
}
$TotalLayout_Time = $Content_Time + $Reflow_Time + $Frame_Time + $Style_Time + $Parse_Time;
$Avg_Time = $Avg_Time + $TotalLayoutTime + $TotalPageLoad_Time;

if( $TotalLayout_Time > 0 ){
  if ($Content_Time != 0)
  {
    $Content_Time_Percentage = ($Content_Time / $TotalLayout_Time) * 100;
  }
  if ($Reflow_Time != 0)
  {
    $Reflow_Time_Percentage = ($Reflow_Time / $TotalLayout_Time) * 100;
  }
  if ($Frame_Time != 0)
  {
    $Frame_Time_Percentage = ($Frame_Time / $TotalLayout_Time) * 100;
  }
  if ($Style_Time != 0)
  {
    $Style_Time_Percentage = ($Style_Time / $TotalLayout_Time) * 100;
  }
  if ($Parse_Time != 0)
  {
    $Parse_Time_Percentage = ($Parse_Time / $TotalLayout_Time) * 100;
  }
  if( $TotalPageLoad_Time > 0 ){
    $TotalLayout_Time_Percentage =  ($TotalLayout_Time / $TotalPageLoad_Time) * 100;
  } else {
    $TotalLayout_Time_Percentage =  100;
  }
}

#------------------------------------------------------------------------------
# Add current values to those in the averages-fields
#------------------------------------------------------------------------------
$Avg_Content_Time += $Content_Time;
$Avg_Reflow_Time += $Reflow_Time;
$Avg_Frame_Time += $Frame_Time;
$Avg_Style_Time += $Style_Time;
$Avg_Parse_Time += $Parse_Time;
$Avg_TotalPageLoad_Time += $TotalPageLoad_Time;
$Avg_TotalLayout_Time += $TotalLayout_Time;

$Avg_Content_Time_Percentage += $Content_Time_Percentage;
$Avg_Reflow_Time_Percentage += $Reflow_Time_Percentage;
$Avg_Frame_Time_Percentage += $Frame_Time_Percentage;
$Avg_Style_Time_Percentage += $Style_Time_Percentage;
$Avg_Parse_Time_Percentage += $Parse_Time_Percentage;
$Avg_TotalLayout_Time_Percentage += $TotalLayout_Time_Percentage;

$Num_Entries += 1;

#------------------------------------------------------------------------------
# Now write this site's data to the table
#------------------------------------------------------------------------------
open (TABLE_FILE, ">>table.html");

print (TABLE_FILE "<tr>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
print (TABLE_FILE "<center><a href='$LinkURL'>$UrlName</a></center>");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Parse_Time);
print (DEVIATION "$Parse_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Parse_Time_Percentage);
print (DEVIATION "$Parse_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Content_Time);
print (DEVIATION "$Content_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Content_Time_Percentage);
print (DEVIATION "$Content_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Frame_Time);
print (DEVIATION "$Frame_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Frame_Time_Percentage);
print (DEVIATION "$Frame_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Style_Time);
print (DEVIATION "$Style_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Style_Time_Percentage);
print (DEVIATION "$Style_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Reflow_Time);
print (DEVIATION "$Reflow_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$Reflow_Time_Percentage);
print (DEVIATION "$Reflow_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$TotalLayout_Time);
print (DEVIATION "$TotalLayout_Time,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFF00'>");
printf (TABLE_FILE "<center>%4.2f</center>",$TotalLayout_Time_Percentage);
print (DEVIATION "$TotalLayout_Time_Percentage,");
print (TABLE_FILE "</td>");

print (TABLE_FILE "<td BGCOLOR='#FFFFFF'>");
printf (TABLE_FILE "<center>%4.2f</center>",$TotalPageLoad_Time);
print (DEVIATION "$TotalPageLoad_Time\n");
print (TABLE_FILE "</td>");

print (TABLE_FILE "</tr>\n");

close (LOG_FILE);

open (AVERAGE, "> Average.txt");
print (AVERAGE "Num Entries: $Num_Entries\n");
print (AVERAGE "Avg Parse: $Avg_Parse_Time\n");
print (AVERAGE "Per Parse: $Avg_Parse_Time_Percentage\n");
print (AVERAGE "Avg Content: $Avg_Content_Time\n");
print (AVERAGE "Per Content: $Avg_Content_Time_Percentage\n");
print (AVERAGE "Avg Frame: $Avg_Frame_Time\n");
print (AVERAGE "Per Frame: $Avg_Frame_Time_Percentage\n");
print (AVERAGE "Avg Style: $Avg_Style_Time\n");
print (AVERAGE "Per Style: $Avg_Style_Time_Percentage\n");
print (AVERAGE "Avg Reflow: $Avg_Reflow_Time\n");
print (AVERAGE "Per Reflow: $Avg_Reflow_Time_Percentage\n");
print (AVERAGE "Avg TotalLayout: $Avg_TotalLayout_Time\n");
print (AVERAGE "Per Layout: $Avg_TotalLayout_Time_Percentage\n");
print (AVERAGE "Avg PageLoad: $Avg_TotalPageLoad_Time\n");
close (AVERAGE);
