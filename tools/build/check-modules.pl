#!/usr/bin/env perl

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
# The Original Code is this file as it was released upon February 25, 1999.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1999
# Netscape Communications Corporation. All Rights Reserved.

#
# check-modules.pl - Check cvs modules file for duplicates and syntax errors.
#
# TODO:
#  - Parse output of 'cvs co -c' command in addition to the raw file.
#
# Send comments, improvements, bugs to Steve Lamm (slamm@netscape.com).

# $Id: check-modules.pl,v 1.1 1999/03/25 17:16:20 slamm%netscape.com Exp $

require 5.004;

use Getopt::Std;

sub usage
{
  my ($progname) = $0 =~ /([^\/]+)$/;
  die "Usage: $progname [options] [<module_file>]
   Reads from stdin if no file is given.
   Options:
      -v    Verbose. Print the modules and what they include.
      -h    Print this usage message.
";
}

&usage if !getopts('hv');
&usage if defined($opt_h);

######################################################################
# begin main


# The subroutine &parse_input creates the globals @module_names,
#    %module_tree, and %line_number (described below).
&parse_input;

foreach $module (@module_names)
{
  &check_module($module);
}


# end main
######################################################################
# begin subroutines

sub parse_input
{
  # Globals created:
  #      @module_names - List of module names in the order they are seen.
  #      %module_tree - Hash table of lists.  Keys are module names. 
  #                     Values are lists of module names and diretories.
  #      %line_number - Hash indexed by module name and module item.  
  #                     Values are line numbers.

  @module_names = ();
  %module_tree = ();
  %line_number = ();

  while (<>) 
  {
    next if /^\#/ || /^\s*$/;

    # Check for a module definition
    if (/^([a-zA-Z]+)\s+-a\s*(.*)$/) 
    {
      my ($module_name) = $1;
      my (@sub_items) = ();
      my ($line) = $2;

      push @module_names, $module_name;

      # Read line continuations (i.e. lines with '\' on the end).
      while ($line =~ /\\$/)
      {
	chomp $line;
	$line =~ s/^\s*(.*?)\s*\\$/$1/;
        if (length($line) > 0)
        {
          my (@line_items) = split(/\s+/, $line);
          push @sub_items, @line_items;
          &save_line_numbers($module_name, $., @line_items);
        }
	$line = <>;
      }
      chomp $line;
      $line =~ s/^\s*(.*?)\s*$/$1/;
      my (@line_items) = split(/\s+/, $line);
      push @sub_items, @line_items;
      &save_line_numbers($module_name, $., @line_items);

      $module_tree{$module_name} = \@sub_items;
    }
    else
    {
      die "Unexpected input: line $.: $_\n";
    }
  }
}

sub check_module
{
  my ($module) = $_[0];
  my ($sub_module, $sub_dir, $prev_module);

  # Globals created:
  #      %have_checked - List of modules already checked. 
  #      %full_list    - All the directories for a module.
  #                      Indexed by module and sub directory.
  #                      Values are the module that added the directory.

  return if defined($have_checked{$module});

  $full_list{$module} = {};

  foreach $sub_module ( &get_modules($module) )
  {
    &check_module($sub_module);

    # Add the directories of the sub_module to this module
    while (($sub_dir, $prev_module) = each %{$full_list{$sub_module}})
    {
      $full_list{$module}{$sub_dir} = $prev_module;
    }
  }
  foreach $sub_dir ( &get_directories($module) )
  {
    if (defined($full_list{$module}{$sub_dir}))
    {
      my ($previous_module) = $full_list{$module}{$sub_dir};
      
      &warn_multiple($sub_dir, $module, $previous_module);
    }
    else
    {
      $full_list{$module}{$sub_dir} = $module;

      # Check if parent or child of directory was previously added
      #
      &check_inclusion($sub_dir, $module);
    }
  }
  if (defined($opt_v))
  {
    print "$module\n";
    while (($sub_dir, $prev_module) = each %{$full_list{$module}})
    {
      print "  $sub_dir, $prev_module\n";
    }
  }
  $have_checked{$module} = 1;
}

sub get_modules
{
  my ($module) = $_[0];
  my (@output) = ();
  my ($sub_item);

  foreach $sub_item ( @{ $module_tree{$module} } )
  {
    push @output, $sub_item if defined($module_tree{$sub_item});
  }
  return @output;
}

sub get_directories
{
  my ($module) = $_[0];
  my (@output) = ();
  my ($sub_item);

  foreach $sub_item ( @{ $module_tree{$module} } )
  {
    push @output, $sub_item unless defined($module_tree{$sub_item});
  }
  return @output;
}

sub save_line_numbers
{
  my ($module, $line_num, @sub_items) = @_;
  my ($sub_item);
  
  foreach $sub_item (@sub_items)
  {
    if (defined($line_number{$module}{$sub_item}))
    {
      $line_number{$module}{$sub_item} = 
        "$line_number{$module}{$sub_item}, $line_num";
    }
    else
    {
      $line_number{$module}{$sub_item} = $line_num;
    }
  }
}

sub warn_multiple
{
  my ($sub_item, $module, $previous_module) = @_;
  my ($line_txt) = '';

  my (@lines) = split(', ', $line_number{$module}{$sub_item});
  
  push(@lines, split(', ', $line_number{$previous_module}{$sub_item}))
    unless $previous_module eq $module;

  $line_txt = "lines ".join(', ', sort { $a <=> $b } @lines);

  warn "Error: Multiple listing: $line_txt: $sub_item.\n";
}

sub check_inclusion
{
  my ($sub_dir, $module) = @_;
  my ($dir);

  foreach $dir (keys %{$full_list{$module}})
  {
    next if $dir eq $sub_dir;
    if (length($dir) < length($sub_dir))
    {
      my ($temp) = $sub_dir;
      $sub_dir = $dir;
      $dir = $temp;
    }
    if ($dir =~ /^$sub_dir\//)
    {
      warn "Warning: $dir (line "
      .$line_number{$full_list{$module}{$dir}}{$dir}
      .") pulled by $sub_dir (line "
      .$line_number{$full_list{$module}{$sub_dir}}{$sub_dir}
      .")\n";
    }
  }
}
