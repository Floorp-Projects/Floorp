# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-masonry2 =
    .label = CSS: układ typu „Masonry”
experimental-features-css-masonry-description = Włącza obsługę eksperymentalnego układu CSS typu „Masonry”. Ta <a data-l10n-name="explainer">strona</a> zawiera jego ogólny opis. W <a data-l10n-name="w3c-issue">tym zgłoszeniu w serwisie GitHub</a> lub <a data-l10n-name="bug">tym błędzie</a> można dodać komentarz na jego temat.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-gpu2 =
    .label = API internetowe: WebGPU
experimental-features-web-gpu-description2 = To nowe API dostarcza niskopoziomową obsługę wykonywania obliczeń i renderowania grafiki za pomocą <a data-l10n-name="wikipedia">procesora graficznego (GPU)</a> urządzenia lub komputera użytkownika. <a data-l10n-name="spec">Specyfikacja</a> jest nadal w trakcie przygotowywania. <a data-l10n-name="bugzilla">Zgłoszenie 1602129</a> zawiera więcej informacji.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-media-avif =
    .label = Multimedia: AVIF
experimental-features-media-avif-description = Po włączeniu tej funkcji { -brand-short-name } obsługuje format obrazów AV1 (AVIF). Jest to format dla nieruchomych obrazów wykorzystujący możliwości algorytmów kompresji wideo AV1 w celu zmniejszenia rozmiaru pliku. <a data-l10n-name="bugzilla">Zgłoszenie 1443863</a> zawiera więcej informacji.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-api-inputmode =
    .label = API internetowe: inputmode
# "inputmode" and "contenteditable" are technical terms and shouldn't be translated.
experimental-features-web-api-inputmode-description = Nasza implementacja globalnego atrybutu <a data-l10n-name="mdn-inputmode">inputmode</a> została zaktualizowana zgodnie ze <a data-l10n-name="whatwg">specyfikacją WHATWG</a>, ale nadal musimy wprowadzić także inne zmiany, na przykład umożliwić działanie w treściach „contenteditable”. <a data-l10n-name="bugzilla">Zgłoszenie 1205133</a> zawiera więcej informacji.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-api-link-preload =
    .label = API internetowe: <link rel="preload">
# Do not translate "rel", "preload" or "link" here, as they are all HTML spec
# values that do not get translated.
experimental-features-web-api-link-preload-description = Atrybut <a data-l10n-name="rel">rel</a> o wartości <code>"preload"</code> na elemencie <a data-l10n-name="link">&lt;link&gt;</a> ma na celu pomóc zwiększyć wydajność przez umożliwienie użytkownikowi pobrania zasobów wcześniej w cyklu życia strony, zapewniając, że są one dostępne wcześniej i rzadziej blokują wyświetlanie strony. Artykuł <a data-l10n-name="readmore">„Preloading content with <code>rel="preload"</code>”</a> i <a data-l10n-name="bugzilla">zgłoszenie 1583604</a> zawierają więcej informacji.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-focus-visible =
    .label = CSS: pseudoklasa „:focus-visible”
experimental-features-css-focus-visible-description = Umożliwia stosowanie stylów aktywacji na elementach typu przyciski i formularze tylko wtedy, gdy zostały aktywowane za pomocą klawiatury (np. podczas przełączania między elementami klawiszem Tab), a nie gdy zostały aktywowane za pomocą myszy lub innego urządzenia wskazującego. <a data-l10n-name="bugzilla">Zgłoszenie 1617600</a> zawiera więcej informacji.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-web-api-beforeinput =
    .label = API internetowe: zdarzenie „beforeinput”
# The terms "beforeinput", "input", "textarea", and "contenteditable" are technical terms
# and shouldn't be translated.
experimental-features-web-api-beforeinput-description = Globalne zdarzenie <a data-l10n-name="mdn-beforeinput">beforeinput</a> jest wywoływane na elementach <a data-l10n-name="mdn-input">&lt;input&gt;</a> i <a data-l10n-name="mdn-textarea">&lt;textarea&gt;</a> lub dowolnym elemencie, którego atrybut <a data-l10n-name="mdn-contenteditable">contenteditable</a> jest włączony, natychmiast przed zmianą wartości elementu. To zdarzenie umożliwia aplikacjom internetowym zastępowanie domyślnego zachowania przeglądarki podczas działań użytkownika, np. aplikacje internetowe mogą anulować wprowadzane dane przez użytkownika tylko dla określonych znaków lub mogą modyfikować wklejanie tekstu ze stylem tylko za pomocą zatwierdzonych stylów.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-css-constructable-stylesheets =
    .label = CSS: arkusze stylów za pomocą konstruktora
experimental-features-css-constructable-stylesheets-description = Dodanie konstruktora do interfejsu <a data-l10n-name="mdn-cssstylesheet">CSSStyleSheet</a>, a także szereg powiązanych zmian umożliwia bezpośrednie tworzenie nowych arkuszy stylów bez konieczności dodawania arkusza do kodu HTML. Znacznie ułatwia to tworzenie arkuszy stylów wielokrotnego użytku do użycia za pomocą <a data-l10n-name="mdn-shadowdom">Shadow DOM</a>. <a data-l10n-name="bugzilla">Zgłoszenie 1520690</a> zawiera więcej informacji.
experimental-features-devtools-color-scheme-simulation =
    .label = Narzędzia dla programistów: symulacja schematu kolorów
experimental-features-devtools-color-scheme-simulation-description = Dodaje opcję symulowania różnych schematów kolorów, umożliwiając testowanie zapytań <a data-l10n-name="mdn-preferscolorscheme">@prefers-color-scheme</a>. Użycie tego zapytania umożliwia arkuszowi stylów reagowanie na to, czy użytkownik preferuje jasny lub ciemny interfejs. Ta funkcja umożliwia testowanie kodu bez konieczności zmiany ustawień w przeglądarce (lub systemie operacyjnym, jeśli przeglądarka używa systemowego ustawienia schematu kolorów). Zgłoszenia <a data-l10n-name="bugzilla1">1550804</a> i <a data-l10n-name="bugzilla2">1137699</a> zawierają więcej informacji.
experimental-features-devtools-execution-context-selector =
    .label = Narzędzia dla programistów: wybór kontekstu wykonywania
experimental-features-devtools-execution-context-selector-description = Ta funkcja wyświetla przycisk w wierszu poleceń konsoli, umożliwiający zmianę kontekstu, w którym wprowadzane wyrażenie będzie wykonywane. Zgłoszenia <a data-l10n-name="bugzilla1">1605154</a> i <a data-l10n-name="bugzilla2">1605153</a> zawierają więcej informacji.
experimental-features-devtools-compatibility-panel =
    .label = Narzędzia dla programistów: panel zgodności
experimental-features-devtools-compatibility-panel-description = Panel boczny inspektora stron, wyświetlający informacje o stanie zgodności aplikacji z różnymi przeglądarkami. <a data-l10n-name="bugzilla">Zgłoszenie 1584464</a> zawiera więcej informacji.
# Do not translate 'SameSite', 'Lax' and 'None'.
experimental-features-cookie-samesite-lax-by-default2 =
    .label = Ciasteczka: „SameSite=Lax” jest domyślne
experimental-features-cookie-samesite-lax-by-default2-description = Domyślnie traktuje ciasteczka jako „SameSite=Lax”, jeśli nie określono żadnego atrybutu „SameSite”. Deweloperzy muszą wyrazić zgodę na obecne status quo nieograniczonego użytkowania, bezpośrednio ustawiając „SameSite=None”.
# Do not translate 'SameSite', 'Lax' and 'None'.
experimental-features-cookie-samesite-none-requires-secure2 =
    .label = Ciasteczka: „SameSite=None” wymaga atrybutu bezpieczeństwa
experimental-features-cookie-samesite-none-requires-secure2-description = Ciasteczka z atrybutem „SameSite=None” wymagają atrybutu bezpieczeństwa. Ta funkcja wymaga włączenia „Ciasteczka: »SameSite=Lax« jest domyślne”.
# about:home should be kept in English, as it refers to the the URI for
# the internal default home page.
experimental-features-abouthome-startup-cache =
    .label = Pamięć podręczna uruchamiania about:home
experimental-features-abouthome-startup-cache-description = Pamięć podręczna dla początkowego dokumentu about:home, który jest domyślnie wczytywany podczas uruchamiania. Celem tej pamięci podręcznej jest przyspieszenie uruchamiania.
experimental-features-print-preview-tab-modal =
    .label = Przeprojektowany podgląd wydruku
experimental-features-print-preview-tab-modal-description = Włącza przeprojektowany podgląd wydruku i udostępnia go w systemie macOS. Może nie działać i nie zawiera wszystkich ustawień związanych z drukowaniem. Kliknij „Drukuj za pomocą okna systemowego…” w panelu drukowania, aby móc korzystać ze wszystkich ustawień.
# The title of the experiment should be kept in English as it may be referenced
# by various online articles and is technical in nature.
experimental-features-cookie-samesite-schemeful =
    .label = Ciasteczka: SameSite typu „Schemeful”
experimental-features-cookie-samesite-schemeful-description = Traktuje ciasteczka z tej samej domeny, ale o różnych protokołach (np. http://example.com i https://example.com) jako ciasteczka między witrynami, zamiast z tej samej witryny. Zwiększa bezpieczeństwo, ale potencjalnie zakłóca działanie witryn.
# "Service Worker" is an API name and is usually not translated.
experimental-features-devtools-serviceworker-debugger-support =
    .label = Narzędzia dla programistów: debugowanie wątków usługowych
# "Service Worker" is an API name and is usually not translated.
experimental-features-devtools-serviceworker-debugger-support-description = Włącza eksperymentalną obsługę wątków usługowych w panelu debugera. Ta funkcja może spowolnić narzędzia dla programistów i zwiększyć zużycie pamięci.
# WebRTC global mute toggle controls
experimental-features-webrtc-global-mute-toggles =
    .label = Przełączniki globalnego wyciszania WebRTC
experimental-features-webrtc-global-mute-toggles-description = Dodaje elementy sterujące do globalnego wskaźnika udostępniania WebRTC umożliwiające użytkownikom globalne wyciszanie transmisji dźwięku z mikrofonu i obrazu z kamery.
# JS JIT Warp project
experimental-features-js-warp =
    .label = JavaScript JIT: Warp
experimental-features-js-warp-description = Włącza Warp, projekt mający na celu zwiększenie wydajności JavaScriptu i zmniejszenie zużycia pamięci.
# Fission is the name of the feature and should not be translated.
experimental-features-fission =
    .label = Fission (izolacja witryn)
experimental-features-fission-description = Fission (izolacja witryn) to eksperymentalna funkcja programu { -brand-short-name }, zapewniająca dodatkową warstwę obrony przed błędami zabezpieczeń. Izolując każdą witrynę w oddzielnym procesie, Fission utrudnia złośliwym witrynom dostęp do informacji z pozostałych odwiedzanych stron. To duża zmiana architektury programu { -brand-short-name } i serdecznie dziękujemy za testowanie i zgłaszanie napotkanych błędów. <a data-l10n-name="wiki">Strona wiki</a> zawiera więcej informacji.
# Support for having multiple Picture-in-Picture windows open simultaneously
experimental-features-multi-pip =
    .label = Obsługa wielu okien „Obraz w obrazie”
experimental-features-multi-pip-description = Eksperymentalna obsługa otwierania wielu okien „Obraz w obrazie” jednocześnie.
