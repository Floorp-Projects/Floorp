# -*- Mode: perl; indent-tabs-mode: nil -*-


# $Revision: 1.26 $ 
# $Date: 2003/08/17 02:14:31 $ 
# $Author: kestes%walrus.com $ 
# $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/webtools/tinderbox2/src/default_conf/BuildStatus.pm,v $ 
# $Name:  $ 



# BuildStatus.pm - the definitions of the various types of build
# results and what action should be taken with each result.  Users can
# customize their tinderbox to call pagers when certain events happen
# or change the names/colors of the build events.


# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Tinderbox build tool.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#

# complete rewrite by Ken Estes for contact info see the
#     mozilla/webtools/tinderbox2/Contact file.
# Contributor(s): 


package BuildStatus;


# This package must not use any tinderbox specific libraries.  It is
# intended to be a base class.


# for each Build status we have:

# color: to display it in on the buildpage
# handler: to execute actions each time the status is reported.
# description: to put in the legend
# header_background_gif: a gif background image for the column header
# hdml_char: a visible character to show the build state on text browsers
# order: to show us the how well this build did compared with other builds, 
#	bigger numbers means more progress

# The Tinderbox code should not depend on the set of status values in
# case we need to add more types later.

# possible new types include: unit-test-failed,
# performance-test-failed, coverage-failed, lint-failed

# The Tinderbox code only hard-codes the values of: 'not_running',
# 'building' to determine if the build in question has completed and
# 'success' to determine if the build finished all that it was
# intended to do.  The various gradations of failure are not tracked
# inside tinderbox but are useful for project management.


# If new types are added, try and keep to a small set of colors or the
# display will get confusing.  You may find it convenient to keep a
# distinction between different kinds of warnings or different kinds
# of tests but we suggest keeping all warnings and all tests get the
# same color.

# Each time a build update is sent to the tinderbox server, a handler
# function is run.  This allows the local administrator to specify an
# arbitrary action to take each time a particular status is reported.

# This handler could be used to open a trouble ticket each time the
# build fails.  There could be a new web page where developers could
# request notification (email, page) when the next build is done.
# This would allow developers to not watch the tinderbox webpage so
# intently but be informed when an interesting change has occurred.

# Please send us interesting uses for the handler.  We would like to
# make examples available.

%STATUS = (

           'not_running'=> {

                            # You may want not want distinguish from
                            # 'building', in that case change the
                            # color to yellow.

                            'html_color'=>  'white',
                            'hdml_char'=> '.',
                            'handler'=> \&main::null,
                            'description'=>  'Build in progress - status unavailable',
                            'order'=>  0,
                           },
           
           'building' => {
                           # for color blind people, we choose our colors 
                           # to be distinguishable.
                          #'html_color'=>  'yellow',
                          'html_color'=>  '#eeff00',
                          'hdml_char'=> '.',
                          'handler'=> \&main::null,
                          'description'=>  'Build in progress - status available',
                          'order'=>  1,
                         },
           
           'build_failed' => {
                        # for color blind people, we choose our colors 
                        # to be distinguishable.
                        #'html_color' => 'red',
                        'html_color' => '#ee0000',
                        'hdml_char'=> '!',

                        # People are really attached to the flame gif
                        # for the header of broken builds. If you
                        # don't want it then remove this line.

                        'header_background_gif'=> 'flames1.gif',  
                        'handler' => \&main::null,
                        'description' => 'Build failed',
                        'order' => 2
                       },

           'test_failed' => {
                             # for color blind people, we choose our colors 
                             # to be distinguishable.
                            #'html_color' => 'orange',
                            'html_color' => '#ffaa00',
                            'hdml_char'=> '~',
                            
                            # People are really attached to the flame gif
                            # for the header of broken builds. If you
                            # don't want it then remove this line.

                            'header_background_gif'=> 'flames1.gif',  
                            'handler' => \&main::null,
                            'description' => 'Build succeeded but tests failed',
                            'order' => 3,
                           },

           'success' => {
                          # for color blind people, we choose our colors 
                          # to be distinguishable.
                         #'html_color' => 'lime',
                         'html_color' => '#11dd11',
                         'hdml_char'=> '+',
                         'handler' => \&main::null,
                         'description'=> 'Build and all tests were successful',
                         'order' => 4,
                        },
          );


# This is probably very mozilla specific, but you may wish to use the
# tinderboxprint command in your build logs to print other test
# results data.


%TinderboxPrint = (

                        'A'  => {
                                 'description'=> '(Number of Allocs)',
                                 },
                         'Bl' => {
                                  'description'=> '(Bytes allocated, bloat)',
                                 },
                        'Lk' => {
                                 'description'=> '(Bytes leaked)',
                                },
                        'MH' => {
                                 'description'=> '(Bytes allocated, Max Heap)',
                                },
                        'Tp' => {
                                 'description'=> '(PageLoadTime in milliseconds)',
                                },
                        'Ts' => {
                                 'description'=> '(StartTime in milliseconds)',
                                },
                        );


sub TinderboxPrintLegend {
    my $out;

    my @keys = sort keys %TinderboxPrint;
    foreach $key (@keys) {
        my $value = $TinderboxPrint{$key}{'description'};
        
        $out .= "\t\t\t<tr><td align=center><TT>$key</TT></td>\n";
        $out .= "\t\t\t\t<td>= $value</td></tr>\n";
    }

    return $out;
}

sub set_status_handler {
    my ($buildstatus, $handler) = @_;

    $BuildStatus::STATUS{$buildstatus}{'handler'} = $handler;

    return $handler;
}

# return true if and only if the status indicates that the build is
# complete.

sub is_status_final {
  my ($buildstatus) = @_;

  if ( ($buildstatus eq 'not_running') ||
       ($buildstatus eq 'building') ) {
    return 0;
    }
    return 1;
}

# return true if and only if the status is valid (known the the
# library).

sub is_status_valid {
  my ($status) = @_;

  if ( defined ($STATUS{$status}) ) {
    return 1;
  } else {
    return 0;
  }

}

# run the status handler associated with the status given as input.

sub run_status_handler {
  my ($record) = @_;

  my ($buildstatus) = $record->{'status'};

  # run status dependent hook.
  &{$BuildStatus::STATUS{$buildstatus}{'handler'}}($record);

  # notice handlers never return any values.

  return ;
}


# return the list of all status values

sub get_all_status {
  my (@status) = keys %STATUS;

  return @status;
}


# return the states in an order sorted by $STATUS{*}{'order'}

sub get_all_sorted_status {
  my (@sorted_status) = (
                         map { $_->[0] }
                         sort{ $a->[1] <=> $b->[1] }	
                         map { [ $_, $STATUS{$_}{'order'} ] }
                         (keys %STATUS ) 
                        );

  return @sorted_status;
}


# convert a list of status strings into a list of html_colors

sub status2html_colors {
  my (@latest_status) = @_;
  my (@out);

  for ($i=0; $i <= $#latest_status; $i++) {
    my ($status) = $latest_status[$i];
    my ($out) = $STATUS{$status}{'html_color'};
    push @out, $out;
  }

  return @out;
}


# convert a list of status strings into a list of hdml_chars

sub status2hdml_chars {
  my (@latest_status) = @_;
  my (@out);

  for ($i=0; $i <= $#latest_status; $i++) {
    my ($status) = $latest_status[$i];
    my ($out) = $STATUS{$status}{'hdml_char'};
    push @out, $out;
  }

  return @out;
}

# convert a list of status strings into a list of hdml_chars

sub status2descriptions {
  my (@latest_status) = @_;
  my (@out);

  for ($i=0; $i <= $#latest_status; $i++) {
    my ($status) = $latest_status[$i];
    my ($out) = $STATUS{$status}{'description'};
    push @out, $out;
  }

  return @out;
}

sub status2handler {
  my (@latest_status) = @_;
  my (@out);

  for ($i=0; $i <= $#latest_status; $i++) {
    my ($status) = $latest_status[$i];
    my ($out) = $STATUS{$status}{'handler'};
    push @out, $out;
  }

    return @out;
}

sub status2header_background_gif {
  my (@latest_status) = @_;
  my (@out);

  for ($i=0; $i <= $#latest_status; $i++) {
    my ($status) = $latest_status[$i];
    my ($out) = $STATUS{$status}{'header_background_gif'};
    push @out, $out;
  }

  return @out;
}


1;
