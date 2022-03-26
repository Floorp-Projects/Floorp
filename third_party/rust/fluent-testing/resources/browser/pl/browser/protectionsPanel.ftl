# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

protections-panel-sendreportview-error = Wystąpił błąd podczas wysyłania zgłoszenia. Proszę spróbować ponownie później.

# A link shown when ETP is disabled for a site. Opens the breakage report subview when clicked.
protections-panel-sitefixedsendreport-label = Naprawiło to stronę? Wyślij zgłoszenie

## These strings are used to define the different levels of
## Enhanced Tracking Protection.

protections-popup-footer-protection-label-strict = Ścisła
    .label = Ścisła
protections-popup-footer-protection-label-custom = Własna
    .label = Własna
protections-popup-footer-protection-label-standard = Standardowa
    .label = Standardowa

##

# The text a screen reader speaks when focused on the info button.
protections-panel-etp-more-info =
    .aria-label = Więcej informacji o wzmocnionej ochronie przed śledzeniem

protections-panel-etp-on-header = Wzmocniona ochrona przed śledzeniem jest włączona na tej witrynie
protections-panel-etp-off-header = Wzmocniona ochrona przed śledzeniem jest wyłączona na tej witrynie

# The link to be clicked to open the sub-panel view
protections-panel-site-not-working = Strona nie działa?

# The heading/title of the sub-panel view
protections-panel-site-not-working-view =
    .title = Strona nie działa?

## The "Allowed" header also includes a "Why?" link that, when hovered, shows
## a tooltip explaining why these items were not blocked in the page.

protections-panel-not-blocking-why-label = Dlaczego?
protections-panel-not-blocking-why-etp-on-tooltip = Blokowanie tych elementów może powodować niepoprawne działanie niektórych stron. Bez elementów śledzących niektóre przyciski, formularze i pola logowania mogą nie działać.
protections-panel-not-blocking-why-etp-off-tooltip = Wszystkie elementy śledzące na tej stronie zostały wczytane, ponieważ ochrona jest wyłączona.

##

protections-panel-no-trackers-found = { -brand-short-name } nie wykrył na tej stronie znanych elementów śledzących.

protections-panel-content-blocking-tracking-protection = Treści z elementami śledzącymi

protections-panel-content-blocking-socialblock = Elementy śledzące serwisów społecznościowych
protections-panel-content-blocking-cryptominers-label = Elementy używające komputera użytkownika do generowania kryptowalut
protections-panel-content-blocking-fingerprinters-label = Elementy śledzące przez zbieranie informacji o konfiguracji

## In the protections panel, Content Blocking category items are in three sections:
##   "Blocked" for categories being blocked in the current page,
##   "Allowed" for categories detected but not blocked in the current page, and
##   "None Detected" for categories not detected in the current page.
##   These strings are used in the header labels of each of these sections.

protections-panel-blocking-label = Zablokowane
protections-panel-not-blocking-label = Dopuszczone
protections-panel-not-found-label = Niewykryte

##

protections-panel-settings-label = Ustawienia ochrony
# This should match the "appmenuitem-protection-dashboard-title" string in browser/appmenu.ftl.
protections-panel-protectionsdashboard-label = Panel ochrony

## In the Site Not Working? view, we suggest turning off protections if
## the user is experiencing issues with any of a variety of functionality.

# The header of the list
protections-panel-site-not-working-view-header = Wyłącz ochronę, jeśli masz problemy z:

# The list items, shown in a <ul>
protections-panel-site-not-working-view-issue-list-login-fields = polami logowania
protections-panel-site-not-working-view-issue-list-forms = formularzami
protections-panel-site-not-working-view-issue-list-payments = płatnościami
protections-panel-site-not-working-view-issue-list-comments = komentarzami
protections-panel-site-not-working-view-issue-list-videos = filmami

protections-panel-site-not-working-view-send-report = Wyślij zgłoszenie

##

protections-panel-cross-site-tracking-cookies = Te ciasteczka śledzą Cię od strony do strony w celu zbierania danych o tym, co robisz w Internecie. Są umieszczane przez zewnętrzne firmy, takie jak agencje reklamowe i firmy analityczne.
protections-panel-cryptominers = Te elementy wykorzystują moc obliczeniową Twojego komputera do generowania cyfrowych walut. Skrypty generujące kryptowaluty rozładowują baterię, spowalniają komputer i mogą zwiększyć rachunek za prąd.
protections-panel-fingerprinters = Te elementy zbierają ustawienia przeglądarki i komputera, aby utworzyć profil użytkownika. Za pomocą tego cyfrowego odcisku palca mogą śledzić Cię między różnymi witrynami.
protections-panel-tracking-content = Witryny mogą wczytywać zewnętrzne reklamy, filmy i inne treści z elementami śledzącymi. Blokowanie ich może przyspieszyć wczytywanie stron, ale niektóre przyciski, formularze i pola logowania mogą działać niepoprawnie.
protections-panel-social-media-trackers = Serwisy społecznościowe umieszczają elementy śledzące na innych witrynach, aby śledzić co robisz, widzisz i oglądasz w Internecie. Dzięki temu ich właściciele wiedzą o Tobie więcej, niż udostępniasz w ich serwisach.

protections-panel-content-blocking-manage-settings =
    .label = Zarządzaj ustawieniami ochrony
    .accesskey = Z

protections-panel-content-blocking-breakage-report-view =
    .title = Zgłoś niepoprawnie działającą stronę
protections-panel-content-blocking-breakage-report-view-description = Blokowanie pewnych elementów śledzących może powodować problemy z niektórymi stronami. Zgłaszając problemy, pomagasz ulepszać program { -brand-short-name } (adres odwiedzanej strony oraz informacje o ustawieniach przeglądarki zostaną przesłane do Mozilli). <label data-l10n-name="learn-more">Więcej informacji</label>
protections-panel-content-blocking-breakage-report-view-collection-url = Adres URL problematycznej strony
protections-panel-content-blocking-breakage-report-view-collection-url-label =
    .aria-label = Adres URL problematycznej strony
protections-panel-content-blocking-breakage-report-view-collection-comments = Opcjonalnie: opisz problem
protections-panel-content-blocking-breakage-report-view-collection-comments-label =
    .aria-label = Opcjonalnie: opisz problem
protections-panel-content-blocking-breakage-report-view-cancel =
    .label = Anuluj
protections-panel-content-blocking-breakage-report-view-send-report =
    .label = Wyślij zgłoszenie
