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
   :container: + anchor-id-amazon-d

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


.. dropdown:: bing-search (BT, FF, CH, CU)
   :container: + anchor-id-bing-search-d

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


.. dropdown:: buzzfeed (BT, FF, CH, CU)
   :container: + anchor-id-buzzfeed-d

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
   * **test url**: `<https://www.buzzfeed.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: cnn (BT, FF, CH, CU)
   :container: + anchor-id-cnn-d

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
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay (BT, FF, CH, CU)
   :container: + anchor-id-ebay-d

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


.. dropdown:: espn (BT, FF, CH, CU)
   :container: + anchor-id-espn-d

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
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: expedia (BT, FF, CH, CU)
   :container: + anchor-id-expedia-d

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
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook (BT, FF, CH, CU)
   :container: + anchor-id-facebook-d

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
   :container: + anchor-id-fandom-d

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
   :container: + anchor-id-google-docs-d

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
   :container: + anchor-id-google-mail-d

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
   :container: + anchor-id-google-search-d

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


.. dropdown:: google-slides (BT, FF, CH, CU)
   :container: + anchor-id-google-slides-d

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
   :container: + anchor-id-imdb-d

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
   :container: + anchor-id-imgur-d

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
   :container: + anchor-id-instagram-d

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
   :container: + anchor-id-linkedin-d

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
   :container: + anchor-id-microsoft-d

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
   :container: + anchor-id-netflix-d

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


.. dropdown:: nytimes (BT, FF, CH, CU)
   :container: + anchor-id-nytimes-d

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
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: office (BT, FF, CH, CU)
   :container: + anchor-id-office-d

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
   :container: + anchor-id-outlook-d

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
   :container: + anchor-id-paypal-d

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
   :container: + anchor-id-pinterest-d

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


.. dropdown:: reddit (BT, FF, CH, CU)
   :container: + anchor-id-reddit-d

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
   :container: + anchor-id-tumblr-d

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
   :container: + anchor-id-twitch-d

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
   :container: + anchor-id-twitter-d

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


.. dropdown:: wikia (BT, FF, CH, CU)
   :container: + anchor-id-wikia-d

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
   * **test url**: `<https://marvel.fandom.com/wiki/Black_Panther>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: wikipedia (BT, FF, CH, CU)
   :container: + anchor-id-wikipedia-d

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
   :container: + anchor-id-yahoo-mail-d

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


.. dropdown:: youtube (BT, FF, CH, CU)
   :container: + anchor-id-youtube-d

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
Page-load performance test suite on Android. The links direct to the actual websites that are being tested. (WX: WebExtension, BT: Browsertime, GV: Geckoview, RB: Refbrow, FE: Fenix, CH-M: Chrome mobile)

.. dropdown:: allrecipes (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-allrecipes-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.allrecipes.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: amazon (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-amazon-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: amazon-search (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-amazon-search-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com/s/ref=nb_sb_noss_2/139-6317191-5622045?url=search-alias%3Daps&field-keywords=mobile+phone>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bbc (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-bbc-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bbc.com/news/business-47245877>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bing (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-bing-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: bing-search-restaurants (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-bing-search-restaurants-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/search?q=restaurants>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: booking (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-booking-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.booking.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: cnn (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-cnn-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: cnn-ampstories (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-cnn-ampstories-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay-kleinanzeigen (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-ebay-kleinanzeigen-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: ebay-kleinanzeigen-search (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-ebay-kleinanzeigen-search-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de/s-anzeigen/auf-zeit-wg-berlin/zimmer/c199-l3331>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: espn (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-espn-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-facebook-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-g5-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: facebook-cristiano (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-facebook-cristiano-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com/Cristiano>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-g5-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-maps (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-maps-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/maps?force=pwa>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: google-search-restaurants (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-google-search-restaurants-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-g5-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/search?q=restaurants+near+me>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: imdb (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-imdb-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.imdb.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: instagram (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-instagram-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **login**: true
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-g5-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-g5-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.instagram.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: jianshu (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-jianshu-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.jianshu.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: microsoft-support (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-microsoft-support-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://support.microsoft.com/en-us>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: reddit (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-reddit-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.reddit.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: stackoverflow (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-stackoverflow-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://stackoverflow.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: web-de (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-web-de-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://web.de/magazine/politik/politologe-glaubt-grossen-koalition-herbst-knallen-33563566>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: wikipedia (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-wikipedia-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://en.m.wikipedia.org/wiki/Main_Page>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: youtube (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-youtube-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm4-pixel2-fennec-{subtest}.manifest
   * **playback recordings**: mitm4-pixel2-fennec-{subtest}.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false


.. dropdown:: youtube-watch (BT, GV, FE, RB, CH-M)
   :container: + anchor-id-youtube-watch-m

   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: geckoview, fenix, refbrow, chrome-m
   * **browser cycles**: 15
   * **expected**: pass
   * **lower is better**: true
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy-android
   * **playback pageset manifest**: mitm6-android-fenix-{subtest}.manifest
   * **playback recordings**: mitm6-android-fenix-{subtest}.mp
   * **playback version**: 6.0.2
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
