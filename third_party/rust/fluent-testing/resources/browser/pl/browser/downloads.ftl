# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


## The title and aria-label attributes are used by screen readers to describe
## the Downloads Panel.

downloads-window =
    .title = Pobierane pliki
downloads-panel =
    .aria-label = Pobierane pliki

##

# The style attribute has the width of the Downloads Panel expressed using
# a CSS unit. The longest labels that should fit are usually those of
# in-progress and blocked downloads.
downloads-panel-list =
    .style = width: 60ch

downloads-cmd-pause =
    .label = Wstrzymaj
    .accesskey = W
downloads-cmd-resume =
    .label = Wznów
    .accesskey = z
downloads-cmd-cancel =
    .tooltiptext = Anuluj
downloads-cmd-cancel-panel =
    .aria-label = Anuluj

# This message is only displayed on Windows and Linux devices
downloads-cmd-show-menuitem =
    .label = Otwórz folder nadrzędny
    .accesskey = f

# This message is only displayed on macOS devices
downloads-cmd-show-menuitem-mac =
    .label = Pokaż w Finderze
    .accesskey = F

downloads-cmd-use-system-default =
    .label = Otwórz w przeglądarce systemowej
    .accesskey = O

downloads-cmd-always-use-system-default =
    .label = Zawsze otwieraj w przeglądarce systemowej
    .accesskey = Z

downloads-cmd-show-button =
    .tooltiptext =
        { PLATFORM() ->
            [macos] Pokaż w Finderze
           *[other] Otwórz folder nadrzędny
        }

downloads-cmd-show-panel =
    .aria-label =
        { PLATFORM() ->
            [macos] Pokaż w Finderze
           *[other] Otwórz folder nadrzędny
        }
downloads-cmd-show-description =
    .value =
        { PLATFORM() ->
            [macos] Pokaż w Finderze
           *[other] Otwórz folder nadrzędny
        }

downloads-cmd-show-downloads =
    .label = Pokaż folder z pobranymi
downloads-cmd-retry =
    .tooltiptext = Spróbuj ponownie
downloads-cmd-retry-panel =
    .aria-label = Spróbuj ponownie
downloads-cmd-go-to-download-page =
    .label = Przejdź do strony pobierania
    .accesskey = P
downloads-cmd-copy-download-link =
    .label = Kopiuj adres, z którego pobrano plik
    .accesskey = K
downloads-cmd-remove-from-history =
    .label = Usuń z historii
    .accesskey = U
downloads-cmd-clear-list =
    .label = Wyczyść listę
    .accesskey = c
downloads-cmd-clear-downloads =
    .label = Wyczyść listę
    .accesskey = c

# This command is shown in the context menu when downloads are blocked.
downloads-cmd-unblock =
    .label = Pozwól pobrać
    .accesskey = P

# This is the tooltip of the action button shown when malware is blocked.
downloads-cmd-remove-file =
    .tooltiptext = Usuń plik

downloads-cmd-remove-file-panel =
    .aria-label = Usuń plik

# This is the tooltip of the action button shown when potentially unwanted
# downloads are blocked. This opens a dialog where the user can choose
# whether to unblock or remove the download. Removing is the default option.
downloads-cmd-choose-unblock =
    .tooltiptext = Usuń plik lub pozwól go pobrać

downloads-cmd-choose-unblock-panel =
    .aria-label = Usuń plik lub pozwól go pobrać

# This is the tooltip of the action button shown when uncommon downloads are
# blocked.This opens a dialog where the user can choose whether to open the
# file or remove the download. Opening is the default option.
downloads-cmd-choose-open =
    .tooltiptext = Otwórz lub usuń plik

downloads-cmd-choose-open-panel =
    .aria-label = Otwórz lub usuń plik

# Displayed when hovering a blocked download, indicates that it's possible to
# show more information for user to take the next action.
downloads-show-more-information =
    .value = Wyświetl więcej informacji

# Displayed when hovering a complete download, indicates that it's possible to
# open the file using an app available in the system.
downloads-open-file =
    .value = Otwórz plik

# Displayed when hovering a download which is able to be retried by users,
# indicates that it's possible to download this file again.
downloads-retry-download =
    .value = Pobierz ponownie

# Displayed when hovering a download which is able to be cancelled by users,
# indicates that it's possible to cancel and stop the download.
downloads-cancel-download =
    .value = Anuluj pobieranie

# This string is shown at the bottom of the Downloads Panel when all the
# downloads fit in the available space, or when there are no downloads in
# the panel at all.
downloads-history =
    .label = Wyświetl wszystkie
    .accesskey = W

# This string is shown at the top of the Download Details Panel, to indicate
# that we are showing the details of a single download.
downloads-details =
    .title = Szczegóły pobieranego pliku

downloads-clear-downloads-button =
    .label = Wyczyść listę
    .tooltiptext = Ukończone, anulowane i nieudane pobierania zostaną usunięte

# This string is shown when there are no items in the Downloads view, when it
# is displayed inside a browser tab.
downloads-list-empty =
    .value = Brak pobranych plików

# This string is shown when there are no items in the Downloads Panel.
downloads-panel-empty =
    .value = Brak pobranych podczas tej sesji.
