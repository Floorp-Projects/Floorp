######
Raptor
######

The following documents all testing we have for Raptor.

Benchmarks
----------
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI. 


Desktop
-------
Tests for page-load performance. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, FF: Firefox, CH: Chrome, CU: Chromium)

.. dropdown:: amazon (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: apple (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.apple.com/macbook-pro/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bing-search (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.ebay.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook-redesign (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: fandom (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-docs (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-mail (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-search (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-sheets (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/spreadsheets/d/1jT9qfZFAeqNoOK97gruc34Zb7y_Q-O_drZ8kSXT-4D4/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-slides (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: imdb (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: imgur (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: instagram (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: linkedin (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: microsoft (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: netflix (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.netflix.com/title/80117263>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: office (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-live-office.manifest
   * **playback recordings**: mitm5-linux-firefox-live-office.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://office.live.com/start/Word.aspx?omkt=en-US>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: outlook (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback recordings**: mitm5-linux-firefox-live.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://outlook.live.com/mail/inbox>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: paypal (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: pinterest (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: raptor-tp6-amazon (WX, FF, CH, CU)

   1. **raptor-tp6-amazon-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **playback recordings**: mitm5-linux-firefox-amazon.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__

   2. **raptor-tp6-amazon-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **playback recordings**: mitm5-linux-firefox-amazon.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__

   3. **raptor-tp6-amazon-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **playback recordings**: mitm5-linux-firefox-amazon.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__


.. dropdown:: raptor-tp6-apple (WX, FF, CH, CU)

   1. **raptor-tp6-apple-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-apple.manifest
   * **playback recordings**: mitm5-linux-firefox-apple.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.apple.com/macbook-pro/>`__

   2. **raptor-tp6-apple-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-apple.manifest
   * **playback recordings**: mitm5-linux-firefox-apple.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.apple.com/macbook-pro/>`__

   3. **raptor-tp6-apple-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-apple.manifest
   * **playback recordings**: mitm5-linux-firefox-apple.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.apple.com/macbook-pro/>`__


.. dropdown:: raptor-tp6-bing (WX, FF, CH, CU)

   1. **raptor-tp6-bing-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback recordings**: mitm5-linux-firefox-bing-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__

   2. **raptor-tp6-bing-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback recordings**: mitm5-linux-firefox-bing-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__

   3. **raptor-tp6-bing-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback recordings**: mitm5-linux-firefox-bing-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__


.. dropdown:: raptor-tp6-cnn-ampstories (WX, FF)

   1. **raptor-tp6-cnn-ampstories-firefox**

   * **alert on**: fnbpaint, fcp, dcf, loadtime
   * **alert threshold**: 5.0
   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__


.. dropdown:: raptor-tp6-docs (WX, FF, CH, CU)

   1. **raptor-tp6-docs-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-docs.manifest
   * **playback recordings**: mitm5-linux-firefox-google-docs.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__

   2. **raptor-tp6-docs-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-docs.manifest
   * **playback recordings**: mitm5-linux-firefox-google-docs.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__

   3. **raptor-tp6-docs-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-docs.manifest
   * **playback recordings**: mitm5-linux-firefox-google-docs.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__


.. dropdown:: raptor-tp6-ebay (WX, FF, CH, CU)

   1. **raptor-tp6-ebay-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-ebay.manifest
   * **playback recordings**: mitm5-linux-firefox-ebay.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.ebay.com/>`__

   2. **raptor-tp6-ebay-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-ebay.manifest
   * **playback recordings**: mitm5-linux-firefox-ebay.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.ebay.com/>`__

   3. **raptor-tp6-ebay-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-ebay.manifest
   * **playback recordings**: mitm5-linux-firefox-ebay.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.ebay.com/>`__


.. dropdown:: raptor-tp6-facebook (WX, CH, CU)

   1. **raptor-tp6-facebook-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **playback recordings**: mitm5-linux-firefox-facebook.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__

   2. **raptor-tp6-facebook-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **playback recordings**: mitm5-linux-firefox-facebook.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__

   3. **raptor-tp6-facebook-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **playback recordings**: mitm5-linux-firefox-facebook.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__


.. dropdown:: raptor-tp6-fandom (WX, FF, CH, CU)

   1. **raptor-tp6-fandom-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback recordings**: mitm5-linux-firefox-fandom.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__

   2. **raptor-tp6-fandom-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback recordings**: mitm5-linux-firefox-fandom.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__

   3. **raptor-tp6-fandom-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback recordings**: mitm5-linux-firefox-fandom.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__


.. dropdown:: raptor-tp6-google (WX, FF, CH, CU)

   1. **raptor-tp6-google-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback recordings**: mitm5-linux-firefox-google-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__

   2. **raptor-tp6-google-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback recordings**: mitm5-linux-firefox-google-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__

   3. **raptor-tp6-google-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback recordings**: mitm5-linux-firefox-google-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__


.. dropdown:: raptor-tp6-google-mail (WX, FF, CH, CU)

   1. **raptor-tp6-google-mail-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-google-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__

   2. **raptor-tp6-google-mail-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-google-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__

   3. **raptor-tp6-google-mail-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-google-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__


.. dropdown:: raptor-tp6-imdb (WX, FF, CH, CU)

   1. **raptor-tp6-imdb-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imdb.manifest
   * **playback recordings**: mitm5-linux-firefox-imdb.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__

   2. **raptor-tp6-imdb-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imdb.manifest
   * **playback recordings**: mitm5-linux-firefox-imdb.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__

   3. **raptor-tp6-imdb-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imdb.manifest
   * **playback recordings**: mitm5-linux-firefox-imdb.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__


.. dropdown:: raptor-tp6-imgur (WX, FF, CH, CU)

   1. **raptor-tp6-imgur-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imgur.manifest
   * **playback recordings**: mitm5-linux-firefox-imgur.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__

   2. **raptor-tp6-imgur-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imgur.manifest
   * **playback recordings**: mitm5-linux-firefox-imgur.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__

   3. **raptor-tp6-imgur-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-imgur.manifest
   * **playback recordings**: mitm5-linux-firefox-imgur.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__


.. dropdown:: raptor-tp6-instagram (WX, FF, CH, CU)

   1. **raptor-tp6-instagram-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-instagram.manifest
   * **playback recordings**: mitm5-linux-firefox-instagram.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__

   2. **raptor-tp6-instagram-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-instagram.manifest
   * **playback recordings**: mitm5-linux-firefox-instagram.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__

   3. **raptor-tp6-instagram-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-instagram.manifest
   * **playback recordings**: mitm5-linux-firefox-instagram.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__


.. dropdown:: raptor-tp6-linkedin (WX, FF, CH, CU)

   1. **raptor-tp6-linkedin-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-linkedin.manifest
   * **playback recordings**: mitm5-linux-firefox-linkedin.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__

   2. **raptor-tp6-linkedin-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-linkedin.manifest
   * **playback recordings**: mitm5-linux-firefox-linkedin.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__

   3. **raptor-tp6-linkedin-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-linkedin.manifest
   * **playback recordings**: mitm5-linux-firefox-linkedin.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__


.. dropdown:: raptor-tp6-microsoft (WX, FF, CH, CU)

   1. **raptor-tp6-microsoft-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-microsoft.manifest
   * **playback recordings**: mitm5-linux-firefox-microsoft.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__

   2. **raptor-tp6-microsoft-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-microsoft.manifest
   * **playback recordings**: mitm5-linux-firefox-microsoft.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__

   3. **raptor-tp6-microsoft-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-microsoft.manifest
   * **playback recordings**: mitm5-linux-firefox-microsoft.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__


.. dropdown:: raptor-tp6-netflix (WX, FF, CH, CU)

   1. **raptor-tp6-netflix-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-netflix.manifest
   * **playback recordings**: mitm5-linux-firefox-netflix.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.netflix.com/title/80117263>`__

   2. **raptor-tp6-netflix-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-netflix.manifest
   * **playback recordings**: mitm5-linux-firefox-netflix.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.netflix.com/title/80117263>`__

   3. **raptor-tp6-netflix-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-netflix.manifest
   * **playback recordings**: mitm5-linux-firefox-netflix.mp
   * **playback version**: 5.1.1
   * **preferences**: {"media.autoplay.default": 1,
"media.eme.enabled": true}
   * **test url**: `<https://www.netflix.com/title/80117263>`__


.. dropdown:: raptor-tp6-office (WX, FF, CH, CU)

   1. **raptor-tp6-office-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live-office.manifest
   * **playback recordings**: mitm5-linux-firefox-live-office.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://office.live.com/start/Word.aspx?omkt=en-US>`__

   2. **raptor-tp6-office-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live-office.manifest
   * **playback recordings**: mitm5-linux-firefox-live-office.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://office.live.com/start/Word.aspx?omkt=en-US>`__

   3. **raptor-tp6-office-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live-office.manifest
   * **playback recordings**: mitm5-linux-firefox-live-office.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://office.live.com/start/Word.aspx?omkt=en-US>`__


.. dropdown:: raptor-tp6-outlook (WX, FF, CH, CU)

   1. **raptor-tp6-outlook-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback recordings**: mitm5-linux-firefox-live.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://outlook.live.com/mail/inbox>`__

   2. **raptor-tp6-outlook-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback recordings**: mitm5-linux-firefox-live.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://outlook.live.com/mail/inbox>`__

   3. **raptor-tp6-outlook-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-live.manifest
   * **playback recordings**: mitm5-linux-firefox-live.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://outlook.live.com/mail/inbox>`__


.. dropdown:: raptor-tp6-paypal (WX, FF, CH, CU)

   1. **raptor-tp6-paypal-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback recordings**: mitm5-linux-firefox-paypal.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__

   2. **raptor-tp6-paypal-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback recordings**: mitm5-linux-firefox-paypal.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__

   3. **raptor-tp6-paypal-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback recordings**: mitm5-linux-firefox-paypal.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__


.. dropdown:: raptor-tp6-pinterest (WX, FF, CH, CU)

   1. **raptor-tp6-pinterest-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-pinterest.manifest
   * **playback recordings**: mitm5-linux-firefox-pinterest.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__

   2. **raptor-tp6-pinterest-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-pinterest.manifest
   * **playback recordings**: mitm5-linux-firefox-pinterest.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__

   3. **raptor-tp6-pinterest-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-pinterest.manifest
   * **playback recordings**: mitm5-linux-firefox-pinterest.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__


.. dropdown:: raptor-tp6-reddit (WX, FF, CH, CU)

   1. **raptor-tp6-reddit-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-reddit.manifest
   * **playback recordings**: mitm5-linux-firefox-reddit.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__

   2. **raptor-tp6-reddit-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-reddit.manifest
   * **playback recordings**: mitm5-linux-firefox-reddit.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__

   3. **raptor-tp6-reddit-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-reddit.manifest
   * **playback recordings**: mitm5-linux-firefox-reddit.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__


.. dropdown:: raptor-tp6-sheets (WX, FF, CH, CU)

   1. **raptor-tp6-sheets-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-sheets.manifest
   * **playback recordings**: mitm5-linux-firefox-google-sheets.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/spreadsheets/d/1jT9qfZFAeqNoOK97gruc34Zb7y_Q-O_drZ8kSXT-4D4/edit?usp=sharing>`__

   2. **raptor-tp6-sheets-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-sheets.manifest
   * **playback recordings**: mitm5-linux-firefox-google-sheets.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/spreadsheets/d/1jT9qfZFAeqNoOK97gruc34Zb7y_Q-O_drZ8kSXT-4D4/edit?usp=sharing>`__

   3. **raptor-tp6-sheets-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-sheets.manifest
   * **playback recordings**: mitm5-linux-firefox-google-sheets.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/spreadsheets/d/1jT9qfZFAeqNoOK97gruc34Zb7y_Q-O_drZ8kSXT-4D4/edit?usp=sharing>`__


.. dropdown:: raptor-tp6-slides (WX, FF, CH, CU)

   1. **raptor-tp6-slides-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-slides.manifest
   * **playback recordings**: mitm5-linux-firefox-google-slides.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__

   2. **raptor-tp6-slides-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-slides.manifest
   * **playback recordings**: mitm5-linux-firefox-google-slides.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__

   3. **raptor-tp6-slides-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-google-slides.manifest
   * **playback recordings**: mitm5-linux-firefox-google-slides.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__


.. dropdown:: raptor-tp6-tumblr (WX, FF, CH, CU)

   1. **raptor-tp6-tumblr-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-tumblr.manifest
   * **playback recordings**: mitm5-linux-firefox-tumblr.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__

   2. **raptor-tp6-tumblr-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-tumblr.manifest
   * **playback recordings**: mitm5-linux-firefox-tumblr.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__

   3. **raptor-tp6-tumblr-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-tumblr.manifest
   * **playback recordings**: mitm5-linux-firefox-tumblr.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__


.. dropdown:: raptor-tp6-twitch (WX, FF, CH, CU)

   1. **raptor-tp6-twitch-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitch.manifest
   * **playback recordings**: mitm5-linux-firefox-twitch.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__

   2. **raptor-tp6-twitch-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitch.manifest
   * **playback recordings**: mitm5-linux-firefox-twitch.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__

   3. **raptor-tp6-twitch-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitch.manifest
   * **playback recordings**: mitm5-linux-firefox-twitch.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__


.. dropdown:: raptor-tp6-twitter (WX, FF, CH, CU)

   1. **raptor-tp6-twitter-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback recordings**: mitm5-linux-firefox-twitter.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__

   2. **raptor-tp6-twitter-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback recordings**: mitm5-linux-firefox-twitter.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__

   3. **raptor-tp6-twitter-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback recordings**: mitm5-linux-firefox-twitter.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__


.. dropdown:: raptor-tp6-wikipedia (WX, FF, CH, CU)

   1. **raptor-tp6-wikipedia-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-wikipedia.manifest
   * **playback recordings**: mitm5-linux-firefox-wikipedia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__

   2. **raptor-tp6-wikipedia-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-wikipedia.manifest
   * **playback recordings**: mitm5-linux-firefox-wikipedia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__

   3. **raptor-tp6-wikipedia-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-wikipedia.manifest
   * **playback recordings**: mitm5-linux-firefox-wikipedia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__


.. dropdown:: raptor-tp6-yahoo-mail (WX, FF, CH, CU)

   1. **raptor-tp6-yahoo-mail-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__

   2. **raptor-tp6-yahoo-mail-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__

   3. **raptor-tp6-yahoo-mail-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__


.. dropdown:: raptor-tp6-yahoo-news (WX, FF, CH, CU)

   1. **raptor-tp6-yahoo-news-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-news.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-news.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.yahoo.com/lifestyle/police-respond-noise-complaint-end-playing-video-games-respectful-tenants-002329963.html>`__

   2. **raptor-tp6-yahoo-news-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-news.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-news.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.yahoo.com/lifestyle/police-respond-noise-complaint-end-playing-video-games-respectful-tenants-002329963.html>`__

   3. **raptor-tp6-yahoo-news-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-news.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-news.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.yahoo.com/lifestyle/police-respond-noise-complaint-end-playing-video-games-respectful-tenants-002329963.html>`__


.. dropdown:: raptor-tp6-yandex (WX, FF, CH, CU)

   1. **raptor-tp6-yandex-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yandex.manifest
   * **playback recordings**: mitm5-linux-firefox-yandex.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://yandex.ru/search/?text=barack%20obama&lr=10115>`__

   2. **raptor-tp6-yandex-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yandex.manifest
   * **playback recordings**: mitm5-linux-firefox-yandex.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://yandex.ru/search/?text=barack%20obama&lr=10115>`__

   3. **raptor-tp6-yandex-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-yandex.manifest
   * **playback recordings**: mitm5-linux-firefox-yandex.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://yandex.ru/search/?text=barack%20obama&lr=10115>`__


.. dropdown:: raptor-tp6-youtube (WX, FF, CH, CU)

   1. **raptor-tp6-youtube-chrome**

   * **apps**: chrome
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **playback recordings**: mitm5-linux-firefox-youtube.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__

   2. **raptor-tp6-youtube-chromium**

   * **apps**: chromium
   * **expected**: pass
   * **measure**: fcp, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **playback recordings**: mitm5-linux-firefox-youtube.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__

   3. **raptor-tp6-youtube-firefox**

   * **apps**: firefox
   * **expected**: pass
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **playback recordings**: mitm5-linux-firefox-youtube.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__


.. dropdown:: reddit (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: tumblr (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: twitch (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: twitter (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: wikipedia (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: yahoo-mail (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: yahoo-news (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.yahoo.com/lifestyle/police-respond-noise-complaint-end-playing-video-games-respectful-tenants-002329963.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: yandex (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://yandex.ru/search/?text=barack%20obama&lr=10115>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: youtube (BT, FF, CH, CU)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-{subtest}.manifest
   * **playback recordings**: mitm5-linux-firefox-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false



Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms.


Mobile
------
Page-load performance test suite on Android. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, GV: Geckoview, RB: Refbrow, FE: Fenix, F68: Fennec68, CH-M: Chrome mobile)

.. dropdown:: allrecipes (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.allrecipes.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: amazon (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.amazon.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: amazon-search (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.amazon.com/s/ref=nb_sb_noss_2/139-6317191-5622045?url=search-alias%3Daps&field-keywords=mobile+phone>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bbc (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.bbc.com/news/business-47245877>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bing (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.bing.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bing-search-restaurants (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.bing.com/search?q=restaurants>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: booking (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.booking.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: cnn (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://cnn.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: cnn-ampstories (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay-kleinanzeigen (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.ebay-kleinanzeigen.de>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay-kleinanzeigen-search (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.ebay-kleinanzeigen.de/s-anzeigen/auf-zeit-wg-berlin/zimmer/c199-l3331>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: espn (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook-cristiano (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.facebook.com/Cristiano>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.google.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-maps (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm5-motog5-gve-{subtest}.manifest
   * **playback recordings**: mitm5-motog5-gve-{subtest}.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/maps?force=pwa>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-search-restaurants (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.google.com/search?q=restaurants+near+me>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: imdb (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.imdb.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: instagram (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.instagram.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: jianshu (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.jianshu.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: microsoft-support (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://support.microsoft.com/en-us>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: reddit (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.reddit.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: stackoverflow (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://stackoverflow.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: web-de (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://web.de/magazine/politik/politologe-glaubt-grossen-koalition-herbst-knallen-33563566>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: wikipedia (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://en.m.wikipedia.org/wiki/Main_Page>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: youtube (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://m.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: youtube-watch (BT, GV, FE, RB, F68, CH-M)

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, fennec, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **test url**: `<https://www.youtube.com/watch?v=COU5T-Wafa4>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false



Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc.


Unittests
---------
These tests aren't used in standard testing, they are only used in the Raptor unit tests (they are similar to raptor-tp6 tests though).



The methods for calling the tests can be found in the `Raptor wiki page <https://wiki.mozilla.org/TestEngineering/Performance/Raptor>`_.
