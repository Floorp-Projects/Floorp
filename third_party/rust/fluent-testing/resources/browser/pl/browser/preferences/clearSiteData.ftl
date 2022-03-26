# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

clear-site-data-window =
    .title = Czyszczenie danych
    .style = width: 35em
clear-site-data-description = Wyczyszczenie wszystkich ciasteczek i danych stron przechowywanych przez przeglądarkę { -brand-short-name } może spowodować wylogowanie ze stron i usunąć treści offline. Czyszczenie danych pamięci podręcznej nie wpłynie na zachowane dane logowania.
clear-site-data-close-key =
    .key = w
# The parameters in parentheses in this string describe disk usage
# in the format ($amount $unit), e.g. "Cookies and Site Data (24 KB)"
# Variables:
#   $amount (Number) - Amount of site data currently stored on disk
#   $unit (String) - Abbreviation of the unit that $amount is in, e.g. "MB"
clear-site-data-cookies-with-data =
    .label = Ciasteczka i dane stron ({ $amount } { $unit })
    .accesskey = C
# This string is a placeholder for while the data used to fill
# clear-site-data-cookies-with-data is loading. This placeholder is usually
# only shown for a very short time (< 1s), so it should be very similar
# or the same as clear-site-data-cookies-with-data (except the amount and unit),
# to avoid flickering.
clear-site-data-cookies-empty =
    .label = Ciasteczka i dane stron
    .accesskey = C
clear-site-data-cookies-info = Może skutkować wylogowaniem ze stron po wyczyszczeniu
# The parameters in parentheses in this string describe disk usage
# in the format ($amount $unit), e.g. "Cached Web Content (24 KB)"
# Variables:
#   $amount (Number) - Amount of cache currently stored on disk
#   $unit (String) - Abbreviation of the unit that $amount is in, e.g. "MB"
clear-site-data-cache-with-data =
    .label = Treści zachowane w pamięci podręcznej ({ $amount } { $unit })
    .accesskey = T
# This string is a placeholder for while the data used to fill
# clear-site-data-cache-with-data is loading. This placeholder is usually
# only shown for a very short time (< 1s), so it should be very similar
# or the same as clear-site-data-cache-with-data (except the amount and unit),
# to avoid flickering.
clear-site-data-cache-empty =
    .label = Treści zachowane w pamięci podręcznej
    .accesskey = T
clear-site-data-cache-info = Skutkuje koniecznością ponownego pobrania obrazów i innych danych przez strony
clear-site-data-cancel =
    .label = Anuluj
    .accesskey = A
clear-site-data-clear =
    .label = Wyczyść
    .accesskey = W
clear-site-data-dialog =
    .buttonlabelaccept = Wyczyść
    .buttonaccesskeyaccept = W
