# -*- Mode: perl; tab-width: 4; indent-tabs-mode: nil; -*-
#
# This file is MPL/GPL dual-licensed under the following terms:
# 
# The contents of this file are subject to the Mozilla Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and
# limitations under the License.
#
# The Original Code is PLIF 1.0.
# The Initial Developer of the Original Code is Ian Hickson.
#
# Alternatively, the contents of this file may be used under the terms
# of the GNU General Public License Version 2 or later (the "GPL"), in
# which case the provisions of the GPL are applicable instead of those
# above. If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice and
# other provisions required by the GPL. If you do not delete the
# provisions above, a recipient may use your version of this file
# under either the MPL or the GPL.

package PLIF::ProtocolHelper::UA::HTTP;
use strict;
use vars qw(@ISA);
use PLIF::Service;
@ISA = qw(PLIF::Service);
1;

# This service heuristically splits the UA strings returned by
#     $app->input->getMetaData('UA')
# ...into five components:
#     name, version, manufacturer, platform, comment
# You want to use
#     service.uaStringInterpreter.<protocol>
# ...where <protocol> is $app->input->defaultOutputProtocol.

sub provides {
    my $class = shift;
    my($service) = @_;
    return ($service eq 'service.uaStringInterpreter.http' or
            $class->SUPER::provides($service));
}

sub interpretUAString {
    my $self = shift;
    my($userAgent) = @_;
    my $name;
    my $version;
    my $manufacturer;
    my $platform;
    my $comment;
    if ($userAgent =~ m|^(Lynx)/([^ ]+) (.*)$|o) {
        # Lynx/2.7 libwww-FM/2.14
        # Lynx/2.7.1 libwww-FM/2.14
        # Lynx/2.8.1dev.4 libwww-FM/2.14FM
        # Lynx/2.8.1rel.2 libwww-FM/2.14
        # Lynx/2.8.2rel.1 libwww-FM/2.14
        $name = $1;
        $version = $2;
        $manufacturer = 'lynx-dev';
        $platform = '';
        $comment = $3;
    } elsif ($userAgent =~ m|(StarOffice)/([^;\)]+); ([^;\)]+)|o) {
        # Mozilla/3.0 (compatible; StarOffice/5.1; Linux)
        $name = $1;
        $version = $2;
        $manufacturer = 'Sun Microsystems';
        $platform = $3;
        $comment = '';
    } elsif ($userAgent =~ m|(WebTV)/([^\(]+)|o) {
        # Mozilla/3.0 WebTV/1.2 (compatible; MSIE 2.0)
        # Mozilla/3.0 WebTV/2.2 (Compatible; MSIE 2.0)
        $name = $1;
        $version = $2;
        $manufacturer = 'Microsoft';
        $platform = '';
        $comment = '';
    } elsif ($userAgent =~ m|(Netbox)/([^;]+); ([^;\)]+)|o) {
        # Mozilla/3.01 (compatible; Netbox/3.5 R92; Linux 2.2)
        $name = $1;
        $version = $2;
        $manufacturer = 'Netgem';
        $platform = $3;
        $comment = '';
    } elsif ($userAgent =~ m|^(Opera)/([^ ]+) \(([^;\)]+)[^\)]*\) *(.*[^ ])? *\[[^\]]+\]$|) {
        # Opera/4.0 (Windows 98;US) Beta 3  [en]
        # Opera/4.0 (Windows NT 4.0;US)  [en]
        # Opera/4.0 (Windows NT 4.0;US) Beta 3  [en]
        # Opera/4.0 (Windows NT 5.0;US)  [en]
        # Opera/4.03 (Windows 95; U)  [en]
        # Opera/4.03 (Windows 98; U)  [en]
        $name = $1;
        $version = $2;
        $manufacturer = 'Opera Software';
        $platform = $3;
        $comment = $4;
    } elsif ($userAgent =~ m|(Opera)/([^;]+); ([^;\)]+)[^\)]*\) *(.*[^ ])? *\[[^\]]+\]$|o) {
        # Mozilla/4.0 (compatible; Opera/3.0; Windows 95) 3.51 [en]
        # Mozilla/4.0 (compatible; Opera/3.0; Windows NT 4.0) 3.51 [en]
        $name = $1;
        if (defined($4)) {
            $version = $4;
        } else {
            $version = $2;
        }
        $manufacturer = 'Opera Software';
        $platform = $3;
        $comment = '';
    } elsif ($userAgent =~ m|    \(               # open bracket
                                      ([^;\)]+)   # first field (in $1)
                                       [^\)]*     # other fields dropped
                                 \)               # close bracket
                             \                    # space
                             (Opera)\ ([^;]*[^ ]) # Opera version
                             \ *                  # maybe some spaces
                                 \[               # open square bracket
                                       [^\]]+     # stuff inside those square brackets
                                 \]               # close square bracket
                                                $ # end of the string
                            |ox) {
        # Mozilla/4.0 (Windows 4.10;US) Opera 3.60  [en]
        # Mozilla/4.0 (Windows 95;DE) Opera 3.60  [de]
        # Mozilla/4.0 (Windows 95;US) Opera 3.60  [en]
        # Mozilla/4.0 (Windows 95;US) Opera 3.62  [en]
        # Mozilla/4.0 (Windows 95;US) Opera 4.0 Technology Preview  [en]
        # Mozilla/4.0 (Windows 98;US) Opera 3.62  [en]
        # Mozilla/4.0 (Windows NT 4.0;US) Opera 3.60  [en]
        # Mozilla/4.0 (Windows NT 4.0;US) Opera 3.61  [en]
        # Mozilla/4.71 (Windows 98;US) Opera 3.62  [en]
        # Mozilla/4.72 (Windows 98;US) Opera 4.0 Beta 1  [en]
        # Mozilla/4.72 (Windows NT 4.0;US) Opera 4.0 Beta 1  [en]
        # Mozilla/4.72 (Windows NT 5.0;US) Opera 4.0  [en]
        # Mozilla/5.0 (Windows NT 5.0; U) Opera 4.02  [en]
        $name = $2;
        $version = $3;
        $manufacturer = 'Opera Software';
        $platform = $1;
        $comment = '';
    } elsif ($userAgent =~ m|(Konqueror)/([^;]); ([^;\)])|o) {
        # Mozilla/4.0 (compatible; Konqueror/1.94 >= 20000911; X11); Supports MD5-Digest; Supports gzip encoding
        # Mozilla/4.0 (compatible; Konqueror/2.0 Release Candidate 1; X11); Supports MD5-Digest; Supports gzip encoding
        # Mozilla/4.0 (compatible; Konqueror/post 1.94 > 20000911; X11); Supports MD5-Digest; Supports gzip encoding
        $name = $1;
        $version = $2;
        $manufacturer = 'KDE';
        $platform = '';
        $comment = $3;
    } elsif ($userAgent =~ m|(NCSA Mosaic)/([^\(]) \(([^\)])\)|o) {
        # NCSA Mosaic/2.1.0 Beta (Windows x86)
        $name = $1;
        $version = $2;
        $manufacturer = 'NCSA';
        $platform = $3;
        $comment = '';
    } elsif ($userAgent =~ m|^(OmniWeb)/([^ ]+) (.*)$|o) {
        # OmniWeb/3.0-beta-8b OmniAppKit/1998G OmniNetworking/1998G OIF/1998G OWF/1998G OmniBase/1998G OmniFoundation/1998G OmniHTML/1998G
        $name = $1;
        $version = $2;
        $manufacturer = 'The Omni Group';
        $platform = 'MacOS X';
        $comment = $3;
    } elsif ($userAgent =~ m|^(iCab)/([^ ]+) \(([^;]+); [^;]+; ([^;\)]+)|o) {
        # iCab/Pre2.0 (Macintosh; I; PPC)
        $name = $1;
        $version = $2;
        $manufacturer = 'iCab Company';
        $platform = "$3 $4";
        $comment = '';
    } elsif ($userAgent =~ m|(MSIE) ([^;]+); ([^\)]+)|) {
        # Mozilla/4.0 (compatible; MSIE 4.01; AOL 4.0; Windows 98)
        # Mozilla/4.0 (compatible; MSIE 4.01; MSN 2.6; AOL 4.0; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 4.01; Mac_PowerPC)
        # Mozilla/4.0 (compatible; MSIE 4.01; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 4.01; Windows 95; Juno)
        # Mozilla/4.0 (compatible; MSIE 4.01; Windows 95; Walt Disney World Co)
        # Mozilla/4.0 (compatible; MSIE 4.01; Windows 98)
        # Mozilla/4.0 (compatible; MSIE 4.01; Windows NT)
        # Mozilla/4.0 (compatible; MSIE 4.0; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 4.5; Mac_PowerPC)
        # Mozilla/4.0 (compatible; MSIE 5.01; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 5.01; Windows 98)
        # Mozilla/4.0 (compatible; MSIE 5.01; Windows NT 5.0)
        # Mozilla/4.0 (compatible; MSIE 5.01; Windows NT)
        # Mozilla/4.0 (compatible; MSIE 5.01; Windows NT; ZDNetSL)
        # Mozilla/4.0 (compatible; MSIE 5.0; MSN 2.5; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 5.0; MSN 2.5; Windows 98; DigExt)
        # Mozilla/4.0 (compatible; MSIE 5.0; Mac_PowerPC)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 95)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 95; DigExt)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 98)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; DigExt)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; DigExt; AltaVista 1.01.01)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; QXW03301; CompuServe Deutschland IE50; DigExt; QXW03314)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows 98; formatpb; DigExt)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows NT 5.0)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows NT)
        # Mozilla/4.0 (compatible; MSIE 5.0; Windows NT; DigExt)
        # Mozilla/4.0 (compatible; MSIE 5.5; MSN 2.5; Windows 98; sunrise free surf)
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows 98)
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows 98; Win 9x 4.90)  via proxy gateway  CERN-HTTPD/3.0 libwww/2.17
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 4.0)
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 4.0; rotaract)
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 5.0)
        # Mozilla/4.0 (compatible; MSIE 5.5; Windows NT 5.0; version.dll; NetCaptor 6.01)
        $name = $1;
        $version = $2;
        $manufacturer = 'Microsoft';
        # find the platform string, which comes after the "AOL" or "MSN" strings and before the random other stuff
        my @firstFields = ();
        my @lastFields = split(/; */, $3);
        while (@lastFields > 0 and $lastFields[0] =~ m/^(?:MSN|AOL)/o) {
            # that wasn't it, store it in the second list
            push(@firstFields, shift(@lastFields));
        }
        if (@lastFields) {
            # there are still some fields left
            # the first will be the platform, so grab that
            $platform = shift(@lastFields);
        } else {
            # don't know the platform
            $platform = '';
        }
        $comment = join('; ', @firstFields, @lastFields);
    } elsif ($userAgent =~ m|  ^                  # start of the line
                                 Mozilla          # text up to the first
                               /                  # slash
                                 [^ ]+            # text up to the first
                               \                  # space
                               \(                 # a round bracket
                                 ([^\)]*)         # anything but a close bracket
                               \)                 # a close bracket
                               \                  # a space
                               Gecko/             # the gecko string
                                 ([0-9]+)         # the date portion of the gecko stamp
                               (?: \              # optionally a space
                                .* )?             # followed by the app-specific string
                              $                   # end of the line
                            |ox) {
        # Mozilla/5.0 (Macintosh; N; PPC; en-US; m18) Gecko/20001002
        # Mozilla/5.0 (Windows; U; Win98; en-US; m16) Gecko/20000503
        # Mozilla/5.0 (Windows; U; Win98; en-US; m17) Gecko/20000702
        # Mozilla/5.0 (Windows; U; Win98; en-US; m17) Gecko/20000807
        # Mozilla/5.0 (Windows; U; Win98; en-US; m17) Gecko/20000807 Netscape6/6.0b2
        # Mozilla/5.0 (Windows; U; Win98; en-US; m18) Gecko/20000816
        # Mozilla/5.0 (Windows; U; Win98; en-US; m18) Gecko/20000825
        # Mozilla/5.0 (Windows; U; Win98; en-US; m18) Gecko/20000910
        # Mozilla/5.0 (Windows; U; Win98; en-US; m18) Gecko/20001002
        # Mozilla/5.0 (Windows; U; WinNT4.0; en-US; m16) Gecko/20000613
        # Mozilla/5.0 (Windows; U; WinNT4.0; en-US; m17) Gecko/20000712
        # Mozilla/5.0 (Windows; U; WinNT4.0; en-US; m17) Gecko/20000725
        # Mozilla/5.0 (Windows; U; WinNT4.0; en-US; m18) Gecko/20000809
        # Mozilla/5.0 (Windows; U; WinNT4.0; en-US; m18) Gecko/20000813
        # Mozilla/5.0 (Windows; U; Windows NT 5.0; en-US; m18) Gecko/20000804
        # Mozilla/5.0 (X11; U; Linux 2.2.12-20 i586; en-US; m16) Gecko/20000617
        # Mozilla/5.0 (X11; U; Linux 2.2.14 i586; en-US; m16) Gecko/20000606
        # Mozilla/5.0 (X11; U; Linux 2.2.14 i586; en-US; m18) Gecko/20000927
        # Mozilla/5.0 (X11; U; Linux 2.2.14-5.0 i686; en-US; m16) Gecko/20000421
        # Mozilla/5.0 (X11; U; Linux 2.2.14-5.0 i686; en-US; m16) Gecko/20000605
        # Mozilla/5.0 (X11; U; Linux 2.2.14-5.0 i686; en-US; m16) Gecko/20000608
        # Mozilla/5.0 (X11; U; Linux 2.2.5-15 i686; en-US; m16) Gecko/20000613
        # Mozilla/5.0 (X11; U; Linux 2.4.0-test1-ac10 i686; en-US; m17) Gecko/20000718
        # Mozilla/5.0 (X11; U; Linux 2.4.0-test3 i686; en-US; m18) Gecko/20000918
        # Mozilla/5.0 (X11; U; Linux 2.4.0-test7 i686; en-US; m18) Gecko/20000928
        # Mozilla/5.0 (X11; U; Linux 2.4.0-test8 i686; en-US; m18) Gecko/20000927
        my @fields = split(/; */, $1);
        my $gecko = $2;
        $manufacturer = '';
        $version = '';
        if (@fields >= 3) {
            $platform = "$fields[0]; $fields[2]";
        } elsif (@fields) {
            $platform = $fields[0];
        }
        $comment = "Gecko build ID: $gecko";
        if ($3) {
            if ($3 =~ m|^([^/]*)/(.*)$|o) {
                $name = $1;
                $version = $2;
                if ($1 =~ /^Netscape/o) {
                    $manufacturer = 'Netscape';
                }
            } else {
                $name = '';
            }
        } else {
            $name = 'Mozilla';
            $manufacturer = 'mozilla.org';
            if (@fields >= 5) {
                if ($fields[4] =~ m/^(.*)\+$/o) {
                    $version = "Build $gecko";
                    $comment = $fields[4];
                } else {
                    $version = $fields[4];
                }
            }
        }
    } elsif ($userAgent =~ m|  ^                  # start of the line
                                 Mozilla          # text up to the first
                               /                  # slash
                                 ([^ ])([^ ]*)    # text up to the first
                               \                  # space
                          (?:  \[                 # optionally: a sqare bracket
                                  [^\]]*          #             anything but a square bracket
                               \]                 #             a close bracket
                                 ([^\(]*)         #             anthing but an open bracket
                               \                  #             a space
                           )?                     #
                               \(                 # a round bracket
                                 ([^\)]*)         # anything but a close bracket
                               \)                 # a close bracket
                              $                   # end of the line
                            |ox) {
        # Mozilla/3.0 (Win95; I)
        # Mozilla/3.01C-KIT  (Win95; U)
        # Mozilla/3.01Gold (Win95; I)
        # Mozilla/4.04 (Macintosh; I; PPC)
        # Mozilla/4.05 [en] (X11; I; OSF1 V4.0 alpha; Nav)
        # Mozilla/4.06 [en] (Win98; I)
        # Mozilla/4.08 (Macintosh; U; PPC, Nav)
        # Mozilla/4.08 [en] (Win98; I ;Nav)
        # Mozilla/4.5 [en] (Win95; I)
        # Mozilla/4.5 [en] (WinNT; I)
        # Mozilla/4.5 [en] (X11; I; Linux 2.0.36 i586)
        # Mozilla/4.5 [en] (X11; U; SunOS 5.5.1 sun4u)
        # Mozilla/4.5 [en]C-CCK-MCD {U S WEST.net}  (Win98; U)
        # Mozilla/4.51 [en] (Win95; I)
        # Mozilla/4.6 (Macintosh; I; PPC)
        # Mozilla/4.6 [en] (WinNT; I)
        # Mozilla/4.6 [en] (WinNT; U)
        # Mozilla/4.6 [en] (X11; I; Linux 2.0.36 i686)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.10 i686)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.11 i686)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.5 i686)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.5-15 i686)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.6-16apmac ppc)
        # Mozilla/4.6 [en] (X11; I; Linux 2.2.9 i586)
        # Mozilla/4.6 [en] (X11; U; Linux 2.0.34 i686)
        # Mozilla/4.6 [en]C-NECCK  (Win98; I)
        # Mozilla/4.61 (Macintosh; I; PPC)
        # Mozilla/4.61 [en] (Win32; Escape 4.0; I)
        # Mozilla/4.61 [en] (Win95; I)
        # Mozilla/4.61 [en] (Win98; I)
        # Mozilla/4.61 [en] (WinNT; I)
        # Mozilla/4.61 [en] (WinNT; U)
        # Mozilla/4.61 [en] (X11; I; Linux 2.2.11 i586)
        # Mozilla/4.61 [en] (X11; I; Linux 2.2.13-7mdk i586)
        # Mozilla/4.7 (Macintosh; I; PPC)
        # Mozilla/4.7 [en] (Win95; I)
        # Mozilla/4.7 [en] (Win98; I)
        # Mozilla/4.7 [en] (WinNT; I)
        # Mozilla/4.7 [en] (X11; I; Linux 2.2.12 i686)
        # Mozilla/4.7 [en] (X11; I; Linux 2.2.12-20 i686)
        # Mozilla/4.7 [en] (X11; I; Linux 2.2.5-15 i686)
        # Mozilla/4.72 [de] (X11; I; Linux 2.2.14 i686)
        # Mozilla/4.72 [en] (Win98;I)
        # Mozilla/4.73 [en] (Win95; U)
        # Mozilla/4.73 [en] (WinNT; U)
        # Mozilla/4.74 [en] (Win98; U)
        # Mozilla/4.74 [en] (WinNT; U)
        # Mozilla/4.75 [en] (Win98; U)
        # Mozilla/4.75 [en] (X11; U; Linux 2.2.16-3 i686)
        if ($1 eq '4') {
            $name = 'Netscape Communicator';
        } elsif ($1 eq '3') {
            $name = 'Netscape Navigator';
        } else {
            $name = 'Netscape';
        }
        $version = "$1$2";
        $manufacturer = 'Netscape';
        my $platform = '';
        my @inputFields = split(/; */, $4);
        my @outputFields = ();
        foreach my $field (@inputFields) {
            if ($field !~ m/^X11$/o and $field !~ m/^[NIU]$/o) {
                if (defined($platform)) {
                    push(@outputFields, $field);
                } else {
                    $platform = $field;
                }
            }
        }
        $comment = $3;
        if (@outputFields) {
            local $" = '; ';
            $comment = "$3; @outputFields";
        }
    } elsif ($userAgent =~ m|^([^/]+)/([^ ]+) (.*)$|ox) {
        $name = $1;
        $version = $2;
        $manufacturer = '';
        $platform = '';
        $comment = $3;
    }
    return ($name, $version, $manufacturer, $platform, $comment);
}

