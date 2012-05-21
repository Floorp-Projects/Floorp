# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

BEGIN     { COUNT = 1
            if (TYPE == "") TYPE = "vms";
          }

/^[0-9]/  { if      (TYPE == "vms") print COUNT, $1;
            else if (TYPE == "vmd") print COUNT, $4;
            else if (TYPE == "vmx") print COUNT, $2 + $3;
            else if (TYPE == "rss") print COUNT, $6;

            COUNT = COUNT + 1;
          }

/^ /      { print COUNT, "0";
            COUNT = COUNT + 1; \
          }


