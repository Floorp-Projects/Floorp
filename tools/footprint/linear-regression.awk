# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is linear-regression.awk, released November 13, 2000.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    Chris Waterson <waterson@netscape.com>
#
# Alternatively, the contents of this file may be used under the
# terms of the GNU Public License (the "GPL"), in which case the
# provisions of the GPL are applicable instead of those above.
# If you wish to allow use of your version of this file only
# under the terms of the GPL and not to allow others to use your
# version of this file under the MPL, indicate your decision by
# deleting the provisions above and replace them with the notice
# and other provisions required by the GPL.  If you do not delete
# the provisions above, a recipient may use your version of this
# file under either the MPL or the GPL.
#
BEGIN {
        if (!Skip) Skip = 0;
      }

NR>Skip {
     sx += $1;
     sy += $2;
     sxy += $1 * $2;
     sx2 += $1 * $1;
#     print NR " " sx " " sy " " sxy " " sx2
     }

END {
    DataPoints = NR - Skip;
    b1 = (DataPoints * sxy - sx * sy) / (DataPoints * sx2 - sx * sx);
    b0 = (sy - b1 * sx ) / DataPoints;
    print b1, "* x +", b0; 
    }
