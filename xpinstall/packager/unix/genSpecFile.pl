#!/usr/bin/perl
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Christopher Blizzard.
# Portions created by Christopher Blizzard are Copyright (C) 1998-1999
# Christopher Blizzard. All Rights Reserved.
# 
# Contributor(s): 
# Christopher Blizzard (blizzard@mozilla.org)

use strict;
use Data::Dumper;

my $package_file = '../packages-unix';
my $spec_in_file = 'mozilla.spec.in';
my $output;

my $packages = parse_package_file($package_file);

open (FILE, $spec_in_file) || die("Failed to open file " . $spec_in_file . ": $!\n");

while (<FILE>) {
  if (/PACKAGE_LISTING/)
  {
    $output .= print_package_desc($packages);
  }
  elsif (/FILE_LISTING/)
  {
    $output .= print_package_list($packages);
  }
  elsif (/PACKAGE_INSTALL/)
  {
    $output .= print_package_install($packages);
  }
  else
  {
    $output .= $_;
  }
}

print $output;

sub print_package_desc
{
  my $modules = shift;
  my $this_key;
  my $retval;

  my $default_desc = { 'browser' => 'Mozilla Browser',
		       'xpcom' => 'Mozilla XPCOM',
		       'mail' => 'Mozilla Mail/News' };
  
  foreach $this_key (keys %{$modules})
  {
    $retval .= '%package mozilla-' . $this_key . "\n";
    $retval .= 'Summary: mozilla-' . $this_key . "\n";
    $retval .= "Group: Mozilla\n";
  }

  foreach $this_key (keys %{$modules})
  {
    $retval .= '%description mozilla-' . $this_key . "\n";
    $retval .= $default_desc->{$this_key} . "\n";
  }
  return $retval;
}

sub print_package_list
{
  my $modules = shift;
  my $retval;
  my $this_key;

  foreach $this_key (keys %{$modules}) {
    my $list = $modules->{$this_key};
    my $len = scalar @{$list};
    my $i;
    $retval .= '%files mozilla-' . $this_key . "\n";
    for ($i = 0; $i < $len; $i++) {
      my $this_item = $list->[$i];
      my $dirname;
      chomp $this_item;
      if (is_component_library($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/lib/mozilla/' . $this_item . "\n";
      }
      elsif (is_component_xpt($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/lib/mozilla/' . $this_item . "\n";
      }
      elsif (is_component_js($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/lib/mozilla/' . $this_item . "\n";
      }
      elsif (is_library($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/lib/' . $this_item . "\n";
      }
      elsif (is_binary($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/bin/' . $this_item . "\n";
      }
      elsif ($dirname = is_default($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/share/mozilla/' . $this_item . "\n";
      }
      elsif ($dirname = is_chrome_wildcard($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/share/mozilla/chrome/' . $dirname . "\n";
      }
      elsif ($dirname = is_res_wildcard($this_item)) {
	$this_item = strip_bin($this_item);
	$this_item = strip_res($this_item);
	$retval .= '%{prefix}/share/mozilla/resources/' . $dirname . "/\n";
      }
      elsif (is_res($this_item)) {
	$this_item = strip_bin($this_item);
	$this_item = strip_res($this_item);
	$retval .= '%{prefix}/share/mozilla/resources/' . $this_item . "\n";
      }
      elsif ($dirname = is_res_file_in_dir($this_item)) {
	$this_item = strip_bin($this_item);
	$this_item = strip_res($this_item);
	$retval .= '%{prefix}/share/mozilla/resources/' . $this_item . "\n";
      }
      elsif ($dirname = is_dir_wildcard($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/lib/mozilla/' . $dirname . "/\n";
      }
      elsif (is_base_chrome($this_item)) {
	$this_item = strip_bin($this_item);
	$retval .= '%{prefix}/share/mozilla/' . $this_item . "\n";
      }
      elsif (is_removed($this_item)) {
	# nada.
      }
      else {
	print STDERR "Warning: print_package_list $this_item is not valid\n";
      }
    }
  }
  return $retval;
}

sub print_package_install
{
  my $modules = shift;
  my $retval;
  my $this_key;
  my $src = "dist/";
  my @list;

  $retval .= q(
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/chrome
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/html
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/entityTables
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/rdf
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/throbber
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/toolbar
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/xpinstall
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/defaults
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/defaults/pref
mkdir -p $RPM_BUILD_ROOT/%{prefix}/share/mozilla/defaults/profile
mkdir -p $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/components
mkdir -p $RPM_BUILD_ROOT/%{prefix}/include/mozilla
mkdir -p $RPM_BUILD_ROOT/%{prefix}/include/mozilla/idl
ln -s ../../share/mozilla/chrome $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/chrome
ln -s ../../share/mozilla/resources $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/resources 
ln -s ../../share/mozilla/defaults $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/defaults
mkdir -p $RPM_BUILD_ROOT/%{prefix}/lib
mkdir -p $RPM_BUILD_ROOT/%{prefix}/bin
);

  foreach $this_key (keys %{$modules}) {
    my $list = $modules->{$this_key};
    my $len = scalar @{$list};
    my $i;
    for ($i = 0; $i < $len; $i++) {
      my $this_item = $list->[$i];
      my $dirname;
      chomp $this_item;
      if (is_component_library($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/components' . "\n";
      }
      elsif (is_component_xpt($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/components' . "\n";
      }
      elsif (is_component_js($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/components' . "\n";
      }
      elsif (is_library($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/lib/' . "\n";
      }
      elsif (is_binary($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/bin/' . "\n";
      }
      elsif ($dirname = is_default($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/defaults/' . $dirname . "/" . "\n";
      }
      elsif ($dirname = is_chrome_wildcard($this_item)) {
	$retval .= 'if [ ! -d $RPM_BUILD_ROOT/%{prefix}/share/mozilla/chrome/' . $dirname . " ]; then\n\t";
	$retval .= 'mkdir $RPM_BUILD_ROOT/%{prefix}/share/mozilla/chrome/' . $dirname;
	$retval .= "\nfi\n";
	$retval .= "cp -r " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/chrome/' . $dirname . "/" . "\n";
      }
      elsif ($dirname = is_res_wildcard($this_item)) {
	$retval .= "cp -r " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/' . $dirname . "/" . "\n";
      }
      elsif (is_res($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/' . "\n";
      }
      elsif ($dirname = is_res_file_in_dir($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/resources/' . $dirname . "/" . "\n";
      }
      elsif ($dirname = is_dir_wildcard($this_item)) {
	$retval .= 'if [ -e ' . $src . $this_item . " ]; then\n\t";
	$retval .= "cp -r " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/lib/mozilla/' . $dirname . "/" . "\n";
	$retval .= "\nfi\n";
      }
      elsif (is_base_chrome($this_item)) {
	$retval .= "cp " . $src . $this_item . ' $RPM_BUILD_ROOT/%{prefix}/share/mozilla/chrome/' . "\n";
      }
      elsif (is_removed($this_item)) {
	# nada.
      }
      else {
	print STDERR "Warning: print_package_install $this_item is not valid\n";
      }
    }
  }
  return $retval;
}

sub strip_bin
{
  my $file = shift;
  $file =~ s/^bin\///;
  return $file;
}

sub strip_res
{
  my $file = shift;
  $file =~ s/^res\///;
  return $file;
}

sub is_removed
{
  my $file = shift;
  if ($file =~ /^-/) { return 1; }
  else { return 0; }
}
    

sub is_default
{
  my $file = shift;
  my $dirname;
  if (($dirname) = $file =~ /^bin\/defaults\/([a-zA-Z-\.]+)\/.*/)
  {
    return $dirname;
  }
  else
  {
    return 0;
  }
}

sub is_chrome_wildcard
{
  my $file = shift;
  my $dirname;
  if (($dirname) = $file =~ /^bin\/chrome\/([\w\/-]+)\/\*$/)
  {
    return $dirname;
  }
  else
  {
    return 0;
  }
}

sub is_dir_wildcard
{
  my $file = shift;
  my $dirname;
  if (($dirname) = $file =~ /^bin\/([a-zA-Z-\.]+)\/\*/)
  {
    return $dirname;
  }
  else 
  {
    return 0;
  }
}

sub is_res_file_in_dir
{
  my $file = shift;
  my $dirname;
  if (($dirname) = $file =~ /^bin\/res\/([a-zA-Z-\.]+)\/[a-zA-Z-\.]+$/) { return $dirname; }
  return 0;
}

sub is_res
{
  my $file = shift;
  if ($file =~ /^bin\/res\/[a-zA-Z-\.]+$/) { return 1; }
  else { return 0; }

}

sub is_res_wildcard
{
  my $file = shift;
  my $dirname;
  if (($dirname) = $file =~ /^bin\/res\/([a-zA-Z-\.]+)\/\*/)
  {
    return $dirname;
  }
  else 
  {
    return 0;
  }
}

sub is_base_chrome
{
  my $file = shift;
  if ($file =~ /^bin\/chrome\/[a-zA-Z-\.]+$/) { return 1; }
  else { return 0; }
}

sub is_component_library
{
    my $file = shift;
    if ($file =~ /components.*so$/) { return 1; }
    else { return 0; }
}

sub is_component_xpt
{
    my $file = shift;
    if ($file =~ /components.*xpt$/) { return 1; }
    else { return 0; }
}

sub is_component_js
{
    my $file = shift;
    if ($file =~ /components.*js$/) { return 1; }
    else { return 0; }
}

sub is_library
{
  my $file = shift;
  if ( $file =~ /.*so$/ ) { return 1; }
  else { return 0; }
}

sub is_binary
{
  my $file = shift;
  if ($file =~ /^bin\/[a-zA-Z-\.]+$/) { return 1; }
  else { return 0; }
}

# this function will parse the package file and return a dictionary
# that has the package names as the keys and the
# file list.
sub parse_package_file
{
  my $package_file = shift;
  my $retval = {};
  my $lineno = 0;
  my $debug = 0;
  my $component = "";

  if (!$package_file)
  {
    die("parse_package_file called without package file name.");
  }
  open (FILE, "$package_file") || die("Failed to open file " . $package_file . ": $!\n");

 LINE:
  while (<FILE>)
  {
    $lineno++;
    my $line;
    
    s/\;.*//;			# it's a comment, kill it.
    s/^\s+//;			# nuke leading whitespace
    s/\s+$//;			# nuke trailing whitespace
    
    ($debug >= 2) && print "\n";
    ($debug >= 8) && print "line $lineno:$_\n";
    
    # it's a blank line, skip it.
    /^$/	&& do {
      ($debug >= 10) && print "blank line.\n";
      next LINE;
    };
    
    # it's a new component
    /^\[/	&& do {
      ( $component ) = /^\[(.*)\]$/;
      ($debug >= 10) && print "component.\n";
      $retval->{$component} = ();
      next LINE;
    };
    
    # make sure a component is defined before doing any copies or deletes.
    ( $component eq "" ) &&
      die "Error: item $_ outside a component ($lineno).  Exiting...\n";
    
    $line = $_;

    if ($debug >= 10)
    {
      print "adding $line to $component\n";
    }
    
    push(@{$retval->{$component}}, $line);

  } # LINE
  close (FILE);
  return $retval;
}

