# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

do-not-track-description = Informowanie witryn o preferencjach względem śledzenia (wysyłanie nagłówka „Do Not Track”):
do-not-track-learn-more = Więcej informacji
do-not-track-option-default-content-blocking-known =
    .label = gdy { -brand-short-name } blokuje znane elementy śledzące
do-not-track-option-always =
    .label = zawsze
pref-page-title =
    { PLATFORM() ->
        [windows] Opcje
       *[other] Preferencje
    }
# This is used to determine the width of the search field in about:preferences,
# in order to make the entire placeholder string visible
#
# Please keep the placeholder string short to avoid truncation.
#
# Notice: The value of the `.style` attribute is a CSS string, and the `width`
# is the name of the CSS property. It is intended only to adjust the element's width.
# Do not translate.
search-input-box =
    .style = width: 15.4em
    .placeholder =
        { PLATFORM() ->
            [windows] Szukaj w opcjach
           *[other] Szukaj w preferencjach
        }
managed-notice = Przeglądarka jest zarządzana przez administratora
category-list =
    .aria-label = Kategorie
pane-general-title = Ogólne
category-general =
    .tooltiptext = Ogólne ustawienia
pane-home-title = Uruchamianie
category-home =
    .tooltiptext = Ustawienia powiązane z uruchamianiem programu
pane-search-title = Wyszukiwanie
category-search =
    .tooltiptext = Ustawienia dotyczące wyszukiwania
pane-privacy-title = Prywatność i bezpieczeństwo
category-privacy =
    .tooltiptext = Ustawienia dotyczące prywatności i bezpieczeństwa
pane-sync-title2 = { -sync-brand-short-name }
category-sync2 =
    .tooltiptext = Ustawienia dotyczące synchronizacji
pane-experimental-title = Eksperymenty
category-experimental =
    .tooltiptext = Eksperymenty przeglądarki { -brand-short-name }
pane-experimental-subtitle = Zachowaj ostrożność
pane-experimental-search-results-header = Eksperymenty przeglądarki { -brand-short-name }: zachowaj ostrożność
pane-experimental-description = Modyfikacja zaawansowanych preferencji może wpłynąć na wydajność lub bezpieczeństwo przeglądarki { -brand-short-name }.
pane-experimental-reset =
    .label = Przywróć domyślne
    .accesskey = P
help-button-label = Wsparcie programu { -brand-short-name }
addons-button-label = Rozszerzenia i motywy
focus-search =
    .key = f
close-button =
    .aria-label = Zamknij

## Browser Restart Dialog

feature-enable-requires-restart = Konieczne jest ponowne uruchomienie przeglądarki { -brand-short-name }, aby włączyć tę funkcję.
feature-disable-requires-restart = Konieczne jest ponowne uruchomienie przeglądarki { -brand-short-name }, aby wyłączyć tę funkcję.
should-restart-title = Ponowne uruchomienie przeglądarki { -brand-short-name }
should-restart-ok = Uruchom przeglądarkę { -brand-short-name } ponownie
cancel-no-restart-button = Anuluj
restart-later = Później

## Extension Control Notifications
##
## These strings are used to inform the user
## about changes made by extensions to browser settings.
##
## <img data-l10n-name="icon"/> is going to be replaced by the extension icon.
##
## Variables:
##   $name (String): name of the extension

# This string is shown to notify the user that their home page
# is being controlled by an extension.
extension-controlled-homepage-override = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje stronę startową.
# This string is shown to notify the user that their new tab page
# is being controlled by an extension.
extension-controlled-new-tab-url = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje stronę nowej karty.
# This string is shown to notify the user that the password manager setting
# is being controlled by an extension
extension-controlled-password-saving = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje to ustawienie.
# This string is shown to notify the user that their notifications permission
# is being controlled by an extension.
extension-controlled-web-notifications = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje to ustawienie.
# This string is shown to notify the user that the default search engine
# is being controlled by an extension.
extension-controlled-default-search = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> zmieniło domyślną wyszukiwarkę.
# This string is shown to notify the user that Container Tabs
# are being enabled by an extension.
extension-controlled-privacy-containers = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> wymaga włączonych „Kart z kontekstem”.
# This string is shown to notify the user that their content blocking "All Detected Trackers"
# preferences are being controlled by an extension.
extension-controlled-websites-content-blocking-all-trackers = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje to ustawienie.
# This string is shown to notify the user that their proxy configuration preferences
# are being controlled by an extension.
extension-controlled-proxy-config = Rozszerzenie „{ $name }” <img data-l10n-name="icon"/> kontroluje, jak { -brand-short-name } łączy się z Internetem.
# This string is shown after the user disables an extension to notify the user
# how to enable an extension that they disabled.
#
# <img data-l10n-name="addons-icon"/> will be replaced with Add-ons icon
# <img data-l10n-name="menu-icon"/> will be replaced with Menu icon
extension-controlled-enable = Aby włączyć rozszerzenie, przejdź do sekcji dodatki <img data-l10n-name="addons-icon"/> w menu <img data-l10n-name="menu-icon"/>.

## Preferences UI Search Results

search-results-header = Wyniki wyszukiwania
# `<span data-l10n-name="query"></span>` will be replaced by the search term.
search-results-empty-message =
    { PLATFORM() ->
        [windows] Niestety! W opcjach niczego nie odnaleziono dla wyszukiwania „<span data-l10n-name="query"></span>”.
       *[other] Niestety! W preferencjach niczego nie odnaleziono dla wyszukiwania „<span data-l10n-name="query"></span>”.
    }
search-results-help-link = Potrzebujesz pomocy? Odwiedź <a data-l10n-name="url">pomoc przeglądarki { -brand-short-name }</a>.

## General Section

startup-header = Uruchamianie
# { -brand-short-name } will be 'Firefox Developer Edition',
# since this setting is only exposed in Firefox Developer Edition
separate-profile-mode =
    .label = Jednoczesne działanie programu { -brand-short-name } oraz programu Firefox
use-firefox-sync = Podczas jednoczesnego działania wykorzystywane są oddzielne profile. Można wykorzystać { -sync-brand-short-name(case: "acc", capitalization: "lower") }, aby dzielić dane między nimi.
get-started-not-logged-in = Zaloguj się do { -sync-brand-short-name(case: "gen", capitalization: "lower") }…
get-started-configured = Otwórz preferencje { -sync-brand-short-name(case: "gen", capitalization: "lower") }
always-check-default =
    .label = Sprawdzanie, czy { -brand-short-name } jest domyślną przeglądarką
    .accesskey = e
is-default = { -brand-short-name } jest obecnie domyślną przeglądarką
is-not-default = { -brand-short-name } nie jest obecnie domyślną przeglądarką
set-as-my-default-browser =
    .label = Ustaw jako domyślną…
    .accesskey = U
startup-restore-previous-session =
    .label = Przywracanie poprzedniej sesji
    .accesskey = P
startup-restore-warn-on-quit =
    .label = Ostrzeganie przy zamykaniu przeglądarki
disable-extension =
    .label = Wyłącz rozszerzenie
tabs-group-header = Karty
ctrl-tab-recently-used-order =
    .label = Przełączanie kart za pomocą Ctrl+Tab w kolejności ostatnich wyświetleń
    .accesskey = T
open-new-link-as-tabs =
    .label = Otwieranie odnośników w kartach zamiast w nowych oknach
    .accesskey = O
warn-on-close-multiple-tabs =
    .label = Ostrzeganie przed zamknięciem wielu kart
    .accesskey = a
warn-on-open-many-tabs =
    .label = Ostrzeganie o otwarciu zbyt wielu kart mogących spowolnić przeglądarkę { -brand-short-name }
    .accesskey = m
switch-links-to-new-tabs =
    .label = Przechodzenie do nowych kart otwieranych poprzez odnośniki
    .accesskey = c
show-tabs-in-taskbar =
    .label = Podgląd kart na pasku zadań Windows
    .accesskey = W
browser-containers-enabled =
    .label = Karty z kontekstem.
    .accesskey = K
browser-containers-learn-more = Więcej informacji
browser-containers-settings =
    .label = Dostosuj…
    .accesskey = D
containers-disable-alert-title = Czy zamknąć wszystkie karty z kontekstem?
containers-disable-alert-desc =
    { $tabCount ->
        [one] Jeśli wyłączysz funkcję kart z kontekstem, jedna taka karta zostanie zamknięta. Czy na pewno wyłączyć karty z kontekstem?
        [few] Jeśli wyłączysz funkcję kart z kontekstem, { $tabCount } takie karty zostaną zamknięte. Czy na pewno wyłączyć karty z kontekstem?
       *[many] Jeśli wyłączysz funkcję kart z kontekstem, { $tabCount } takich kart zostanie zamkniętych. Czy na pewno wyłączyć karty z kontekstem?
    }
containers-disable-alert-ok-button =
    { $tabCount ->
        [one] Zamknij kartę z kontekstem
        [few] Zamknij { $tabCount } karty z kontekstem
       *[many] Zamknij { $tabCount } kart z kontekstem
    }
containers-disable-alert-cancel-button = Anuluj
containers-remove-alert-title = Usuwanie kontekstu
# Variables:
#   $count (Number) - Number of tabs that will be closed.
containers-remove-alert-msg =
    { $count ->
        [one] Jeśli usuniesz teraz ten kontekst, { $count } karta zostanie zamknięta. Czy na pewno usunąć ten kontekst?
        [few] Jeśli usuniesz teraz ten kontekst, { $count } karty zostaną zamknięte. Czy na pewno usunąć ten kontekst?
       *[many] Jeśli usuniesz teraz ten kontekst, { $count } kart zostanie zamkniętych. Czy na pewno usunąć ten kontekst?
    }
containers-remove-ok-button = Usuń
containers-remove-cancel-button = Nie usuwaj

## General Section - Language & Appearance

language-and-appearance-header = Język i wygląd
fonts-and-colors-header = Czcionki i kolory
default-font = Domyślna czcionka:
    .accesskey = D
default-font-size = Rozmiar:
    .accesskey = R
advanced-fonts =
    .label = Zaawansowane…
    .accesskey = s
colors-settings =
    .label = Kolory…
    .accesskey = K
# Zoom is a noun, and the message is used as header for a group of options
preferences-zoom-header = Powiększenie
preferences-default-zoom = Domyślne powiększenie:
    .accesskey = D
preferences-default-zoom-value =
    .label = { $percentage }%
preferences-zoom-text-only =
    .label = Powiększaj tylko tekst
    .accesskey = P
language-header = Język
choose-language-description = Wybierz preferowany język, w jakim mają być wyświetlane strony
choose-button =
    .label = Wybierz…
    .accesskey = e
choose-browser-language-description = Wybierz język używany do wyświetlania interfejsu użytkownika przeglądarki { -brand-short-name } (menu, komunikaty, powiadomienia itp.).
manage-browser-languages-button =
    .label = Wybierz alternatywne…
    .accesskey = W
confirm-browser-language-change-description = Uruchom przeglądarkę { -brand-short-name } ponownie, aby zastosować zmiany.
confirm-browser-language-change-button = Uruchom ponownie
translate-web-pages =
    .label = Tłumaczenie stron WWW
    .accesskey = T
# The <img> element is replaced by the logo of the provider
# used to provide machine translations for web pages.
translate-attribution = Tłumaczeń dostarcza <img data-l10n-name="logo"/>
translate-exceptions =
    .label = Wyjątki…
    .accesskey = i
# Variables:
#    $localeName (string) - Localized name of the locale to be used.
use-system-locale =
    .label = Używaj ustawień systemu operacyjnego dla języka „{ $localeName }” do formatowania dat, czasu, liczb i miar
check-user-spelling =
    .label = Sprawdzanie pisowni podczas wprowadzania tekstu
    .accesskey = S

## General Section - Files and Applications

files-and-applications-title = Pliki i aplikacje
download-header = Pobieranie
download-save-to =
    .label = Pobierane pliki zapisuj do:
    .accesskey = o
download-choose-folder =
    .label =
        { PLATFORM() ->
            [macos] Wybierz…
           *[other] Przeglądaj…
        }
    .accesskey =
        { PLATFORM() ->
            [macos] y
           *[other] g
        }
download-always-ask-where =
    .label = Pytaj, gdzie zapisać każdy plik
    .accesskey = t
applications-header = Aplikacje
applications-description = Wybierz, jak { -brand-short-name } będzie obsługiwać pobierane z sieci pliki i aplikacje używane podczas przeglądania.
applications-filter =
    .placeholder = Typ pliku lub nazwa aplikacji
applications-type-column =
    .label = Typ zawartości
    .accesskey = T
applications-action-column =
    .label = Czynność
    .accesskey = C
# Variables:
#   $extension (String) - file extension (e.g .TXT)
applications-file-ending = plik { $extension }
applications-action-save =
    .label = Zapisz plik
# Variables:
#   $app-name (String) - Name of an application (e.g Adobe Acrobat)
applications-use-app =
    .label = Użyj aplikacji { $app-name }
# Variables:
#   $app-name (String) - Name of an application (e.g Adobe Acrobat)
applications-use-app-default =
    .label = Użyj aplikacji { $app-name } (domyślnej)
applications-use-os-default =
    .label =
        { PLATFORM() ->
            [macos] Użyj domyślnej aplikacji systemu macOS
            [windows] Użyj domyślnej aplikacji systemu Windows
           *[other] Użyj domyślnej aplikacji systemu
        }
applications-use-other =
    .label = Użyj innej aplikacji…
applications-select-helper = Wybierz aplikację pomocniczą
applications-manage-app =
    .label = Szczegóły aplikacji…
applications-always-ask =
    .label = Zawsze pytaj
applications-type-pdf = Dokument PDF
# Variables:
#   $type (String) - the MIME type (e.g application/binary)
applications-type-pdf-with-type = { applications-type-pdf } ({ $type })
# Variables:
#   $type-description (String) - Description of the type (e.g "Portable Document Format")
#   $type (String) - the MIME type (e.g application/binary)
applications-type-description-with-type = { $type-description } ({ $type })
# Variables:
#   $extension (String) - file extension (e.g .TXT)
#   $type (String) - the MIME type (e.g application/binary)
applications-file-ending-with-type = { applications-file-ending } ({ $type })
# Variables:
#   $plugin-name (String) - Name of a plugin (e.g Adobe Flash)
applications-use-plugin-in =
    .label = Wtyczka { $plugin-name } (w programie { -brand-short-name })
applications-open-inapp =
    .label = Otwórz w programie { -brand-short-name }

## The strings in this group are used to populate
## selected label element based on the string from
## the selected menu item.

applications-use-plugin-in-label =
    .value = { applications-use-plugin-in.label }
applications-action-save-label =
    .value = { applications-action-save.label }
applications-use-app-label =
    .value = { applications-use-app.label }
applications-open-inapp-label =
    .value = { applications-open-inapp.label }
applications-always-ask-label =
    .value = { applications-always-ask.label }
applications-use-app-default-label =
    .value = { applications-use-app-default.label }
applications-use-other-label =
    .value = { applications-use-other.label }
applications-use-os-default-label =
    .value = { applications-use-os-default.label }

##

drm-content-header = Treści DRM (Digital Rights Management)
play-drm-content =
    .label = Odtwarzanie treści chronionych przez DRM.
    .accesskey = O
play-drm-content-learn-more = Więcej informacji
update-application-title = Aktualizacje przeglądarki { -brand-short-name }
update-application-description = Utrzymuj przeglądarkę { -brand-short-name } aktualną dla najlepszej wydajności, stabilności i bezpieczeństwa.
update-application-version = Wersja: { $version }. <a data-l10n-name="learn-more">Informacje o wydaniu</a>.
update-history =
    .label = Wyświetl historię aktualizacji…
    .accesskey = W
update-application-allow-description = Zezwalaj przeglądarce { -brand-short-name } na:
update-application-auto =
    .label = automatyczne instalowanie aktualizacji (zalecane)
    .accesskey = a
update-application-check-choose =
    .label = sprawdzanie dostępności aktualizacji i pytania o ich instalację
    .accesskey = s
update-application-manual =
    .label = niesprawdzanie dostępności aktualizacji (niezalecane)
    .accesskey = n
update-application-warning-cross-user-setting = To ustawienie będzie obowiązywać dla wszystkich kont systemu Windows i profilów programu { -brand-short-name } używających tej instalacji.
update-application-use-service =
    .label = Używaj usługi instalowania aktualizacji działającej w tle
    .accesskey = U
update-setting-write-failure-title = Błąd podczas zachowywania preferencji aktualizacji
# Variables:
#   $path (String) - Path to the configuration file
# The newlines between the main text and the line containing the path is
# intentional so the path is easier to identify.
update-setting-write-failure-message =
    W przeglądarce { -brand-short-name } wystąpił błąd i nie zachowano tej zmiany. Zauważ, że ustawienie tej preferencji aktualizacji wymaga uprawnienia do zapisu do poniższego pliku. Ty lub administrator komputera może móc rozwiązać błąd przez udzielenie grupie „Użytkownicy” pełnej kontroli nad tym plikiem.
    
    Nie można zapisać do pliku: { $path }
update-in-progress-title = Trwa aktualizacja
update-in-progress-message = Czy { -brand-short-name } ma kontynuować tę aktualizację?
update-in-progress-ok-button = &Odrzuć
# Continue is the cancel button so pressing escape or using a platform standard
# method of closing the UI will not discard the update.
update-in-progress-cancel-button = &Kontynuuj

## General Section - Performance

performance-title = Wydajność
performance-use-recommended-settings-checkbox =
    .label = Zalecane ustawienia wydajności.
    .accesskey = w
performance-use-recommended-settings-desc = Ustawienia te są specjalnie dostosowane do specyfikacji tego komputera i systemu operacyjnego.
performance-settings-learn-more = Więcej informacji
performance-allow-hw-accel =
    .label = Korzystaj ze sprzętowego przyspieszania, jeśli jest dostępne
    .accesskey = s
performance-limit-content-process-option = Limit liczby procesów treści:
    .accesskey = o
performance-limit-content-process-enabled-desc = Więcej procesów treści może poprawić wydajność przy wielu otwartych kartach, ale zwiększy też zapotrzebowanie na pamięć.
performance-limit-content-process-blocked-desc = Zmiana liczby procesów jest możliwa, jeśli { -brand-short-name } został uruchomiony z obsługą wielu procesów. <a data-l10n-name="learn-more">Jak sprawdzić, czy obsługa wielu procesów jest włączona</a>?
# Variables:
#   $num - default value of the `dom.ipc.processCount` pref.
performance-default-content-process-count =
    .label = { $num } (domyślnie)

## General Section - Browsing

browsing-title = Przeglądanie
browsing-use-autoscroll =
    .label = Używaj automatycznego przewijania
    .accesskey = y
browsing-use-smooth-scrolling =
    .label = Używaj płynnego przewijania
    .accesskey = n
browsing-use-onscreen-keyboard =
    .label = Wyświetlaj klawiaturę ekranową, gdy zachodzi taka potrzeba
    .accesskey = e
browsing-use-cursor-navigation =
    .label = Zawsze używaj klawiszy kursora do nawigacji po stronach
    .accesskey = g
browsing-search-on-start-typing =
    .label = Rozpoczynaj wyszukiwanie podczas wpisywania tekstu
    .accesskey = R
browsing-picture-in-picture-toggle-enabled =
    .label = Wyświetlaj przycisk trybu „Obraz w obrazie”.
    .accesskey = O
browsing-picture-in-picture-learn-more = Więcej informacji
browsing-media-control =
    .label = Sterowanie multimediami za pomocą klawiatury, zestawu słuchawkowego lub interfejsu wirtualnego.
    .accesskey = m
browsing-media-control-learn-more = Więcej informacji
browsing-cfr-recommendations =
    .label = Polecaj rozszerzenia podczas przeglądania.
    .accesskey = P
browsing-cfr-features =
    .label = Polecaj funkcje podczas przeglądania.
    .accesskey = u
browsing-cfr-recommendations-learn-more = Więcej informacji

## General Section - Proxy

network-settings-title = Sieć
network-proxy-connection-description = Konfiguruj, jak { -brand-short-name } ma się łączyć z Internetem.
network-proxy-connection-learn-more = Więcej informacji
network-proxy-connection-settings =
    .label = Ustawienia…
    .accesskey = U

## Home Section

home-new-windows-tabs-header = Nowe okna i karty
home-new-windows-tabs-description2 = Wybierz, co ma być wyświetlane przy otwieraniu strony startowej, nowych okien i kart.

## Home Section - Home Page Customization

home-homepage-mode-label = Strona startowa i nowe okna:
home-newtabs-mode-label = Nowa karta:
home-restore-defaults =
    .label = Przywróć domyślne
    .accesskey = P
# "Firefox" should be treated as a brand and kept in English,
# while "Home" and "(Default)" can be localized.
home-mode-choice-default =
    .label = strona startowa Firefoksa (domyślnie)
home-mode-choice-custom =
    .label = inne strony
home-mode-choice-blank =
    .label = pusta strona
home-homepage-custom-url =
    .placeholder = Adres URL
# This string has a special case for '1' and [other] (default). If necessary for
# your language, you can add {$tabCount} to your translations and use the
# standard CLDR forms, or only use the form for [other] if both strings should
# be identical.
use-current-pages =
    .label =
        { $tabCount ->
            [1] Użyj bieżącej strony
           *[other] Użyj bieżących stron
        }
    .accesskey = b
choose-bookmark =
    .label = Użyj zakładki…
    .accesskey = z

## Home Section - Firefox Home Content Customization

home-prefs-content-header = Strona startowa Firefoksa
home-prefs-content-description = Wybierz, co wyświetlać na stronie startowej Firefoksa.
home-prefs-search-header =
    .label = Pasek wyszukiwania
home-prefs-topsites-header =
    .label = Popularne
home-prefs-topsites-description = Najczęściej odwiedzane strony.
home-prefs-topsites-by-option-sponsored =
    .label = Sponsorowane popularne witryny

## Variables:
##  $provider (String): Name of the corresponding content provider, e.g "Pocket".

home-prefs-recommended-by-header =
    .label = Polecane przez { $provider }
home-prefs-recommended-by-description-update = Wyjątkowe rzeczy z całego Internetu, wybrane przez { $provider }

##

home-prefs-recommended-by-learn-more = Jak to działa?
home-prefs-recommended-by-option-sponsored-stories =
    .label = Sponsorowane artykuły
home-prefs-highlights-header =
    .label = Wyróżnione
home-prefs-recent-activity-header =
    .label = Ostatnia aktywność
home-prefs-highlights-description = Wybierane z zachowanych i odwiedzonych stron.
home-prefs-highlights-option-visited-pages =
    .label = Historia
home-prefs-highlights-options-bookmarks =
    .label = Zakładki
home-prefs-highlights-option-most-recent-download =
    .label = Ostatnio pobrane pliki
home-prefs-highlights-option-saved-to-pocket =
    .label = Zachowane w { -pocket-brand-name }
# For the "Snippets" feature traditionally on about:home.
# Alternative translation options: "Small Note" or something that
# expresses the idea of "a small message, shortened from something else,
# and non-essential but also not entirely trivial and useless.
home-prefs-snippets-header =
    .label = Od Mozilli
home-prefs-snippets-description = Informacje od organizacji { -vendor-short-name } i przeglądarki { -brand-product-name }.
home-prefs-sections-rows-option =
    .label =
        { $num ->
            [one] { $num } wiersz
            [few] { $num } wiersze
           *[many] { $num } wierszy
        }

## Search Section

search-bar-header = Pasek wyszukiwania
search-bar-hidden =
    .label = Pasek adresu z funkcjami wyszukiwania i nawigacji
search-bar-shown =
    .label = Osobny pasek wyszukiwania
search-engine-default-header = Domyślna wyszukiwarka
search-engine-default-desc-2 = To domyślna wyszukiwarka paska adresu i paska wyszukiwania. Można ją zmienić w każdej chwili.
search-engine-default-private-desc-2 = Wybierz inną domyślną wyszukiwarkę tylko w oknach prywatnych.
search-separate-default-engine =
    .label = Używaj tej wyszukiwarki w oknach prywatnych
    .accesskey = U
search-suggestions-header = Podpowiedzi wyszukiwania
search-suggestions-desc = Wybierz, jak wyświetlać podpowiedzi od wyszukiwarek.
search-suggestions-option =
    .label = Podpowiedzi wyszukiwania
    .accesskey = P
search-show-suggestions-url-bar-option =
    .label = Podpowiedzi wyszukiwania w wynikach paska adresu
    .accesskey = e
# This string describes what the user will observe when the system
# prioritizes search suggestions over browsing history in the results
# that extend down from the address bar. In the original English string,
# "ahead" refers to location (appearing most proximate to), not time
# (appearing before).
search-show-suggestions-above-history-option =
    .label = Podpowiedzi wyszukiwania nad historią przeglądania w wynikach paska adresu
search-show-suggestions-private-windows =
    .label = Podpowiedzi wyszukiwania w prywatnych oknach
suggestions-addressbar-settings-generic = Zmień preferencje innych podpowiedzi w pasku adresu
search-suggestions-cant-show = Podpowiedzi wyszukiwania nie będą wyświetlane w wynikach paska adresu, ponieważ wyłączono zachowywanie historii przeglądania programu { -brand-short-name }.
search-one-click-header = Dodatkowe wyszukiwarki
search-one-click-header2 = Skróty wyszukiwania
search-one-click-desc = Wybierz dodatkowe wyszukiwarki wyświetlane na dole wyników wyszukiwania w pasku adresu i pasku wyszukiwania.
search-choose-engine-column =
    .label = Nazwa
search-choose-keyword-column =
    .label = Słowo kluczowe
search-restore-default =
    .label = Przywróć domyślne
    .accesskey = d
search-remove-engine =
    .label = Usuń
    .accesskey = U
search-add-engine =
    .label = Dodaj
    .accesskey = o
search-find-more-link = Znajdź więcej wyszukiwarek
# This warning is displayed when the chosen keyword is already in use
# ('Duplicate' is an adjective)
search-keyword-warning-title = Słowo kluczowe już istnieje
# Variables:
#   $name (String) - Name of a search engine.
search-keyword-warning-engine = Wybrano słowo kluczowe używane obecnie przez wyszukiwarkę { $name }. Należy wybrać inne.
search-keyword-warning-bookmark = Wybrano słowo kluczowe używane obecnie przez zakładkę. Należy wybrać inne.

## Containers Section

containers-back-button =
    .aria-label =
        { PLATFORM() ->
            [windows] Wróć do opcji
           *[other] Wróć do preferencji
        }
containers-header = Karty z kontekstem
containers-add-button =
    .label = Dodaj kontekst
    .accesskey = D
containers-new-tab-check =
    .label = Wybieraj kontekst dla każdej nowej karty
    .accesskey = W
containers-preferences-button =
    .label = Preferencje
containers-remove-button =
    .label = Usuń

## Firefox Account - Signed out. Note that "Sync" and "Firefox Account" are now
## more discrete ("signed in" no longer means "and sync is connected").

sync-signedout-caption = Zabierz swoją sieć ze sobą
sync-signedout-description = Synchronizuj zakładki, historię, karty, hasła, dodatki i preferencje między wszystkimi swoimi urządzeniami.
sync-signedout-account-signin2 =
    .label = Zaloguj się do { -sync-brand-short-name(case: "gen", capitalization: "lower") }…
    .accesskey = Z
# This message contains two links and two icon images.
#   `<img data-l10n-name="android-icon"/>` - Android logo icon
#   `<a data-l10n-name="android-link">` - Link to Android Download
#   `<img data-l10n-name="ios-icon">` - iOS logo icon
#   `<a data-l10n-name="ios-link">` - Link to iOS Download
#
# They can be moved within the sentence as needed to adapt
# to your language, but should not be changed or translated.
sync-mobile-promo = Firefox na <a data-l10n-name="android-link">Androida</a> <img data-l10n-name="android-icon"/> i <a data-l10n-name="ios-link">iOS</a> <img data-l10n-name="ios-icon"/> daje możliwość synchronizacji z urządzeniami przenośnymi.

## Firefox Account - Signed in

sync-profile-picture =
    .tooltiptext = Zmień obraz przypisany do konta
sync-sign-out =
    .label = Wyloguj się…
    .accesskey = W
sync-manage-account = Zarządzaj kontem
    .accesskey = Z
sync-signedin-unverified = Konto { $email } nie zostało zweryfikowane.
sync-signedin-login-failure = Zaloguj się, aby ponownie połączyć konto { $email }
sync-resend-verification =
    .label = Wyślij nową wiadomość weryfikującą
    .accesskey = W
sync-remove-account =
    .label = Usuń konto
    .accesskey = U
sync-sign-in =
    .label = Zaloguj się
    .accesskey = o

## Sync section - enabling or disabling sync.

prefs-syncing-on = Synchronizowanie: włączone
prefs-syncing-off = Synchronizowanie: wyłączone
prefs-sync-setup =
    .label = Skonfiguruj { -sync-brand-short-name(case: "acc", capitalization: "lower") }…
    .accesskey = S
prefs-sync-offer-setup-label = Synchronizuj zakładki, historię, karty, hasła, dodatki i preferencje między wszystkimi swoimi urządzeniami.
prefs-sync-now =
    .labelnotsyncing = Synchronizuj teraz
    .accesskeynotsyncing = S
    .labelsyncing = Synchronizowanie…

## The list of things currently syncing.

sync-currently-syncing-heading = Obecnie synchronizowane:
sync-currently-syncing-bookmarks = zakładki
sync-currently-syncing-history = historia
sync-currently-syncing-tabs = otwarte karty
sync-currently-syncing-logins-passwords = dane logowania i hasła
sync-currently-syncing-addresses = adresy
sync-currently-syncing-creditcards = karty płatnicze
sync-currently-syncing-addons = dodatki
sync-currently-syncing-prefs =
    { PLATFORM() ->
        [windows] opcje
       *[other] preferencje
    }
sync-change-options =
    .label = Zmień…
    .accesskey = m

## The "Choose what to sync" dialog.

sync-choose-what-to-sync-dialog =
    .title = Wybierz, co synchronizować
    .style = width: 36em; min-height: 35em;
    .buttonlabelaccept = Zapisz zmiany
    .buttonaccesskeyaccept = Z
    .buttonlabelextra2 = Rozłącz…
    .buttonaccesskeyextra2 = R
sync-engine-bookmarks =
    .label = zakładki
    .accesskey = z
sync-engine-history =
    .label = historia
    .accesskey = h
sync-engine-tabs =
    .label = karty
    .tooltiptext = Lista otwartych stron na wszystkich synchronizowanych urządzeniach
    .accesskey = k
sync-engine-logins-passwords =
    .label = dane logowania i hasła
    .tooltiptext = Zachowane nazwy użytkownika i hasła
    .accesskey = l
sync-engine-addresses =
    .label = adresy
    .tooltiptext = Zachowane adresy pocztowe (tylko na komputerach)
    .accesskey = a
sync-engine-creditcards =
    .label = dane kart płatniczych
    .tooltiptext = Nazwiska, numery i okresy ważności (tylko na komputerach)
    .accesskey = d
sync-engine-addons =
    .label = dodatki
    .tooltiptext = Rozszerzenia i motywy w wersji na komputery
    .accesskey = d
sync-engine-prefs =
    .label =
        { PLATFORM() ->
            [windows] opcje
           *[other] preferencje
        }
    .tooltiptext = Zmienione ustawienia ogólne, uruchamiania, wyszukiwania, prywatności i bezpieczeństwa
    .accesskey = e

## The device name controls.

sync-device-name-header = Nazwa urządzenia
sync-device-name-change =
    .label = Zmień nazwę urządzenia…
    .accesskey = n
sync-device-name-cancel =
    .label = Anuluj
    .accesskey = A
sync-device-name-save =
    .label = Zachowaj
    .accesskey = Z
sync-connect-another-device = Połącz inne urządzenie

## Privacy Section

privacy-header = Prywatność

## Privacy Section - Logins and Passwords

# The search keyword isn't shown to users but is used to find relevant settings in about:preferences.
pane-privacy-logins-and-passwords-header = Dane logowania i hasła
    .searchkeywords = { -lockwise-brand-short-name }
# Checkbox to control whether UI is shown to users to save or fill logins/passwords.
forms-ask-to-save-logins =
    .label = Pytanie o zachowywanie danych logowania do witryn
    .accesskey = P
forms-exceptions =
    .label = Wyjątki…
    .accesskey = i
forms-generate-passwords =
    .label = Proponowanie i generowanie silnych haseł
    .accesskey = s
forms-breach-alerts =
    .label = Powiadomienia o hasłach do stron, z których wyciekły dane.
    .accesskey = o
forms-breach-alerts-learn-more-link = Więcej informacji
# Checkbox which controls filling saved logins into fields automatically when they appear, in some cases without user interaction.
forms-fill-logins-and-passwords =
    .label = Automatyczne wypełnianie formularzy logowania
    .accesskey = A
forms-saved-logins =
    .label = Zachowane dane logowania…
    .accesskey = d
forms-master-pw-use =
    .label = Hasło główne
    .accesskey = H
forms-primary-pw-use =
    .label = Hasło główne.
    .accesskey = H
forms-primary-pw-learn-more-link = Więcej informacji
# This string uses the former name of the Primary Password feature
# ("Master Password" in English) so that the preferences can be found
# when searching for the old name. The accesskey is unused.
forms-master-pw-change =
    .label = Zmień hasło główne…
    .accesskey = Z
forms-master-pw-fips-title = Program pracuje obecnie w trybie FIPS. Tryb FIPS wymaga niepustego hasła głównego.
forms-primary-pw-change =
    .label = Zmień hasło główne…
    .accesskey = Z
# Leave this message empty if the translation for "Primary Password" matches
# "Master Password" in your language. If you're editing the FTL file directly,
# use { "" } as the value.
forms-primary-pw-former-name = { "" }
forms-primary-pw-fips-title = Program pracuje obecnie w trybie FIPS. Tryb FIPS wymaga niepustego hasła głównego.
forms-master-pw-fips-desc = Zmiana hasła się nie powiodła.

## OS Authentication dialog

# This message can be seen by trying to add a Master Password.
master-password-os-auth-dialog-message-win = Aby utworzyć hasło główne, wprowadź swoje dane logowania do systemu Windows. Pomaga to chronić bezpieczeństwo Twoich kont.
# This message can be seen by trying to add a Master Password.
# The macOS strings are preceded by the operating system with "Firefox is trying to "
# and includes subtitle of "Enter password for the user "xxx" to allow this." These
# notes are only valid for English. Please test in your locale.
master-password-os-auth-dialog-message-macosx = utworzenie hasła głównego
# This message can be seen by trying to add a Primary Password.
primary-password-os-auth-dialog-message-win = Aby utworzyć hasło główne, wprowadź swoje dane logowania do systemu Windows. Pomaga to chronić bezpieczeństwo Twoich kont.
# This message can be seen by trying to add a Primary Password.
# The macOS strings are preceded by the operating system with "Firefox is trying to "
# and includes subtitle of "Enter password for the user "xxx" to allow this." These
# notes are only valid for English. Please test in your locale.
primary-password-os-auth-dialog-message-macosx = utworzenie hasła głównego
master-password-os-auth-dialog-caption = { -brand-full-name }

## Privacy Section - History

history-header = Historia
# This label is followed, on the same line, by a dropdown list of options
# (Remember history, etc.).
# In English it visually creates a full sentence, e.g.
# "Firefox will" + "Remember history".
#
# If this doesn't work for your language, you can translate this message:
#   - Simply as "Firefox", moving the verb into each option.
#     This will result in "Firefox" + "Will remember history", etc.
#   - As a stand-alone message, for example "Firefox history settings:".
history-remember-label = Program { -brand-short-name }:
    .accesskey = m
history-remember-option-all =
    .label = będzie zachowywał historię
history-remember-option-never =
    .label = nie będzie zachowywał historii
history-remember-option-custom =
    .label = będzie używał ustawień użytkownika
history-remember-description = { -brand-short-name } zachowa historię przeglądania, wyszukiwania, pobieranych plików i danych formularzy.
history-dontremember-description = { -brand-short-name } będzie używał tych samych ustawień co w trybie prywatnym i nie będzie zachowywał historii podczas przeglądania witryn WWW.
history-private-browsing-permanent =
    .label = Zawsze aktywny tryb przeglądania prywatnego
    .accesskey = t
history-remember-browser-option =
    .label = Zachowywanie historii przeglądania i pobierania plików
    .accesskey = h
history-remember-search-option =
    .label = Zachowywanie historii pola wyszukiwania i formularzy
    .accesskey = o
history-clear-on-close-option =
    .label = Czyszczenie historii podczas zamykania przeglądarki { -brand-short-name }
    .accesskey = z
history-clear-on-close-settings =
    .label = Ustawienia…
    .accesskey = U
history-clear-button =
    .label = Wyczyść historię…
    .accesskey = W

## Privacy Section - Site Data

sitedata-header = Ciasteczka i dane stron
sitedata-total-size-calculating = Obliczanie rozmiaru danych i pamięci podręcznej stron…
# Variables:
#   $value (Number) - Value of the unit (for example: 4.6, 500)
#   $unit (String) - Name of the unit (for example: "bytes", "KB")
sitedata-total-size = Przechowywane ciasteczka, dane i pamięć podręczna stron zajmują { $value } { $unit } na dysku.
sitedata-learn-more = Więcej informacji
sitedata-delete-on-close =
    .label = Usuwanie ciasteczek i danych stron podczas zamykania przeglądarki { -brand-short-name }
    .accesskey = U
sitedata-delete-on-close-private-browsing = W stale aktywnym trybie przeglądania prywatnego ciasteczka i dane stron są zawsze usuwane podczas zamykania programu { -brand-short-name }.
sitedata-allow-cookies-option =
    .label = Akceptowanie ciasteczek i danych stron
    .accesskey = A
sitedata-disallow-cookies-option =
    .label = Blokowanie ciasteczek i danych stron
    .accesskey = B
# This label means 'type of content that is blocked', and is followed by a drop-down list with content types below.
# The list items are the strings named sitedata-block-*-option*.
sitedata-block-desc = Blokowanie:
    .accesskey = B
sitedata-option-block-cross-site-trackers =
    .label = śledzące między witrynami
sitedata-option-block-cross-site-and-social-media-trackers =
    .label = śledzące między witrynami i serwisów społecznościowych
sitedata-option-block-cross-site-tracking-cookies-including-social-media =
    .label = ciasteczka śledzące między witrynami — w tym ciasteczka serwisów społecznościowych
sitedata-option-block-cross-site-cookies-including-social-media =
    .label = ciasteczka między witrynami — w tym ciasteczka serwisów społecznościowych
sitedata-option-block-cross-site-and-social-media-trackers-plus-isolate =
    .label = śledzące między witrynami i serwisów społecznościowych oraz izolowanie pozostałych ciasteczek
sitedata-option-block-unvisited =
    .label = nieodwiedzonych witryn
sitedata-option-block-all-third-party =
    .label = wszystkie zewnętrznych witryn (może powodować problemy)
sitedata-option-block-all =
    .label = wszystkie (powoduje problemy)
sitedata-clear =
    .label = Wyczyść dane…
    .accesskey = a
sitedata-settings =
    .label = Zachowane dane…
    .accesskey = c
sitedata-cookies-permissions =
    .label = Wyjątki…
    .accesskey = W
sitedata-cookies-exceptions =
    .label = Wyjątki…
    .accesskey = W

## Privacy Section - Address Bar

addressbar-header = Pasek adresu
addressbar-suggest = Podpowiedzi w pasku adresu opieraj na:
addressbar-locbar-history-option =
    .label = historii przeglądania
    .accesskey = h
addressbar-locbar-bookmarks-option =
    .label = zakładkach
    .accesskey = z
addressbar-locbar-openpage-option =
    .label = otwartych kartach
    .accesskey = k
addressbar-locbar-topsites-option =
    .label = popularnych witrynach
    .accesskey = w
addressbar-suggestions-settings = Zmień preferencje podpowiedzi dostarczanych przez wyszukiwarki

## Privacy Section - Content Blocking

content-blocking-enhanced-tracking-protection = Wzmocniona ochrona przed śledzeniem
content-blocking-section-top-level-description = Elementy śledzące monitorują Cię w Internecie, zbierając informacje o Twoich działaniach i zainteresowaniach. { -brand-short-name } blokuje wiele tych elementów i inne złośliwe skrypty.
content-blocking-learn-more = Więcej informacji
content-blocking-fpi-incompatibility-warning = Używasz funkcji FPI („First Party Isolation”), która zastępuje część ustawień ciasteczek przeglądarki { -brand-short-name }.

## These strings are used to define the different levels of
## Enhanced Tracking Protection.

# "Standard" in this case is an adjective, meaning "default" or "normal".
enhanced-tracking-protection-setting-standard =
    .label = Standardowa
    .accesskey = S
enhanced-tracking-protection-setting-strict =
    .label = Ścisła
    .accesskey = c
enhanced-tracking-protection-setting-custom =
    .label = Własna
    .accesskey = W

##

content-blocking-etp-standard-desc = Równowaga między bezpieczeństwem a szybkością wczytywania stron. Strony będą działać bez problemów.
content-blocking-etp-strict-desc = Silniejsza ochrona, ale może powodować niepoprawne działanie niektórych stron.
content-blocking-etp-custom-desc = Wybierz, które elementy śledzące i skrypty blokować:
content-blocking-private-windows = treści z elementami śledzącymi w oknach prywatnych
content-blocking-cross-site-cookies = ciasteczka między witrynami
content-blocking-cross-site-tracking-cookies = ciasteczka śledzące między witrynami
content-blocking-cross-site-tracking-cookies-plus-isolate = ciasteczka śledzące między witrynami i izolowanie pozostałych
content-blocking-social-media-trackers = elementy śledzące serwisów społecznościowych
content-blocking-all-cookies = wszystkie ciasteczka
content-blocking-unvisited-cookies = ciasteczka z nieodwiedzonych witryn
content-blocking-all-windows-tracking-content = treści z elementami śledzącymi we wszystkich oknach
content-blocking-all-third-party-cookies = wszystkie ciasteczka zewnętrznych witryn
content-blocking-cryptominers = elementy używające komputera użytkownika do generowania kryptowalut
content-blocking-fingerprinters = elementy śledzące przez zbieranie informacji o konfiguracji
content-blocking-warning-title = Ostrzeżenie
content-blocking-and-isolating-etp-warning-description = Blokowanie elementów śledzących i izolowanie ciasteczek może wpłynąć na funkcjonowanie niektórych stron. Odśwież stronę z włączonymi elementami śledzącymi, aby wyświetlić całą jej zawartość.
content-blocking-and-isolating-etp-warning-description-2 = To ustawienie może spowodować niepoprawne działanie lub wyświetlanie niektórych stron. Jeśli dana strona wydaje się niewłaściwie działać, możesz wyłączyć dla niej ochronę przed śledzeniem, aby wczytać ją w całości.
content-blocking-warning-learn-how = Więcej informacji
content-blocking-reload-description = Zastosowanie tych zmian wymaga odświeżenia kart.
content-blocking-reload-tabs-button =
    .label = Odśwież wszystkie karty
    .accesskey = O
content-blocking-tracking-content-label =
    .label = treści z elementami śledzącymi:
    .accesskey = e
content-blocking-tracking-protection-option-all-windows =
    .label = zawsze
    .accesskey = z
content-blocking-option-private =
    .label = w oknach prywatnych
    .accesskey = w
content-blocking-tracking-protection-change-block-list = Zmień listę blokowanych
content-blocking-cookies-label =
    .label = ciasteczka:
    .accesskey = c
content-blocking-expand-section =
    .tooltiptext = Więcej informacji
# Cryptomining refers to using scripts on websites that can use a computer’s resources to mine cryptocurrency without a user’s knowledge.
content-blocking-cryptominers-label =
    .label = elementy używające komputera użytkownika do generowania kryptowalut
    .accesskey = e
# Browser fingerprinting is a method of tracking users by the configuration and settings information (their "digital fingerprint")
# that is visible to websites they browse, rather than traditional tracking methods such as IP addresses and unique cookies.
content-blocking-fingerprinters-label =
    .label = elementy śledzące przez zbieranie informacji o konfiguracji
    .accesskey = k

## Privacy Section - Tracking

tracking-manage-exceptions =
    .label = Wyjątki…
    .accesskey = W

## Privacy Section - Permissions

permissions-header = Uprawnienia
permissions-location = Położenie
permissions-location-settings =
    .label = Ustawienia…
    .accesskey = t
permissions-xr = Rzeczywistość wirtualna
permissions-xr-settings =
    .label = Ustawienia…
    .accesskey = e
permissions-camera = Kamera
permissions-camera-settings =
    .label = Ustawienia…
    .accesskey = a
permissions-microphone = Mikrofon
permissions-microphone-settings =
    .label = Ustawienia…
    .accesskey = w
permissions-notification = Powiadomienia.
permissions-notification-settings =
    .label = Ustawienia…
    .accesskey = s
permissions-notification-link = Więcej informacji
permissions-notification-pause =
    .label = Wstrzymaj powiadomienia do czasu ponownego uruchomienia przeglądarki { -brand-short-name }
    .accesskey = W
permissions-autoplay = Automatyczne odtwarzanie
permissions-autoplay-settings =
    .label = Ustawienia…
    .accesskey = n
permissions-block-popups =
    .label = Blokowanie wyskakujących okien
    .accesskey = B
permissions-block-popups-exceptions =
    .label = Wyjątki…
    .accesskey = t
permissions-addon-install-warning =
    .label = Ostrzeganie, gdy witryny próbują instalować dodatki
    .accesskey = O
permissions-addon-exceptions =
    .label = Wyjątki…
    .accesskey = W
permissions-a11y-privacy-checkbox =
    .label = Blokowanie dostępu do przeglądarki usługom ułatwień dostępu.
    .accesskey = u
permissions-a11y-privacy-link = Więcej informacji

## Privacy Section - Data Collection

collection-header = Dane zbierane przez program { -brand-short-name }
collection-description = Dążymy do zapewnienia odpowiedniego wyboru i zbierania wyłącznie niezbędnych danych, aby dostarczać i doskonalić program { -brand-short-name } dla nas wszystkich. Zawsze prosimy o pozwolenie przed przesłaniem danych osobistych.
collection-privacy-notice = Prywatność
collection-health-report-telemetry-disabled = { -vendor-short-name } nie ma już zezwolenia na zbieranie danych technicznych i o interakcjach z przeglądarką. Wszystkie wcześniej zebrane dane zostaną usunięte w ciągu 30 dni.
collection-health-report-telemetry-disabled-link = Więcej informacji
collection-health-report =
    .label = Przesyłanie do organizacji { -vendor-short-name } danych technicznych i o interakcjach z przeglądarką { -brand-short-name }.
    .accesskey = z
collection-health-report-link = Więcej informacji
collection-studies =
    .label = Instalowanie i przeprowadzanie badań przez przeglądarkę { -brand-short-name }.
collection-studies-link = Wyświetl badania przeglądarki { -brand-short-name }
addon-recommendations =
    .label = Personalizowane polecenia rozszerzeń przez przeglądarkę { -brand-short-name }.
addon-recommendations-link = Więcej informacji
# This message is displayed above disabled data sharing options in developer builds
# or builds with no Telemetry support available.
collection-health-report-disabled = Przesyłanie danych jest wyłączone przy tej konfiguracji programu
collection-backlogged-crash-reports =
    .label = Przesyłanie zgromadzonych zgłoszeń awarii przeglądarki { -brand-short-name }.
    .accesskey = o
collection-backlogged-crash-reports-link = Więcej informacji

## Privacy Section - Security
##
## It is important that wording follows the guidelines outlined on this page:
## https://developers.google.com/safe-browsing/developers_guide_v2#AcceptableUsage

security-header = Bezpieczeństwo
security-browsing-protection = Ochrona przed oszustwami i niebezpiecznym oprogramowaniem
security-enable-safe-browsing =
    .label = Blokowanie niebezpiecznych i podejrzanych treści.
    .accesskey = B
security-enable-safe-browsing-link = Więcej informacji
security-block-downloads =
    .label = Blokowanie możliwości pobierania niebezpiecznych plików
    .accesskey = e
security-block-uncommon-software =
    .label = Ostrzeganie przed niepożądanym i nietypowym oprogramowaniem
    .accesskey = n

## Privacy Section - Certificates

certs-header = Certyfikaty
certs-personal-label = Kiedy serwer żąda osobistego certyfikatu użytkownika:
certs-select-auto-option =
    .label = wybierz certyfikat automatycznie
    .accesskey = a
certs-select-ask-option =
    .label = pytaj za każdym razem
    .accesskey = r
certs-enable-ocsp =
    .label = Odpytywanie serwerów OCSP w celu potwierdzenia wiarygodności certyfikatów
    .accesskey = O
certs-view =
    .label = Wyświetl certyfikaty…
    .accesskey = W
certs-devices =
    .label = Urządzenia zabezpieczające…
    .accesskey = U
space-alert-learn-more-button =
    .label = Więcej informacji
    .accesskey = W
space-alert-over-5gb-pref-button =
    .label =
        { PLATFORM() ->
            [windows] Otwórz opcje
           *[other] Otwórz preferencje
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] O
        }
space-alert-over-5gb-message =
    { PLATFORM() ->
        [windows] Przeglądarce { -brand-short-name } zaczyna brakować miejsca na dysku. Zawartość stron może być wyświetlana niepoprawnie. Przechowywane dane może wyczyścić w Opcje → Prywatność i bezpieczeństwo → Ciasteczka i dane stron.
       *[other] Przeglądarce { -brand-short-name } zaczyna brakować miejsca na dysku. Zawartość stron może być wyświetlana niepoprawnie. Przechowywane dane może wyczyścić w Preferencje → Prywatność i bezpieczeństwo → Ciasteczka i dane stron.
    }
space-alert-under-5gb-ok-button =
    .label = OK
    .accesskey = O
space-alert-under-5gb-message = Przeglądarce { -brand-short-name } zaczyna brakować miejsca na dysku. Zawartość stron może być wyświetlana niepoprawnie. Skorzystaj z odnośnika „Więcej informacji”, aby zoptymalizować użycie dysku dla lepszego przeglądania.

## Privacy Section - HTTPS-Only

httpsonly-header = Tryb używania wyłącznie protokołu HTTPS
httpsonly-description = Protokół HTTPS zapewnia zabezpieczone, zaszyfrowane połączenie między przeglądarką { -brand-short-name } a odwiedzanymi witrynami. Większość witryn obsługuje HTTPS, a jeśli tryb używania wyłącznie protokołu HTTPS jest włączony, to { -brand-short-name } przełączy wszystkie połączenia na HTTPS.
httpsonly-learn-more = Więcej informacji
httpsonly-radio-enabled =
    .label = Tryb używania wyłącznie protokołu HTTPS we wszystkich oknach
httpsonly-radio-enabled-pbm =
    .label = Tryb używania wyłącznie protokołu HTTPS tylko w oknach prywatnych
httpsonly-radio-disabled =
    .label = Nie włączaj trybu używania wyłącznie protokołu HTTPS

## The following strings are used in the Download section of settings

desktop-folder-name = Pulpit
downloads-folder-name = Pobrane
choose-download-folder-title = Wybór folderu dla pobieranych plików
# Variables:
#   $service-name (String) - Name of a cloud storage provider like Dropbox, Google Drive, etc...
save-files-to-cloud-storage =
    .label = Wysyłanie plików do usługi { $service-name }
