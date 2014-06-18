#!/usr/bin/perl
# Usage:
# PATH=$NSS_PREFIX/bin:$NSS_PREFIX/lib:$PATH ./generate.pl

use Cwd;
use File::Temp qw/ tempfile tempdir /;

use strict;

my $srcdir=getcwd();
my $db = tempdir( CLEANUP => 1 );
my $noisefile=$db."/noise";
my $passwordfile=$db."/passwordfile";
my $ca_responses=$srcdir."/ca_responses";
my $ee_responses=$srcdir."/ee_responses";

#my $db=$tmpdir;

my @base_usages=("",
                 "certSigning,crlSigning",
                 "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement,certSigning,crlSigning",
                 "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement,crlSigning");

my @ee_usages=("",
               "digitalSignature,keyEncipherment,dataEncipherment",
               "digitalSignature,nonRepudiation,keyEncipherment,dataEncipherment,keyAgreement",
               "certSigning");
my @eku_usages=("serverAuth,clientAuth,codeSigning,emailProtection,timeStamp,ocspResponder,stepUp,msTrustListSign",
                "serverAuth,clientAuth",
                "codeSigning,emailProtection",
                "timeStamp,ocspResponder,stepUp,msTrustListSign"
               );

sub dsystem{
    my @args = @_;
    system(@args) == 0
    or die "system @args failed: $?";
}

sub generate_certs(){
   for (my $i = 1; $i < scalar(@base_usages) + 1; $i++) {
     my $ca_name = "ca-$i";
     my $ca_key_usage = $base_usages[$i - 1];
     if (length($ca_key_usage) > 1) {
       $ca_key_usage = " --keyUsage $ca_key_usage,critical";
     }
     my $ca_email = "$ca_name\@example.com";
     my $ca_subject = "CN=$ca_name, E=$ca_email";
     print "key_usage=$ca_key_usage\n";
     dsystem("certutil -S -s '$ca_name' -s '$ca_subject' -t 'C,,' -x -m $i -v 120 -n '$ca_name' $ca_key_usage -Z SHA256 -2 -d $db -f $passwordfile -z $noisefile < $ca_responses");

     #and now export
     dsystem("certutil -d $db -f $passwordfile -L -n $ca_name -r -o $srcdir/$ca_name.der");

     for (my $j = 1; $j < scalar(@ee_usages) + 1; $j++) {
       ##do ee certs
       my $ee_name = "ee-$j-ca-$i";
       my $ee_key_usage = $ee_usages[$j - 1];
       if (length($ee_key_usage) > 1) {
         $ee_key_usage=" --keyUsage $ee_key_usage,critical";
       }
       my $serial = (scalar(@base_usages) + 1) * $j + $i;
       dsystem("certutil -S -n '$ee_name' -s 'CN=$ee_name' -c '$ca_name' $ee_key_usage -t 'P,,' -k rsa -g 1024 -Z SHA256 -m $serial -v 120 -d $db -f $passwordfile -z $noisefile < $ee_responses");
       #and export
       dsystem("certutil -d $db -f $passwordfile -L -n $ee_name -r -o $srcdir/$ee_name.der");
     }
     for (my $j = 1; $j < scalar(@eku_usages) + 1; $j++){
       my $ee_name = "ee-" . ($j + scalar(@ee_usages)) . "-ca-$i";
       my $eku_key_usage = $eku_usages[$j - 1];
       $eku_key_usage = " --extKeyUsage $eku_key_usage,critical";
       my $serial = 10000 + (scalar(@base_usages) + 1) * $j + $i;
       dsystem("certutil -S -n '$ee_name' -s 'CN=$ee_name' -c '$ca_name' $eku_key_usage -t 'P,,' -k rsa -g 1024 -Z SHA256 -m $serial -v 120 -d $db -f $passwordfile -z $noisefile < $ee_responses");
       #and export
       dsystem("certutil -d $db -f $passwordfile -L -n $ee_name -r -o $srcdir/$ee_name.der");

     }
   }
}


sub main(){

  ##setup
  dsystem("echo password1 > $passwordfile");
  dsystem("head --bytes 32 /dev/urandom > $noisefile");

  ##why no include this in the source dir?
# XXX: certutil cannot generate basic constraints without interactive prompts,
#      so we need to build response files to answer its questions
# XXX: certutil cannot generate AKI/SKI without interactive prompts so we just
#      skip them.
  dsystem("echo y >  $ca_responses"); # Is this a CA?
  dsystem("echo >>   $ca_responses");# Accept default path length constraint (no constraint)
  dsystem("echo y >> $ca_responses"); # Is this a critical constraint?
  dsystem("echo n >  $ee_responses"); # Is this a CA?
  dsystem("echo >>   $ee_responses"); # Accept default path length constraint (no constraint)
  dsystem("echo y >> $ee_responses"); # Is this a critical constraint?

  dsystem("certutil -d $db -N -f $passwordfile");

  generate_certs();

  print "Done\n";

}


main();
