#!/usr/bin/perl
# -*- Mode: Perl; tab-width: 4; indent-tabs-mode: nil; -*-
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
# The Original Code is Mozilla Automated Testing Code.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Bob Clary <bob@bclary.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

use File::Basename;
use File::Copy;
use Cwd;

#my $DEBUG = 0;
#editTalkback("/tmp/test/firefox-1.5-nightly-opt/firefox/firefox", "talkbackid");

1;

sub editTalkback
{
    my ($binary, $talkbackid) = @_;
    my $rc;
    my $signal;
    my $home = $ENV{HOME};
    my $os = undef;
    my $ostype = "";

    # prevent replacements due to & in urls
    $talkbackid =~ s/&/\\&/g;
    
    # hack around lack of available environment entries in both
    # cygwin perl and redhat perl
    open OSTYPE, "set | grep OSTYPE |" || die "Unable to open OSTYPE: $!";
    while (<OSTYPE>)
    {
        chomp;
        $ostype .= $_;
    }
    close OSTYPE;

    die "Unable to determine OSTYPE" if (!$ostype);

    if ($ostype =~ /cygwin/i)
    {
        $os = "nt";
    }
    elsif ($ostype =~ /linux/i)
    {
        $os = "linux";
    }
    elsif ($ostype =~ /darwin/)
    {
        $os = "darwin";
    }
    else
    {
        die "Unknown Operating System: $ostype";
    }

    if (!$binary)
    {
        die "binary argument missing";
    }

    if (! -f $binary)
    {
        die "$binary does not exist";
    }

    my $installpath = dirname($binary);

    #print "installpath=$installpath\n";

    if (! -d $installpath)
    {
        die "$installpath does not exist";
    }

#
# edit talkback to automatically submit
#
    my $talkback=1;

    if ( -e "$installpath/extensions/talkback\@mozilla.org/components/master.ini")
    {
        chdir "$installpath/extensions/talkback\@mozilla.org/components/";
    }
    elsif ( -e "$installpath/extensions/talkback\@mozilla.org/components/talkback/master.ini")
    {
        chdir "$installpath/extensions/talkback\@mozilla.org/components/talkback/";
    }
    elsif ( -e "$installpath/components/master.ini")
    {
        chdir "$installpath/components";
    }
    else
    { 
        #print "Talkback not installed.\n";
        $talkback=0 ;
    }

    if ( $talkback == 1 )
    {
        # edit to automatically send talkback incidents
        if ( -e "master.sed")
        {
            #print "talkback master.ini already edited\n";
        }
        else
        {
            #print "editing talkback master.ini in " . cwd() . "\n";
            copy("/work/mozilla/mozilla.com/test.mozilla.com/www/bin/master.sed", "master.sed");
            system(("sed", "-ibak", "-f", "master.sed", "master.ini"));

            $rc = $? >> 8;
            $signal = $? & 127;
            $dumped = $? & 128;
            if ($rc != 0)
            {
                die "non zero exitcode editing master.ini: $!";
            }
            if ($signal)
            {
                #print "signal $signal editing master.ini\n";
            }
        }

        open MASTER, "master.ini" || die "unable to open master.ini: $!";
        while (<MASTER>)
        {
            chomp $_;
            if (/VendorID = "([^"]*)"/)
	    {
		($vendorid) = $1;
	    }
	    elsif (/ProductID = "([^"]*)"/)
                              {
                                  ($productid) = $1;
                              }
                              elsif (/PlatformID = "([^"]*)"/)
	    {
		($platformid) = $1;
	    }
	    elsif (/BuildID = "([^"]*)"/)
                            {
                                ($buildid) = $1;
                            }
                        }
        close MASTER;

        if ("$DEBUG")
        {
            print "vendorid=$vendorid\n";
            print "productid=$productid\n";
            print "platformid=$platformid\n";
            print "buildid=$buildid\n";
        }

        my $appdata;

        if ($os eq "nt")
        {
            # get application data directory in windows format
            $appdata = $ENV{APPDATA};
            if (!$appdata)
            {
                die "Empty Windows Application Data directory\n";
            }

            open PATH, "cygpath -d \"$appdata\"|" || die "Unable to open cygpath: $!";
            my $path = "";
            while (<PATH>)
            {
                chomp;
                $path .= $_;
            }
            close PATH;
            $path =~ s/\\/\\\\/g;

            if (!$path)
            {
                die "Unable to convert Windows Application Data directory to short format\n";
            }

            # convert application data directory to unix format
            $appdata = "";
            open PATH, "cygpath -u $path|" || die "unable to open cygpath: $!";
            while (<PATH>)
            {
                chomp;
                $appdata .= $_;
            }
            close PATH;
            if (!$appdata)
            {
                die "Unix format Windows Application Data directory is empty\n";
            }
            $talkbackdir = "$appdata/Talkback";
        }
        elsif ($os eq "linux")
        {
            $talkbackdir="$home/.fullcircle";
        }
        elsif ($os eq "darwin")
        {
            $talkbackdir="$home/Library/Application\ Support/FullCircle";
        }
        else
        {
            die "unknown os $os";
        }


        if ( ! -e "$talkbackdir" )
        {
            if (! mkdir "$talkbackdir", 755)
            {
                die "unable to create $talkbackdir\n: $!";
            }
        }

        my $talkbackinidir;

        if ($os eq "nt")
        {
            $talkbackinidir="$talkbackdir/$vendorid/$productid/$platformid/$buildid/";

            if ( ! -e "$talkbackdir/$vendorid" )
            {
                if (! mkdir "$talkbackdir/$vendorid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid: $!";
                }
            }

            if (! -e "$talkbackdir/$vendorid/$productid")
            {
                if (! mkdir "$talkbackdir/$vendorid/$productid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid/$productid: $!";
                }
            }


            if (! -e "$talkbackdir/$vendorid")
            {
                if (! mkdir "$talkbackdir/$vendorid/$productid/$platformid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid/$productid/$platformid: $!";
                }
            }


            if (! -e "$talkbackdir/$vendorid/$productid/$platformid")
            {
                if (! mkdir "$talkbackdir/$vendorid/$productid/$platformid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid/$productid/$platformid: $!";
                }
            }

            if ( ! -e "$talkbackdir/$vendorid/$productid/$platformid/$buildid")
            {
                if (! mkdir "$talkbackdir/$vendorid/$productid/$platformid/$buildid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid/$productid/$platformid/$buildid: $!";
                }
            }
        }
        elsif ($os eq "linux")
        {
            $talkbackinidir="$talkbackdir/$vendorid$productid$platformid$buildid";
            if (! -e "$talkbackdir/$vendorid$productid$platformid$buildid" )
            {
                if (! mkdir "$talkbackdir/$vendorid$productid$platformid$buildid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid$productid$platformid$buildid: $!";
                }
            }
        }
        elsif ($os eq "darwin")
        {
            $talkbackinidir="$talkbackdir/$vendorid$productid$platformid$buildid";
            if (! -e "$talkbackdir/$vendorid$productid$platformid$buildid" )
            {
                if (! mkdir "$talkbackdir/$vendorid$productid$platformid$buildid", 755)
                {
                    die "unable to make $talkbackdir/$vendorid$productid$platformid$buildid: $!";
                }
            }
        }
        else
        {
            die "$os not supported yet";
        }

        if (! chdir $talkbackinidir)
        {
            die "unable to cd $talkbackinidir: $!";
        }


        if (!copy("/work/mozilla/mozilla.com/test.mozilla.com/www/talkback/$os/Talkback.ini", "Talkback.ini"))
        {
            die "unable to copy Talkback.ini: $!";
        }
        
        #print "patching Talkback.ini\n";
        if ($os eq "nt")
        {
            $rc = system(("sed", 
                          "-ibak", 
                          "-e", 
                          "s|URLEdit .*|URLEdit = \"mozqa:$talkbackid\"|", 
                          "Talkback.ini")) & 0xffff;
            if ($rc != 0)
            {
                die "unable to edit Talkback.ini: $!";
            }
        }
        elsif ($os eq "linux")
        {
            $rc = system(("sed", 
                          "-ibak", 
                          "-e",
                          "s|URLEditControl .*|URLEditControl = \"mozqa:$talkbackid\"|", 
                          "Talkback.ini")) & 0xffff;
            if ($rc != 0)
            {
                die "unable to edit Talkback.ini: $!";
            }
        }
        elsif ($os eq "darwin")
        {
            $rc = system(("sed", 
                          "-ibak", 
                          "-e",
                          "s|URLEditControl .*|URLEditControl = \"mozqa:$talkbackid\"|", 
                          "Talkback.ini")) & 0xffff;
            if ($rc != 0)
            {
                die "unable to edit Talkback.ini: $!";
            }
        }
        else
        {
            die "$os not supported yet";
        }
    }
}

