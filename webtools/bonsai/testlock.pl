#!/usr/bin/perl --
# -*- Mode: perl; indent-tabs-mode: nil -*-

require 'utils.pl';

Lock();

print "Got lock.\n";

sleep 10;

Unlock();
