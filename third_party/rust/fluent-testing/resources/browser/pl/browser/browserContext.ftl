# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

navbar-tooltip-instruction =
    .value =
        { PLATFORM() ->
            [macos] Rozwiń, by wyświetlić historię
           *[other] Kliknij prawym przyciskiem lub rozwiń, by wyświetlić historię
        }

## Back

main-context-menu-back =
    .tooltiptext = Przejdź do poprzedniej strony
    .aria-label = Wstecz
    .accesskey = W
navbar-tooltip-back =
    .value = { main-context-menu-back.tooltiptext }
toolbar-button-back =
    .label = { main-context-menu-back.aria-label }

## Forward

main-context-menu-forward =
    .tooltiptext = Przejdź do następnej strony
    .aria-label = Do przodu
    .accesskey = D
navbar-tooltip-forward =
    .value = { main-context-menu-forward.tooltiptext }
toolbar-button-forward =
    .label = { main-context-menu-forward.aria-label }

## Reload

main-context-menu-reload =
    .aria-label = Odśwież
    .accesskey = O
toolbar-button-reload =
    .label = { main-context-menu-reload.aria-label }

## Stop

main-context-menu-stop =
    .aria-label = Zatrzymaj
    .accesskey = Z
toolbar-button-stop =
    .label = { main-context-menu-stop.aria-label }

## Stop-Reload Button

toolbar-button-stop-reload =
    .title = { main-context-menu-reload.aria-label }

## Save Page

main-context-menu-page-save =
    .label = Zapisz stronę jako…
    .accesskey = s
toolbar-button-page-save =
    .label = { main-context-menu-page-save.label }

## Simple menu items

main-context-menu-bookmark-add =
    .aria-label = Dodaj zakładkę do tej strony
    .accesskey = D
    .tooltiptext = Dodaj zakładkę do tej strony
# Variables
#   $shortcut (String) - A keyboard shortcut for the add bookmark command.
main-context-menu-bookmark-add-with-shortcut =
    .aria-label = Dodaj zakładkę do tej strony
    .accesskey = D
    .tooltiptext = Dodaj zakładkę do tej strony ({ $shortcut })
main-context-menu-bookmark-change =
    .aria-label = Edytuj zakładkę
    .accesskey = D
    .tooltiptext = Edytuj zakładkę
# Variables
#   $shortcut (String) - A keyboard shortcut for the edit bookmark command.
main-context-menu-bookmark-change-with-shortcut =
    .aria-label = Edytuj zakładkę
    .accesskey = D
    .tooltiptext = Edytuj zakładkę ({ $shortcut })
main-context-menu-open-link =
    .label = Otwórz odnośnik
    .accesskey = O
main-context-menu-open-link-new-tab =
    .label = Otwórz odnośnik w nowej karcie
    .accesskey = j
main-context-menu-open-link-container-tab =
    .label = Otwórz odnośnik w nowej karcie w nowym kontekście
    .accesskey = k
main-context-menu-open-link-new-window =
    .label = Otwórz odnośnik w nowym oknie
    .accesskey = n
main-context-menu-open-link-new-private-window =
    .label = Otwórz odnośnik w nowym oknie w trybie prywatnym
    .accesskey = w
main-context-menu-bookmark-this-link =
    .label = Dodaj zakładkę do odnośnika
    .accesskey = D
main-context-menu-save-link =
    .label = Zapisz element docelowy jako…
    .accesskey = s
main-context-menu-save-link-to-pocket =
    .label = Wyślij odnośnik do { -pocket-brand-name }
    .accesskey = o

## The access keys for "Copy Link Location" and "Copy Email Address"
## should be the same if possible; the two context menu items
## are mutually exclusive.

main-context-menu-copy-email =
    .label = Kopiuj adres e-mail
    .accesskey = a
main-context-menu-copy-link =
    .label = Kopiuj adres odnośnika
    .accesskey = a

## Media (video/audio) controls
##
## The accesskey for "Play" and "Pause" are the
## same because the two context-menu items are
## mutually exclusive.

main-context-menu-media-play =
    .label = Odtwórz
    .accesskey = z
main-context-menu-media-pause =
    .label = Wstrzymaj
    .accesskey = W

##

main-context-menu-media-mute =
    .label = Wycisz
    .accesskey = c
main-context-menu-media-unmute =
    .label = Włącz dźwięk
    .accesskey = c
main-context-menu-media-play-speed =
    .label = Szybkość odtwarzania
    .accesskey = S
main-context-menu-media-play-speed-slow =
    .label = Zmniejszona (0,5×)
    .accesskey = Z
main-context-menu-media-play-speed-normal =
    .label = Normalna
    .accesskey = N
main-context-menu-media-play-speed-fast =
    .label = Zwiększona (1,25×)
    .accesskey = k
main-context-menu-media-play-speed-faster =
    .label = Wysoka (1,5×)
    .accesskey = W
# "Ludicrous" is a reference to the movie "Space Balls" and is meant
# to say that this speed is very fast.
main-context-menu-media-play-speed-fastest =
    .label = Absurdalna (2×)
    .accesskey = A
main-context-menu-media-loop =
    .label = Zapętl
    .accesskey = Z

## The access keys for "Show Controls" and "Hide Controls" are the same
## because the two context-menu items are mutually exclusive.

main-context-menu-media-show-controls =
    .label = Wyświetl elementy sterujące
    .accesskey = e
main-context-menu-media-hide-controls =
    .label = Ukryj elementy sterujące
    .accesskey = e

##

main-context-menu-media-video-fullscreen =
    .label = Tryb pełnoekranowy
    .accesskey = n
main-context-menu-media-video-leave-fullscreen =
    .label = Opuść tryb pełnoekranowy
    .accesskey = u
# This is used when right-clicking on a video in the
# content area when the Picture-in-Picture feature is enabled.
main-context-menu-media-pip =
    .label = Obraz w obrazie
    .accesskey = O
main-context-menu-image-reload =
    .label = Odśwież obraz
    .accesskey = O
main-context-menu-image-view =
    .label = Pokaż obraz
    .accesskey = P
main-context-menu-video-view =
    .label = Pokaż wideo
    .accesskey = k
main-context-menu-image-copy =
    .label = Kopiuj obraz
    .accesskey = r
main-context-menu-image-copy-location =
    .label = Kopiuj adres obrazu
    .accesskey = b
main-context-menu-video-copy-location =
    .label = Kopiuj adres wideo
    .accesskey = u
main-context-menu-audio-copy-location =
    .label = Kopiuj adres dźwięku
    .accesskey = u
main-context-menu-image-save-as =
    .label = Zapisz obraz jako…
    .accesskey = Z
main-context-menu-image-email =
    .label = Wyślij obraz…
    .accesskey = o
main-context-menu-image-set-as-background =
    .label = Ustaw jako tapetę…
    .accesskey = t
main-context-menu-image-info =
    .label = Pokaż informacje o obrazie
    .accesskey = f
main-context-menu-image-desc =
    .label = Pokaż opis
    .accesskey = s
main-context-menu-video-save-as =
    .label = Zapisz wideo jako…
    .accesskey = s
main-context-menu-audio-save-as =
    .label = Zapisz dźwięk jako…
    .accesskey = s
main-context-menu-video-image-save-as =
    .label = Zapisz klatkę jako…
    .accesskey = k
main-context-menu-video-email =
    .label = Wyślij wideo…
    .accesskey = o
main-context-menu-audio-email =
    .label = Wyślij dźwięk…
    .accesskey = d
main-context-menu-plugin-play =
    .label = Aktywuj tę wtyczkę
    .accesskey = w
main-context-menu-plugin-hide =
    .label = Ukryj tę wtyczkę
    .accesskey = U
main-context-menu-save-to-pocket =
    .label = Wyślij stronę do { -pocket-brand-name }
    .accesskey = W
main-context-menu-send-to-device =
    .label = Wyślij stronę do
    .accesskey = W
main-context-menu-view-background-image =
    .label = Pokaż obraz tła
    .accesskey = t
main-context-menu-generate-new-password =
    .label = Użyj wygenerowanego hasła…
    .accesskey = h
main-context-menu-keyword =
    .label = Utwórz słowo kluczowe dla tej wyszukiwarki…
    .accesskey = U
main-context-menu-link-send-to-device =
    .label = Wyślij odnośnik do
    .accesskey = W
main-context-menu-frame =
    .label = Ramka
    .accesskey = R
main-context-menu-frame-show-this =
    .label = Pokaż tylko tę ramkę
    .accesskey = r
main-context-menu-frame-open-tab =
    .label = Otwórz ramkę w nowej karcie
    .accesskey = j
main-context-menu-frame-open-window =
    .label = Otwórz ramkę w nowym oknie
    .accesskey = n
main-context-menu-frame-reload =
    .label = Odśwież ramkę
    .accesskey = O
main-context-menu-frame-bookmark =
    .label = Dodaj zakładkę do ramki
    .accesskey = D
main-context-menu-frame-save-as =
    .label = Zapisz ramkę jako…
    .accesskey = Z
main-context-menu-frame-print =
    .label = Drukuj ramkę…
    .accesskey = u
main-context-menu-frame-view-source =
    .label = Pokaż źródło ramki
    .accesskey = P
main-context-menu-frame-view-info =
    .label = Pokaż informacje o ramce
    .accesskey = i
main-context-menu-print-selection =
    .label = Drukuj tylko zaznaczenie
    .accesskey = u
main-context-menu-view-selection-source =
    .label = Pokaż źródło zaznaczenia
    .accesskey = d
main-context-menu-view-page-source =
    .label = Pokaż źródło strony
    .accesskey = y
main-context-menu-view-page-info =
    .label = Pokaż informacje o stronie
    .accesskey = I
main-context-menu-bidi-switch-text =
    .label = Przełącz kierunek tekstu
    .accesskey = t
main-context-menu-bidi-switch-page =
    .label = Przełącz kierunek strony
    .accesskey = s
main-context-menu-inspect-element =
    .label = Zbadaj element
    .accesskey = t
main-context-menu-inspect-a11y-properties =
    .label = Zbadaj własności dostępności
main-context-menu-eme-learn-more =
    .label = Więcej informacji o DRM…
    .accesskey = D
