@stats = (
    {
	name => 'Thunderbird',
        version => '0.4',
	isactive => 1,
        path => '/pub/mozilla.org/thunderbird/releases/0.4',
        platforms => {
            Windows => {
                standard  => { name => "thunderbird-0.4-win32.zip" }
	    },
            Mac => {
                standard  => { name => "thunderbird-0.4-macosx.dmg.gz" }
	    },
            Linux => {
                standard  => { name => "thunderbird-0.4-i686-pc-linux-gtk2-gnu.tar.bz2" }
	    }
        }
    },
    {
	name => 'Thunderbird',
        version => '0.5',
	isactive => 1,
        path => '/pub/mozilla.org/thunderbird/releases/0.5',
        platforms => {
            Windows => {
                standard  => { name => "thunderbird-0.5-win32.zip" }
	    },
            Mac => {
                standard  => { name => "thunderbird-0.5-macosx.dmg.gz" }
	    },
            Linux => {
                standard  => { name => "thunderbird-0.5-i686-pc-linux-gtk2-gnu.tar.bz2" }
	    }
        }
    },
    {
	name => 'Thunderbird',
        version => '0.6',
	isactive => 1,
        path => '/pub/mozilla.org/thunderbird/releases/0.6',
        platforms => {
            Windows => {
                standard  => { name => "thunderbird-0.6-win32.zip" },
                installer => { name => "ThunderbirdSetup-0.6.exe" }
	    },
            Mac => {
                standard  => { name => "thunderbird-0.6-macosx.dmg.gz" }
	    },
            Linux => {
                standard  => { name => "thunderbird-0.6-i686-linux-gtk2+xft.tar.gz" }
	    }
        }
    }
);

1;
