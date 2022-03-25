# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


## Strings used for device manager

devmgr =
    .title = Menedżer urządzeń
    .style = width: 67em; height: 32em;

devmgr-devlist =
    .label = Urządzenia i moduły zabezpieczające

devmgr-header-details =
    .label = Szczegóły

devmgr-header-value =
    .label = Wartość

devmgr-button-login =
    .label = Zaloguj
    .accesskey = Z

devmgr-button-logout =
    .label = Wyloguj
    .accesskey = W

devmgr-button-changepw =
    .label = Zmień hasło
    .accesskey = h

devmgr-button-load =
    .label = Wczytaj
    .accesskey = c

devmgr-button-unload =
    .label = Usuń z pamięci
    .accesskey = U

devmgr-button-enable-fips =
    .label = Włącz FIPS
    .accesskey = F

devmgr-button-disable-fips =
    .label = Wyłącz FIPS
    .accesskey = F

## Strings used for load device

load-device =
    .title = Wczytaj sterownik urządzenia PKCS#11

load-device-info = Wprowadź informacje dla modułu, który ma zostać dodany.

load-device-modname =
    .value = Nazwa modułu:
    .accesskey = m

load-device-modname-default =
    .value = Nowy moduł PKCS#11

load-device-filename =
    .value = Nazwa pliku modułu:
    .accesskey = N

load-device-browse =
    .label = Przeglądaj…
    .accesskey = P

## Token Manager

devinfo-status =
    .label = Stan

devinfo-status-disabled =
    .label = Wyłączony

devinfo-status-not-present =
    .label = Nieobecny

devinfo-status-uninitialized =
    .label = Niezainicjowany

devinfo-status-not-logged-in =
    .label = Niezalogowany

devinfo-status-logged-in =
    .label = Zalogowany

devinfo-status-ready =
    .label = Gotowy

devinfo-desc =
    .label = Opis

devinfo-man-id =
    .label = Producent

devinfo-hwversion =
    .label = Wersja HW
devinfo-fwversion =
    .label = Wersja FW

devinfo-modname =
    .label = Moduł

devinfo-modpath =
    .label = Ścieżka

login-failed = Nie można się zalogować

devinfo-label =
    .label = Etykieta

devinfo-serialnum =
    .label = Numer seryjny

fips-nonempty-password-required = Tryb FIPS wymaga hasła głównego ustawionego dla każdego urządzenia zabezpieczającego. Ustaw hasło przed włączeniem trybu FIPS.

fips-nonempty-primary-password-required = Tryb FIPS wymaga hasła głównego ustawionego dla każdego urządzenia zabezpieczającego. Ustaw hasło przed włączeniem trybu FIPS.
unable-to-toggle-fips = Nie udało się zmienić trybu FIPS dla urządzenia bezpieczeństwa. Zaleca się zakończenie pracy i ponowne uruchomienie tego programu.
load-pk11-module-file-picker-title = Wybierz sterownik urządzenia PKCS#11 do wczytania

# Load Module Dialog
load-module-help-empty-module-name =
    .value = Nazwa modułu nie może być pusta.

# Do not translate 'Root Certs'
load-module-help-root-certs-module-name =
    .value = Nazwa „Root Certs” jest zarezerwowana i nie może zostać użyta jako nazwa modułu.

add-module-failure = Nie można dodać modułu
del-module-warning = Czy na pewno usunąć wybrany moduł szyfrujący?
del-module-error = Nie można usunąć modułu
