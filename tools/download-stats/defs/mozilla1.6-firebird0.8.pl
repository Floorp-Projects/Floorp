#!/usr/bin/perl

@stats = (
    {
	name => 'Mozilla',
        version => '1.5',
	isactive => 0,
        path => '/pub/mozilla.org/mozilla/releases/mozilla1.5',
        platforms => {
            Windows => {
                stub      => { name => "mozilla-win32-1.5-stub-installer.exe", size => 231120 },
                installer => { name => "mozilla-win32-1.5-installer.exe", size => 12432080 },
                zip       => { name => "mozilla-win32-1.5.zip", size => 10805398 },
                tbzip     => { name => "mozilla-win32-1.5-talkback.zip", size => 11063159 }
	    },
            Mac => {
                standard  => { name => "mozilla-mac-MachO-1.5.dmg.gz" }
	    },
            Linux => {
                stub      => { name => "mozilla-i686-pc-linux-gnu-1.5-installer.tar.gz" },
                installer => { name => "mozilla-i686-pc-linux-gnu-1.5-sea.tar.gz" },
                tarball   => { name => "mozilla-i686-pc-linux-gnu-1.5.tar.gz" }
	    }
        }
    },
    {
	name => 'Mozilla',
        version => '1.6',
	isactive => 1,
        path => '/pub/mozilla.org/mozilla/releases/mozilla1.6',
        platforms => {
            Windows => {
                stub      => { name => "mozilla-win32-1.6-stub-installer.exe" },
                installer => { name => "mozilla-win32-1.6-installer.exe" },
                zip       => { name => "mozilla-win32-1.6.zip" },
	    },
            Mac => {
                standard  => { name => "mozilla-mac-MachO-1.6.dmg.gz" }
	    },
            Linux => {
                stub      => { name => "mozilla-i686-pc-linux-gnu-1.6-stub-installer.tar.gz" },
                installer => { name => "mozilla-i686-pc-linux-gnu-1.6-installer.tar.gz" },
                tarball   => { name => "mozilla-i686-pc-linux-gnu-1.6.tar.gz" }
	    }
        }
    },
    {
	name => 'Firefox',
        version => '0.8',
	isactive => 1,
        path => '/pub/mozilla.org/firefox/releases/0.8',
        platforms => {
            Windows => {
                zip       => { name => "Firefox-0.8.zip" },
                installer => { name => "FirefoxSetup-0.8.exe" }
	    },
            Mac => {
                standard  => { name => "firefox-0.8-mac.dmg.gz" }
	    },
            Linux => {
                tarball   => { name => "firefox-0.8-i686-pc-linux-gnu.tar.gz" },
                gtk2xft   => { name => "firefox-0.8-i686-linux-gtk2+xft.tar.gz" }
	    }
        }
    },
    {
	name => 'Thunderbird',
        version => '0.3',
	isactive => 0,
        path => '/pub/mozilla.org/thunderbird/releases/0.3',
        platforms => {
            Windows => {
                standard  => { name => "thunderbird-0.3-win32.zip" }
	    },
            Mac => {
                standard  => { name => "thunderbird-0.3-macosx.dmg.gz" }
	    },
            Linux => {
                standard  => { name => "thunderbird-0.3-i686-pc-linux-gtk2-gnu.tar.bz2" }
	    }
        }
    }
);

1;
