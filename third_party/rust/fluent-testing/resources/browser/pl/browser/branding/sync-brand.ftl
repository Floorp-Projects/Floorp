# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

-sync-brand-short-name =
    { $case ->
       *[nom]
            { $capitalization ->
               *[upper] Synchronizacja
                [lower] synchronizacja
            }
        [gen]
            { $capitalization ->
               *[upper] Synchronizacji
                [lower] synchronizacji
            }
        [dat]
            { $capitalization ->
               *[upper] Synchronizacji
                [lower] synchronizacji
            }
        [acc]
            { $capitalization ->
               *[upper] Synchronizację
                [lower] synchronizację
            }
        [ins]
            { $capitalization ->
               *[upper] Synchronizacją
                [lower] synchronizacją
            }
        [loc]
            { $capitalization ->
               *[upper] Synchronizacji
                [lower] synchronizacji
            }
    }
# “Sync” can be localized, “Firefox” must be treated as a brand,
# and kept in English.
-sync-brand-name =
    { $case ->
       *[nom]
            { $capitalization ->
               *[upper] Synchronizacja Firefoksa
                [lower] synchronizacja Firefoksa
            }
        [gen]
            { $capitalization ->
               *[upper] Synchronizacji Firefoksa
                [lower] synchronizacji Firefoksa
            }
        [dat]
            { $capitalization ->
               *[upper] Synchronizacji Firefoksa
                [lower] synchronizacji Firefoksa
            }
        [acc]
            { $capitalization ->
               *[upper] Synchronizację Firefoksa
                [lower] synchronizację Firefoksa
            }
        [ins]
            { $capitalization ->
               *[upper] Synchronizacją Firefoksa
                [lower] synchronizacją Firefoksa
            }
        [loc]
            { $capitalization ->
               *[upper] Synchronizacji Firefoksa
                [lower] synchronizacji Firefoksa
            }
    }
# “Account” can be localized, “Firefox” must be treated as a brand,
# and kept in English.
-fxaccount-brand-name =
    { $case ->
       *[nom]
            { $capitalization ->
               *[upper] Konto Firefoksa
                [lower] konto Firefoksa
            }
        [gen]
            { $capitalization ->
               *[upper] Konta Firefoksa
                [lower] konta Firefoksa
            }
        [dat]
            { $capitalization ->
               *[upper] Kontu Firefoksa
                [lower] kontu Firefoksa
            }
        [acc]
            { $capitalization ->
               *[upper] Konto Firefoksa
                [lower] konto Firefoksa
            }
        [ins]
            { $capitalization ->
               *[upper] Kontem Firefoksa
                [lower] kontem Firefoksa
            }
        [loc]
            { $capitalization ->
               *[upper] Koncie Firefoksa
                [lower] koncie Firefoksa
            }
    }
