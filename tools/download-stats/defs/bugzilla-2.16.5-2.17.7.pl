#!/usr/bin/perl

@stats = (
    {
	name => 'Bugzilla',
        version => '20040303',
	isactive => 1,
        path => '/pub/mozilla.org/webtools',
        platforms => {
            '2.16.4' => {
                standard  => { name => "bugzilla-2.16.4.tar.gz" }
	    },
            '2.17.6' => {
                standard  => { name => "bugzilla-2.17.6.tar.gz" }
	    },
            'bugzilla-submit' => {
                standard  => { name => "bugzilla-submit-0.5.tar.gz" }
	    },
        }
    }
);

1;
