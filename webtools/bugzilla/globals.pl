# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dan Mosedale <dmose@mozilla.org>
#                 Jacob Steenhagen <jake@bugzilla.org>
#                 Bradley Baetz <bbaetz@student.usyd.edu.au>
#                 Christopher Aillon <christopher@aillon.com>
#                 Joel Peshkin <bugreport@peshkin.net>
#                 Dave Lawrence <dkl@redhat.com>
#                 Max Kanat-Alexander <mkanat@bugzilla.org>
#                 Lance Larsh <lance.larsh@oracle.com>

# Contains some global variables and routines used throughout bugzilla.

use strict;

use Bugzilla::Constants;
use Bugzilla::Util;
# Bring ChmodDataFile in until this is all moved to the module
use Bugzilla::Config qw(:DEFAULT ChmodDataFile $localconfig $datadir);
use Bugzilla::User;
use Bugzilla::Error;

# XXX - Move this to Bugzilla::Config once code which uses these has moved out
# of globals.pl
do $localconfig;

1;
