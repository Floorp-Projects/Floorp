#!/usr/bin/env perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Lamm <slamm@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# make-panels.pl - Generate list of sidebar panels in RDF and entities.
#     The script writes two files, out.rdf and out.dtd.  The data to
#     generate the list is at the end of the file.  Labels in quotes are
#     marks as "Do not translate" in the dtd file.
#
# Data formats:
#
#     nc-panel|<label>|<netcenter_id>|<parent>
#     panel-group|<label>|<id>|<parent>
#     panel|<label>|<id>|<parent>|<content_url>
#

use FileHandle;

%items = ();
$items{'master-panel-list'} = { type => 'panel-group', 
                              id => 'master-panel-list', 
                              children => [] };

# Parse the panel list
#
while (<DATA>) {
  chomp;

  # Skip comments and blank lines
  next if /^\s*(?:\#)?$/;

  parse_line(\%items, $_);
}

# Output the RDF
#
print '<?xml version="1.0"?>'."\n"
     ."<!DOCTYPE RDF\n"
     ."[\n";
print_entities(\%items);
print "]>\n";

print qq(<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
  xmlns:nc="http://home.netscape.com/NC-rdf#">);
print qq(
  <rdf:Description about="urn:sidebar:master-panel-list">
    <nc:panel-list>
      <rdf:Seq>
);
for my $child (@{$items{'master-panel-list'}->{children}}) {
  print_structure($child, ' ' x 8);
}

print qq(
      </rdf:Seq>
    </nc:panel-list>
  </rdf:Description>
);

print_items(\%items);
print "</rdf:RDF>\n";

# end of main
#############################################################

sub parse_line {
  my ($items, $line) = @_;
  local $_ = $line;
  my $rec;
  my ($type, $label, $id, $parent, $url) = split /\|/;

  $label =~ s/&/&amp;/g;
  $label =~ s/>/&gt;/g;
  $label =~ s/</&lt;/g;

  warn "Warning: labels do not match for multiple entries of $id\n"
    if defined $items->{id} and $items->{id}->{label} ne $label;

  $rec = { 
    type   => $type,
    label  => $label,
    id     => $id,
    parent => $parent,
    entity => "sidebar.$type.$id",
    urn    => "urn:sidebar:$type:$id",
  };

  if (/^panel-group\|/) {
    $rec->{children} = [];
    $rec->{resource_url} = $url if defined $url;
  } elsif (/^panel\|/) {
    $rec->{content_url} = $url;
  }
  $items->{"$id"} = $rec;
  push @{$items->{$parent}->{children}}, $rec;
}

sub print_entities {
  my ($items) = @_;
  for my $id (sort keys %{$items}) {
    my $item = $items->{$id};

    print_entity($item);
  }
}

sub print_structure {
  my ($item, $depth) = @_;
  $depth = '' unless defined $depth;

  if ($item->{type} eq 'panel-group') {
    print qq($depth<rdf:li>\n);
    print qq($depth  <rdf:Description about="$item->{urn}">\n);
    if (@{$item->{children}}) {
      print qq($depth    <nc:panel-list>\n);
      print qq($depth      <rdf:Seq>\n);
      for my $child (@{$item->{children}}) {
        print_structure($child, $depth.' ' x 8);
      }
      print qq($depth      </rdf:Seq>\n);
      print qq($depth    </nc:panel-list>\n);
    }
    print qq($depth  </rdf:Description>\n);
    print qq($depth</rdf:li>\n);
  } else {
    print qq($depth<rdf:li><rdf:Description about="$item->{urn}"/></rdf:li>\n);
  }
}

sub print_items {
  my ($items) = @_;
  for my $id (sort keys %{$items}) {
    my $item = $items->{$id};
    next unless $item->{type} eq 'panel-group';
    next if $item->{label} eq '';
    print_item($item);
  }
  for my $id (sort keys %{$items}) {
    my $item = $items->{$id};
    next if $item->{type} eq 'panel-group';
    print_item($item);
  }
}

sub print_item {
  my ($item) = @_;

  my $output = "\n"
             . "  <rdf:Description about='$item->{urn}'>\n"
             . "    <nc:title>&$item->{entity};</nc:title>\n";
  if ($item->{type} eq 'panel') {
    $output .= "    <nc:content>$item->{content_url}</nc:content>\n"
  } elsif ($item->{type} eq 'nc-panel') {
    $output .= "    <nc:content>http://my.netscape.com/sidebar/wrapper.tmpl"
             .        "?service=$item->{id}</nc:content>\n"
             . "    <nc:customize>http://my.netscape.com/setup_frameset.tmpl"
             .        "?mn_yes=1&amp;services=$item->{id}</nc:customize>\n";
  }
  if (defined $item->{resource_url}) {
    $output .= "    <nc:link>$item->{resource_url}</nc:link>\n"
             . "    <nc:haslink>true</nc:haslink>\n";
  }
  $output .= "  </rdf:Description>\n";

  print $output;
}

sub print_entity {
  my ($item) = @_;

  return unless $item->{entity};

  my $entity = '';
  if ($item->{label} =~ /^"[^\"]+"$/) {
    $entity .= "<!-- LOCALIZATION NOTE $item->{entity}: DONT_TRANSLATE -->\n";
  } elsif ($item->{label} =~ /("[^\"]+")/) {
    $entity .= "<!-- LOCALIZATION NOTE $item->{entity}: Do NOT localize $1 -->\n";
  }
  $item->{label} =~ s/\"//g;

  $entity .= sprintf qq{<!ENTITY %-40s "$item->{label}">\n}, $item->{entity};

  print $entity;
}

__DATA__
panel-group|Most Popular|most-popular|master-panel-list
panel|Bookmarks|client-bookmarks|most-popular|chrome://bookmarks/content/bm-panel.xul
nc-panel|Calendar|calendar|most-popular
nc-panel|"MozillaZine"|net.254|most-popular
nc-panel|"Reuters" News|reuters|most-popular
nc-panel|Sports|sports|most-popular
nc-panel|Stocks|stocks|most-popular
panel|"Tinderbox"|tinderbox|most-popular|http://tinderbox.mozilla.org/seamonkey/panel.html
nc-panel|Weather|weather|most-popular

panel-group|Local|local|master-panel-list
nc-panel|Local Events|localevent|local
nc-panel|Local Movies|localmovie|local
nc-panel|Local News|localnews|local

panel-group|News "&" Weather|news-weather|master-panel-list
nc-panel|Business News|newsedge|news-weather
nc-panel|Local News|localnews|news-weather
nc-panel|Netscape News|netscape|news-weather
nc-panel|"Reuters" News|reuters|news-weather
nc-panel|Weather|weather|news-weather

panel-group|Sports "&" Entertainment|sports-entertainment|master-panel-list
nc-panel|"Fishcam"|fishcam|sports-entertainment
nc-panel|Horoscopes|horoscopes|sports-entertainment
nc-panel|Local Events|localevent|sports-entertainment
nc-panel|Local Movies|localmovie|sports-entertainment
nc-panel|Sports|sports|sports-entertainment

panel-group|Applications|applications|master-panel-list
nc-panel|Calendar|calendar|applications
nc-panel|Delivery|delivery|applications
nc-panel|"Netcenter" Apps|netcenterservices|applications
nc-panel|Stocks|stocks|applications
nc-panel|Travel|travel|applications
nc-panel|Web Address Book|addressbook|applications
nc-panel|Web Bookmarks|bookmarks|applications

panel-group|Search, Directories, Maps|search-directories-maps|master-panel-list
nc-panel|Maps|maps|search-directories-maps
nc-panel|Member Directory|memberdirectory|search-directories-maps
nc-panel|Search|search|search-directories-maps
nc-panel|White "&" Yellow Pages|whiteyellowpages|search-directories-maps

panel-group|"Netscape, Netcenter"|netscape-netcenter|master-panel-list
nc-panel|"Netcenter" Apps|netcenterservices|netscape-netcenter
nc-panel|"Netcenter" Today|announcements|netscape-netcenter
nc-panel|"Netscape" News|netscape|netscape-netcenter

panel-group|Others, My Netscape Network|other|master-panel-list
nc-panel|Civil Liberties News|net.281|other
nc-panel|Delaware River Fly Fishing|net.1226|other
nc-panel|Intellectual Capital|net.1096|other
nc-panel|Professional Bartender|net.240|other
nc-panel|"Red Herring"|net.229|other
nc-panel|"Sci-Fi" News|net.400|other
nc-panel|"SlashDot"|net.1683|other

panel-group|Technology|technology|other
nc-panel|"JavaWorld"|net.514|technology
nc-panel|"Tinderbox" Channel|net.467|technology

panel-group|"Mozilla"|mozilla|technology
nc-panel|"Mozilla.org"|net.404|mozilla
nc-panel|"Mozilla" News|net.418|mozilla
nc-panel|"MozillaZine"|net.254|mozilla
panel-group|"MNNN"|mnnn|other|http://dunk/mnnn.rdf
