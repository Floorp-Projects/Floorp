***** BEGIN LICENSE BLOCK *****
Version: MPL 1.1/GPL 2.0/LGPL 2.1

The contents of this file are subject to the Mozilla Public License Version 
1.1 (the "License"); you may not use this file except in compliance with 
the License. You may obtain a copy of the License at 
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is readme.txt by Brian Bober

Portions created by the Initial Developer are Copyright (C) 2003
the Initial Developer. All Rights Reserved.

Contributor(s):

Alternatively, the contents of this file may be used under the terms of
either the GNU General Public License Version 2 or later (the "GPL"), or
the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
in which case the provisions of the GPL or the LGPL are applicable instead
of those above. If you wish to allow use of your version of this file only
under the terms of either the GPL or the LGPL, and not to allow others to
use your version of this file under the terms of the MPL, indicate your
decision by deleting the provisions above and replace them with the notice
and other provisions required by the GPL or the LGPL. If you do not delete
the provisions above, a recipient may use your version of this file under
the terms of any one of the MPL, the GPL or the LGPL.

***** END LICENSE BLOCK *****



The icons in this directory are Win32 program icons, file association icons, 
and window icons (to appear in the upper left corner of various windows).



Requirements:
Each icon should contain the following devices images:
48x48, 32x32, and 16x16 - 16 color
48x48, 32x32, and 16x16 - True color
48x48, 32x32, and 16x16 - True color XP (Contains alpha shadows)

At this time, we don't think 256 color is a good idea since Windows does
a good job dithering and some systems will use 256 color icons even when
True Color exists.

See bug http://bugzilla.mozilla.org/show_bug.cgi?id=99380 for a lot of rambling about
icons.



Window icons:
Should be named using the following convention: [NAME]-window.ico where [NAME]
represents the name of the window. Example:
history-window.ico

Blank template icon should be available as: template-window.ico

XXXFIXME 
Some icons have been given names such as downloadManager.ico and is because 
there are two naming schemes for windows. This should be remedied. 
Bug http://bugzilla.mozilla.org/show_bug.cgi?id=199576
XXXFIXME


File association icons:
Should be named using the following convention: [NAME]-file.ico where [NAME]
represents the type of file it is. Example:
image-file.ico

Blank template icon should be available as: template-file.ico


Program icon:
Currently, the only available program icon is mozilla.ico
