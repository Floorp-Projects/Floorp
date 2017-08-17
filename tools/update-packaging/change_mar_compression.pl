#!/usr/bin/perl -w
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This tool extracts a mar file, changes the compression of the files contained
# by the mar file either from bzip2 to lzma or lzma to bzip2, and then recreates
# the mar file. The script determines whether the files are compressed with
# bzip2 or lzma to determine what compression should be used. The permissions
# of the files will be retained as long as the script is run on a system such
# as Linux or Mac OS X but not on Windows. The mar file will be created with the
# same MAR channel and Product version information as the original mar file.
# If a mar signature is required the converted mar files will need to be
# re-signed afterwards.
# Author: Robert Strong
#

# -----------------------------------------------------------------------------
# By default just assume that these tools exist in our path

use Getopt::Std;
use Cwd 'abs_path';
use File::Basename;

$|++;

my ($MAR, $XZ, $BZIP2, $MAR_OLD_FORMAT, $FILES, $CHANNEL, $VERSION, $REPLACE, $archive, $tmparchive, @marentries, @marfiles);

if (defined($ENV{"MAR"})) {
    $MAR = $ENV{"MAR"};
}
else {
    $MAR = "mar";
}

if (defined($ENV{"BZIP2"})) {
    $BZIP2 = $ENV{"BZIP2"};
}
else {
    $BZIP2 = "bzip2";
}

if (defined($ENV{"XZ"})) {
    $XZ = $ENV{"XZ"};
}
else {
    $XZ = "xz";
}

sub print_usage
{
    print "Usage: change_mar_compression.pl [OPTIONS] ARCHIVE\n";
	print "\n";
    print "The ARCHIVE will be recreated using either bzip2 for mar file contents that\n";
	print "are compressed with lzma or lzma for mar file contents that are compressed\n";
	print "with bzip2. The new mar file that is created will not be signed but all other\n";
	print "attributes that should be retained will be retained.\n";
	print "\n";
    print "Options:\n";
    print "  -h show this help text\n";
    print "  -r replace the original mar file with the new mar file\n";
}

my %opts;
getopts("hr", \%opts);

if (defined($opts{'h'}) || scalar(@ARGV) != 1) {
    print_usage();
    exit 1;
}

if ($opts{'r'}) {
    $REPLACE = 1;
}

$archive = $ARGV[0];
@marentries = `"$MAR" -T "$archive"`;
$? && die("Couldn't run \"$MAR\" -t");

system("$MAR -x \"$archive\"") == 0 ||
  die "Couldn't run $MAR -x";

open(my $testfilename, "updatev3.manifest") or die $!;
binmode($testfilename);
read($testfilename, my $bytes, 3);
if ($bytes eq "BZh") {
    $MAR_OLD_TO_NEW = 1;
    print "Converting mar file from bzip2 to lzma compression\n";
} else {
    undef $MAR_OLD_TO_NEW;
    print "Converting mar file from lzma to bzip2 compression\n";
}
close $testfilename;

print "\n";

# The channel is the 4th line of the output
shift @marentries;
shift @marentries;
shift @marentries;
$CHANNEL = substr($marentries[0], 24, -1);
print "MAR channel name: " . $CHANNEL . "\n";

# The version is the 5th line of the output
shift @marentries;
$VERSION = substr($marentries[0], 23, -1);
print "Product version: " . $VERSION . "\n";

# The file entries start on the 8th line of the output
shift @marentries;
shift @marentries;
shift @marentries;

print "\n";
# Decompress the extracted files
foreach (@marentries) {
    tr/\n\r//d;
    my @splits = split(/\t/,$_);
    my $file = $splits[2];

    print "Decompressing: " . $file . "\n";
    if ($MAR_OLD_TO_NEW) {
        system("mv \"$file\" \"$file.bz2\"") == 0 ||
          print "\n" && die "Couldn't mv \"$file\"";
        system("\"$BZIP2\" -d \"$file.bz2\"") == 0 ||
          print "\n" && die "Couldn't decompress \"$file\"";
    }
    else {
        system("mv \"$file\" \"$file.xz\"") == 0 ||
          print "\n" && die "Couldn't mv \"$file\"";
        system("\"$XZ\" -d \"$file.xz\"") == 0 ||
          print "\n" && die "Couldn't decompress \"$file\"";
    }
}
print "All files decompressed\n";

print "\n";
# Compress the files in the requested format
$FILES = "";
foreach (@marentries) {
    tr/\n\r//d;
    my @splits = split(/\t/,$_);
    my $mod = $splits[1];
    my $file = $splits[2];

    print "Compressing: " . $file . "\n";
    if ($MAR_OLD_TO_NEW) {
        system("\"$XZ\" --compress --x86 --lzma2 --format=xz --check=crc64 --force --stdout \"$file\" > \"$file.xz\"") == 0 ||
          die "Couldn't compress \"$file\"";
        system("mv \"$file.xz\" \"$file\"") == 0 ||
          die "Couldn't mv \"$file.xz\"";
    }
    else {
        system("\"$BZIP2\" -z9 \"$file\"") == 0 ||
          die "Couldn't compress \"$file\"";
        system("mv \"$file.bz2\" \"$file\"") == 0 ||
          die "Couldn't mv \"$file.bz2\"";
    }
    $FILES = $FILES . "\"$file\" ";
    chmod oct($mod), $file;
}
print "All files compressed\n";

my $filesuffix = ".bz";
if ($MAR_OLD_TO_NEW) {
    $filesuffix = ".xz";
}
$tmparchive = $archive . $filesuffix;

system("$MAR -H $CHANNEL -V $VERSION -c \"$tmparchive\" $FILES") == 0 ||
  die "Couldn't run $MAR -c";

if ($REPLACE) {
    print "\n";
    print "Replacing mar file with the converted mar file\n";
    unlink $archive;
    system("mv \"$tmparchive\" \"$archive\"") == 0 ||
      die "Couldn't mv \"$tmparchive\"";
}

print "\n";
print "Removing extracted files\n";
foreach (@marentries) {
    tr/\n\r//d;
    my @splits = split(/\t/,$_);
    my $file = $splits[2];

    unlink $file;
    my $dirpath = $file;
    while (1) {
        if (index($dirpath, '/') < 0) {
            last;
        }
        $dirpath = substr($dirpath, 0, rindex($dirpath, '/'));
        rmdir($dirpath);
        if (-d $dirpath) {
            last;
        }
    }
}

print "\n";
if ($MAR_OLD_TO_NEW) {
    print "Finished converting mar file from bzip2 to lzma compression\n";
} else {
    print "Finished converting mar file from lzma to bzip2 compression\n";
}
