#!/usr/bin/perl

use Getopt::Long;

$win32 = 0;
for ($^O) {
    if (/((MS)?win32)|cygwin/i) {
        $win32 = 1;
    }
}
if ($win32) {
    $moz = "$ENV{'MOZ_SRC'}/mozilla";
}
else {
    $moz = "~/mozilla";
}

$help = 0;
$verbose = 0;
$bigendian = 0;
$mbin = '';
$midl = '';
GetOptions('moz=s' => \$moz,
           'help' => \$help,
           'bigendian' => \$bigendian,
           'verbose' => \$verbose,
           'bin=s' => \$mbin,
           'idl=s' => \$midl);

if ($help)
{
    print STDERR "find_iids usage.\n\n",
                 "This utility slurps up all the IIDs specified in the .idl files and counts\n",
                 "occurences in the compiled binaries. If the compiler/linker is working\n",
                 "there should be a maximum of 1 instance of each IID per binary file!\n\n",
                 "  -help       Show this help\n",
                 "  -verbose    Print more information\n",
                 "  -bigendian  For Big Endian architectures, e.g. PowerPC\n",
                 "  -moz <path> Specify the path to mozilla\n",
                 "  -bin <path> Specify the path to the binaries\n",
                 "  -idl <path> Specify the path to the idl files\n",
                 "\n",
                 "Use -bin & -idl instead of -moz when you are attempting to analyze binaries that live outside the\n",
                 "Mozilla build dir\n";
    exit 1;
}

if ($midl =~ "") {
    $moz_idl = "$moz/dist/idl";
}
else {
    $moz_idl = $midl;
}
if ($mbin =~ "") {
    $moz_bin = "$moz/dist/bin";
}
else {
    $moz_bin = $mbin;
}

print "Scanning $moz_idl\n" if ($verbose);

@uuidList = {};

%interfaces = ();

opendir(IDLDIR, $moz_idl) or die("Error: can't open $moz_idl\n");
@idlList = readdir(IDLDIR);
closedir(IDLDIR);

chdir($moz_idl);
foreach $idl (@idlList) {
    if (-f $idl) {
        print "$idl\n" if ($verbose);
        open(IDL, "<$idl") or die ("Error: cannot open $idl\n");
        $in_comment = 0;
        while (<IDL>) {
            chomp;
            next if (/\/\/$/);
            if (/\/\*/) {
                $in_comment++;
            }
            elsif (/\*\//) {
                $in_comment--;
            }
            elsif ($in_comment > 0) {
                next;
            }
            elsif (/uuid\(([^\)]*)\)/) {
                $uuid = $1;
            }
            elsif (/interface[\s]+([\w]*)/) {
                my($uuid_regex) = bin_uuid_regex($uuid);
                if ("$uuid_regex" !~ "error") {
                    print " Adding $1 $uuid \n" if ($verbose);
                    $interfaces{$1} = $uuid_regex;
                }
            }
        }
        close(IDL);
    }
}

scan_dir("$moz_bin");
scan_dir("$moz_bin/components");

exit 0;

sub scan_dir($)
{
    my ($dir) = @_;
    my (@dlls);
    chdir($dir);
    opendir(DLLDIR, $dir);
    @dlls = readdir(DLLDIR);
    closedir(DLLDIR);
    
    foreach $dll (@dlls) {
        # Only .dll, .exe, .so, .dylib & executable files
        if (-f $dll && (-x $dll || $dll =~ /\.(dll|exe|so|dylib)$/i)) {
            print "Scanning $dll...\n" if ($verbose);
            while (my($iface, $uuid_regex) = each %interfaces) {
                # print("$iface, $uuid_regex\n");
                $count = 0;
                open(DLL, "<$dll") or die ("Error: cannot open $dll\n");
                binmode(DLL);
                while (<DLL>) {
                    # Turn uuid into binary expression
                    if (/$uuid_regex/g) {
                        $count++;
                    }
                }
                close(DLL);
                if ($count > 0) {
                    print "  Interface $iface found $count times in $dll\n";
                }
            }
        }
    }
}

# An iid defined like this:
#
#   11223344-4455-7788-99aa-bbccddeeff00
#
# Appears in memory like this on x86 architectures
#
#   \x440\x33\x22\x11\x66\x55\x88\x77\x99\xaa\xbb\xcc\xdd\xee\xff\x00
sub bin_uuid_regex($)
{
    my ($uuid) = @_;
    my ($uuid_regex);
    
    # Check formatting and hex-ness of digits
    
    if ($uuid !~ /([0-9a-f]{8})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{4})-([0-9a-f]{12})/i) {
        print "error\n";
        return "error";
    }
      
    $pt1 = $1;
    $pt2 = $2;
    $pt3 = $3;
    $pt4 = $4;
    $pt5 = $5;

    # Flip the search pattern bytes around as they appear in memory
    # Note use \w now instead of [0-9a-f] since we know they are hex chars
    # because of the previous test.

    if ($bigendian) {
        # non x86 platforms
        $pt1 =~ s/(\w{2})(\w{2})(\w{2})(\w{2})/\\x$1\\x$2\\x$3\\x$4/g;
        $pt2 =~ s/(\w{2})(\w{2})/\\x$1\\x$2/g;
        $pt3 =~ s/(\w{2})(\w{2})/\\x$1\\x$2/g;
        $pt4 =~ s/(\w{2})(\w{2})/\\x$1\\x$2/g;
        $pt5 =~ s/(\w{2})(\w{2})(\w{2})(\w{2})(\w{2})(\w{2})/\\x$1\\x$2\\x$3\\x$4\\x$5\\x$6/g;
    }
    else {
        $pt1 =~ s/(\w{2})(\w{2})(\w{2})(\w{2})/\\x$4\\x$3\\x$2\\x$1/g;
        $pt2 =~ s/(\w{2})(\w{2})/\\x$2\\x$1/g;
        $pt3 =~ s/(\w{2})(\w{2})/\\x$2\\x$1/g;
        $pt4 =~ s/(\w{2})(\w{2})/\\x$1\\x$2/g;
        $pt5 =~ s/(\w{2})(\w{2})(\w{2})(\w{2})(\w{2})(\w{2})/\\x$1\\x$2\\x$3\\x$4\\x$5\\x$6/g;
    }

    $uuid_regex = "$pt1$pt2$pt3$pt4$pt5";
    return $uuid_regex;
}
