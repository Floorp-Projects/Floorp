package Tinderbox3::Util;

use strict;

require Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(escape_html escape_js escape_url);

sub escape_html {
  my ($str) = @_;
  $str =~ s/>/&gt;/g;
  $str =~ s/</&lt;/g;
  $str =~ s/'/&apos;/g;
  $str =~ s/"/&quot;/g;
  die if $str =~ /\n/;
  return $str;
}

sub escape_js {
  my ($str) = @_;
  $str =~ s/(['"\\])/\\$1/g;
  $str =~ s/(\r?)\n/\\n/g;
  return $str;
}

sub escape_url {
  my ($str) = @_;
  $str =~ s/ /+/g;
  $str =~ s/([%&])/sprintf('%%%x', ord($1))/eg;
  return $str;
}


1
