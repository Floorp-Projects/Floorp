######
Raptor
######

The following documents all testing we have for Raptor.

Benchmarks
----------
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI. 


Custom
------
Browsertime tests that use a custom pageload test script. These use the pageload type, but may have other intentions.

.. dropdown:: process-switch (Measures process switch time)
   :container: + anchor-id-process-switch-c

   * **accept zero vismet**: true
   * **alert on**: fcp, loadtime
   * **alert threshold**: 2.0
   * **apps**: firefox, chrome, chromium
   * **browser cycles**: 25
   * **browsertime args**: --pageCompleteWaitTime=1000 --pageCompleteCheckInactivity=true
   * **expected**: pass
   * **gecko profile entries**: 14000000
   * **gecko profile interval**: 1
   * **lower is better**: true
   * **measure**: fnbpaint, fcp, dcf, loadtime
   * **page cycles**: 25
   * **page timeout**: 60000
   * **playback**: mitmproxy
   * **playback pageset manifest**: mitm5-linux-firefox-proc-switch.manifest
   * **playback recordings**: mitm5-linux-firefox-seanfeng.mp mitm5-linux-firefox-pettay.mp
   * **playback version**: 5.1.1
   * **test script**: process_switch.js
   * **test url**: `<https://mozilla.seanfeng.dev/files/red.html,https://mozilla.pettay.fi/moztests/blue.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-custom-firefox-process-switch-e10s
      * test-linux1804-64/opt-browsertime-custom-firefox-process-switch-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-32-shippable/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-32/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-64-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-custom-firefox-process-switch-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-custom-firefox-process-switch-e10s
      * test-windows10-64/opt-browsertime-custom-firefox-process-switch-e10s



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
   * **playback pageset manifest**: mitm5-linux-firefox-amazon.manifest
   * **playback recordings**: mitm5-linux-firefox-amazon.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-linux1804-64/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-32/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-amazon-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-amazon-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-profiling-firefox-amazon-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-profiling-firefox-amazon-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-amazon-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-amazon-e10s
      * test-windows10-64/opt-browsertime-tp6-profiling-firefox-amazon-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-bing-search.manifest
   * **playback recordings**: mitm5-linux-firefox-bing-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.bing.com/search?q=barack+obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-bing-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-bing-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-bing-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-bing-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-bing-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-bing-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-bing-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-bing-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-bing-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-bing-search-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-bing-search-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-bing-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-bing-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-bing-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-bing-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-bing-search-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-bing-search-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-bing-search-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-buzzfeed.manifest
   * **playback recordings**: mitm5-linux-firefox-buzzfeed.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.buzzfeed.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-buzzfeed-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-buzzfeed-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-buzzfeed-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-buzzfeed-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-buzzfeed-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-buzzfeed-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-buzzfeed-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-buzzfeed-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-buzzfeed-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-buzzfeed-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-buzzfeed-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-cnn.manifest
   * **playback recordings**: mitm5-linux-firefox-cnn.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.cnn.com/2021/03/22/weather/climate-change-warm-waters-lake-michigan/index.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-cnn-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-cnn-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-cnn-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-cnn-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-cnn-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-cnn-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-cnn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-cnn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-cnn-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-cnn-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-cnn-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-cnn-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-cnn-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-ebay.manifest
   * **playback recordings**: mitm5-linux-firefox-ebay.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.ebay.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-ebay-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-ebay-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-ebay-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-ebay-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-ebay-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-ebay-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-ebay-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-ebay-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-ebay-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-ebay-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-ebay-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-ebay-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-ebay-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-ebay-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-ebay-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-ebay-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-ebay-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-ebay-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-ebay-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-ebay-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-ebay-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-espn.manifest
   * **playback recordings**: mitm5-linux-firefox-espn.mp
   * **playback version**: 5.1.1
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-espn-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-espn-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-espn-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-espn-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-espn-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-espn-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-espn-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-espn-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-espn-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-espn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-espn-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-espn-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-espn-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-espn-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-espn-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-espn-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-expedia.manifest
   * **playback recordings**: mitm5-linux-firefox-expedia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://expedia.com/Hotel-Search?destination=New+York%2C+New+York&latLong=40.756680%2C-73.986470&regionId=178293&startDate=&endDate=&rooms=1&_xpid=11905%7C1&adults=2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-expedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-expedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-expedia-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-expedia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-expedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-expedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-expedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-expedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-expedia-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-expedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-expedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-expedia-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-expedia-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-facebook.manifest
   * **playback recordings**: mitm5-linux-firefox-facebook.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-facebook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-facebook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-facebook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-facebook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-facebook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-facebook-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-facebook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-facebook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-facebook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-facebook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-facebook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-facebook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-facebook-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-facebook-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-facebook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-facebook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-facebook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-facebook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-facebook-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-facebook-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-facebook-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-fandom.manifest
   * **playback recordings**: mitm5-linux-firefox-fandom.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.fandom.com/articles/fallout-76-will-live-and-die-on-the-creativity-of-its-playerbase>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-fandom-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-fandom-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-fandom-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-fandom-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-fandom-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-fandom-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-fandom-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-fandom-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-fandom-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-fandom-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-fandom-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-fandom-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-fandom-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-fandom-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-fandom-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-fandom-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-fandom-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-fandom-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-fandom-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-fandom-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-fandom-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-google-docs.manifest
   * **playback recordings**: mitm5-linux-firefox-google-docs.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/document/d/1US-07msg12slQtI_xchzYxcKlTs6Fp7WqIc6W5GK5M8/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-google-docs-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-google-docs-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-google-docs-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-google-docs-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-google-docs-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-google-docs-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-google-docs-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-google-docs-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-google-docs-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-google-docs-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-docs-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-google-docs-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-google-docs-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-google-docs-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-google-docs-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-google-docs-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-google-docs-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-google-docs-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-google-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-google-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.google.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-google-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-google-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-google-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-google-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-google-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-google-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-google-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-google-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-google-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-mail-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-mail-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-google-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-google-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-google-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-google-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-google-mail-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-google-mail-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-google-mail-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-google-search.manifest
   * **playback recordings**: mitm5-linux-firefox-google-search.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.google.com/search?hl=en&q=barack+obama&cad=h>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-google-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-google-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-google-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-google-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-google-search-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-google-search-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-google-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-google-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-google-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-google-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-google-search-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-google-search-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-google-search-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-search-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-google-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-google-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-google-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-google-search-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-google-search-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-google-search-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-google-search-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-google-slides.manifest
   * **playback recordings**: mitm5-linux-firefox-google-slides.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://docs.google.com/presentation/d/1Ici0ceWwpFvmIb3EmKeWSq_vAQdmmdFcWqaiLqUkJng/edit?usp=sharing>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-google-slides-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-google-slides-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-google-slides-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-google-slides-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-google-slides-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-google-slides-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-google-slides-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-google-slides-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-google-slides-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-google-slides-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-google-slides-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-google-slides-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-google-slides-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-google-slides-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-google-slides-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-google-slides-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-google-slides-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-google-slides-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-imdb.manifest
   * **playback recordings**: mitm5-linux-firefox-imdb.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.imdb.com/title/tt0084967/?ref_=nv_sr_2>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-imdb-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-imdb-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-imdb-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-imdb-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-imdb-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-imdb-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-imdb-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-imdb-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-imdb-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-imdb-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-imdb-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-imdb-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-imdb-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-imdb-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-imdb-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-imdb-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-imdb-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-imdb-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-imdb-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-imdb-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-imdb-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-imgur.manifest
   * **playback recordings**: mitm5-linux-firefox-imgur.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://imgur.com/gallery/m5tYJL6>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-imgur-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-imgur-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-imgur-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-imgur-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-imgur-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-imgur-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-imgur-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-imgur-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-imgur-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-imgur-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-imgur-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-imgur-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-imgur-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-imgur-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-imgur-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-imgur-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-imgur-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-imgur-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-instagram.manifest
   * **playback recordings**: mitm5-linux-firefox-instagram.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.instagram.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-instagram-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-instagram-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-instagram-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-instagram-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-instagram-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-instagram-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-instagram-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-instagram-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-instagram-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-instagram-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-instagram-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-instagram-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-instagram-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-instagram-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-instagram-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-instagram-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-instagram-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-instagram-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-instagram-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-instagram-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-instagram-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-linkedin.manifest
   * **playback recordings**: mitm5-linux-firefox-linkedin.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.linkedin.com/in/thommy-harris-hk-385723106/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-linkedin-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-linkedin-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-linkedin-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-linkedin-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-linkedin-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-linkedin-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-linkedin-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-linkedin-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-linkedin-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-linkedin-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-linkedin-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-linkedin-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-linkedin-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-linkedin-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-linkedin-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-linkedin-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-linkedin-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-linkedin-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-microsoft.manifest
   * **playback recordings**: mitm5-linux-firefox-microsoft.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.microsoft.com/en-us/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-microsoft-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-microsoft-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-microsoft-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-microsoft-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-microsoft-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-microsoft-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-microsoft-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-microsoft-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-microsoft-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-microsoft-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-microsoft-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-microsoft-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-microsoft-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-microsoft-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-microsoft-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-microsoft-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-microsoft-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-microsoft-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-netflix.manifest
   * **playback recordings**: mitm5-linux-firefox-netflix.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.netflix.com/title/80117263>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-netflix-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-netflix-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-netflix-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-netflix-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-netflix-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-netflix-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-netflix-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-netflix-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-netflix-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-netflix-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-netflix-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-netflix-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-netflix-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-netflix-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-netflix-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-netflix-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-netflix-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-netflix-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-netflix-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-netflix-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-netflix-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-nytimes.manifest
   * **playback recordings**: mitm5-linux-firefox-nytimes.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.nytimes.com/2020/02/19/opinion/surprise-medical-bill.html>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-nytimes-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-nytimes-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-nytimes-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-nytimes-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-nytimes-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-nytimes-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-nytimes-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-nytimes-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-nytimes-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-nytimes-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-nytimes-e10s


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
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-office-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-office-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-office-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-office-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-office-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-office-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-office-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-office-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-office-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-office-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-office-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-office-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-office-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-office-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-office-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-office-e10s


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
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-outlook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-outlook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-outlook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-outlook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-outlook-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-outlook-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-outlook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-outlook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-outlook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-outlook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-outlook-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-outlook-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-outlook-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-outlook-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-outlook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-outlook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-outlook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-outlook-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-outlook-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-outlook-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-outlook-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-paypal.manifest
   * **playback recordings**: mitm5-linux-firefox-paypal.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.paypal.com/myaccount/summary/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-paypal-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-paypal-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-paypal-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-paypal-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-paypal-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-paypal-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-paypal-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-paypal-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-paypal-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-paypal-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-paypal-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-paypal-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-paypal-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-paypal-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-paypal-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-paypal-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-paypal-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-paypal-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-paypal-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-paypal-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-paypal-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-pinterest.manifest
   * **playback recordings**: mitm5-linux-firefox-pinterest.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://pinterest.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-pinterest-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-pinterest-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-pinterest-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-pinterest-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-pinterest-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-pinterest-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-pinterest-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-pinterest-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-pinterest-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-pinterest-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-pinterest-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-pinterest-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-pinterest-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-pinterest-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-pinterest-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-pinterest-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-pinterest-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-pinterest-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-reddit.manifest
   * **playback recordings**: mitm5-linux-firefox-reddit.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.reddit.com/r/technology/comments/9sqwyh/we_posed_as_100_senators_to_run_ads_on_facebook/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-reddit-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-reddit-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-reddit-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-reddit-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-reddit-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-reddit-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-reddit-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-reddit-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-reddit-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-reddit-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-reddit-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-reddit-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-reddit-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-reddit-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-reddit-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-reddit-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-reddit-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-reddit-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-reddit-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-reddit-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-reddit-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-tumblr.manifest
   * **playback recordings**: mitm5-linux-firefox-tumblr.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.tumblr.com/dashboard>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-tumblr-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-tumblr-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-tumblr-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-tumblr-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-tumblr-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-tumblr-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-tumblr-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-tumblr-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-tumblr-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-tumblr-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-tumblr-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-tumblr-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-tumblr-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-tumblr-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-tumblr-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-tumblr-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-tumblr-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-tumblr-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-twitch.manifest
   * **playback recordings**: mitm5-linux-firefox-twitch.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.twitch.tv/videos/326804629>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-twitch-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-twitch-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-twitch-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-twitch-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-twitch-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-twitch-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-twitch-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-twitch-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-twitch-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitch-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitch-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-twitch-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-twitch-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-twitch-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-twitch-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-twitch-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-twitch-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-twitch-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-twitter.manifest
   * **playback recordings**: mitm5-linux-firefox-twitter.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://twitter.com/BarackObama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chrome-twitter-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-chromium-twitter-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-twitter-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-twitter-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-linux1804-64/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chrome-twitter-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-chromium-twitter-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-twitter-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-twitter-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chrome-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-chromium-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-twitter-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-32/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-essential-firefox-twitter-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-twitter-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chrome-twitter-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-chromium-twitter-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-twitter-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-twitter-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-twitter-e10s
      * test-windows10-64/opt-browsertime-tp6-essential-firefox-twitter-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-twitter-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-wikia.manifest
   * **playback recordings**: mitm5-linux-firefox-wikia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://marvel.fandom.com/wiki/Black_Panther>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-wikia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-wikia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-wikia-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-wikia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-wikia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-wikia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-wikia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-wikia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-wikia-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-wikia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-wikia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-wikia-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-wikia-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-wikipedia.manifest
   * **playback recordings**: mitm5-linux-firefox-wikipedia.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://en.wikipedia.org/wiki/Barack_Obama>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-wikipedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-wikipedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-wikipedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-wikipedia-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-wikipedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-wikipedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-wikipedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-wikipedia-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-wikipedia-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-wikipedia-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-wikipedia-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-wikipedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-wikipedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-wikipedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-wikipedia-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-wikipedia-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-wikipedia-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-wikipedia-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-yahoo-mail.manifest
   * **playback recordings**: mitm5-linux-firefox-yahoo-mail.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://mail.yahoo.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-yahoo-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-yahoo-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-yahoo-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-yahoo-mail-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-yahoo-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-yahoo-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-yahoo-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-yahoo-mail-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-yahoo-mail-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-yahoo-mail-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-yahoo-mail-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-yahoo-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-yahoo-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-yahoo-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-yahoo-mail-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-yahoo-mail-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-yahoo-mail-e10s


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
   * **playback pageset manifest**: mitm5-linux-firefox-youtube.manifest
   * **playback recordings**: mitm5-linux-firefox-youtube.mp
   * **playback version**: 5.1.1
   * **test url**: `<https://www.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-linux1804-64-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-linux1804-64-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-fis-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-linux1804-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-fis-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chrome-youtube-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-chromium-youtube-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-firefox-youtube-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chrome-youtube-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-chromium-youtube-e10s
      * test-linux1804-64-shippable/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-linux1804-64/opt-browsertime-tp6-firefox-youtube-e10s
      * test-linux1804-64/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-fis-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-macosx1014-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-fis-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-macosx1015-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-fis-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chrome-youtube-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-chromium-youtube-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-firefox-youtube-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chrome-youtube-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-chromium-youtube-e10s
      * test-macosx1015-64-shippable/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chrome-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-chromium-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chrome-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-chromium-youtube-e10s
      * test-windows10-32-shippable/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-32/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-32/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-64-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-64-ref-hw-2017/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-firefox-youtube-fis-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-64-shippable-qr/opt-browsertime-tp6-live-firefox-youtube-fis-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chrome-youtube-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-chromium-youtube-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chrome-youtube-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-chromium-youtube-e10s
      * test-windows10-64-shippable/opt-browsertime-tp6-live-firefox-youtube-e10s
      * test-windows10-64/opt-browsertime-tp6-firefox-youtube-e10s
      * test-windows10-64/opt-browsertime-tp6-live-firefox-youtube-e10s



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
   * **playback pageset manifest**: mitm6-android-fenix-allrecipes.manifest
   * **playback recordings**: mitm6-android-fenix-allrecipes.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.allrecipes.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-allrecipes-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-allrecipes-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-amazon.manifest
   * **playback recordings**: mitm6-android-fenix-amazon.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-profiling-geckoview-amazon-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-amazon-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-amazon-search.manifest
   * **playback recordings**: mitm6-android-fenix-amazon-search.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.amazon.com/s/ref=nb_sb_noss_2/139-6317191-5622045?url=search-alias%3Daps&field-keywords=mobile+phone>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-amazon-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-amazon-search-e10s


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
   * **playback pageset manifest**: mitm4-pixel2-fennec-bbc.manifest
   * **playback recordings**: mitm4-pixel2-fennec-bbc.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bbc.com/news/business-47245877>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bbc-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bbc-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-bing.manifest
   * **playback recordings**: mitm6-android-fenix-bing.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bing-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bing-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-bing-search-restaurants.manifest
   * **playback recordings**: mitm6-android-fenix-bing-search-restaurants.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.bing.com/search?q=restaurants+in+exton+pa+19341>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-bing-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-bing-search-restaurants-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-booking.manifest
   * **playback recordings**: mitm6-android-fenix-booking.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.booking.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-booking-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-booking-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-cnn.manifest
   * **playback recordings**: mitm6-android-fenix-cnn.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-cnn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-cnn-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-cnn-ampstories.manifest
   * **playback recordings**: mitm6-android-fenix-cnn-ampstories.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://cnn.com/ampstories/us/why-hurricane-michael-is-a-monster-unlike-any-other>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-cnn-ampstories-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-cnn-ampstories-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-ebay-kleinanzeigen.manifest
   * **playback recordings**: mitm6-android-fenix-ebay-kleinanzeigen.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-ebay-kleinanzeigen-search.manifest
   * **playback recordings**: mitm6-android-fenix-ebay-kleinanzeigen-search.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.ebay-kleinanzeigen.de/s-anzeigen/auf-zeit-wg-berlin/zimmer/c199-l3331>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-ebay-kleinanzeigen-search-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-ebay-kleinanzeigen-search-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-espn.manifest
   * **playback recordings**: mitm6-android-fenix-espn.mp
   * **playback version**: 6.0.2
   * **test url**: `<http://www.espn.com/nba/story/_/page/allstarweekend25788027/the-comparison-lebron-james-michael-jordan-their-own-words>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-espn-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-espn-e10s


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
   * **playback pageset manifest**: mitm6-g5-fenix-facebook.manifest
   * **playback recordings**: mitm6-g5-fenix-facebook.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-facebook-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-facebook-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-facebook-cristiano.manifest
   * **playback recordings**: mitm6-android-fenix-facebook-cristiano.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.facebook.com/Cristiano>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-facebook-cristiano-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-facebook-cristiano-e10s


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
   * **playback pageset manifest**: mitm6-g5-fenix-google.manifest
   * **playback recordings**: mitm6-g5-fenix-google.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-google-maps.manifest
   * **playback recordings**: mitm6-android-fenix-google-maps.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/maps?force=pwa>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-maps-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-google-maps-e10s


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
   * **playback pageset manifest**: mitm6-g5-fenix-google-search-restaurants.manifest
   * **playback recordings**: mitm6-g5-fenix-google-search-restaurants.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.google.com/search?q=restaurants+near+me>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-google-search-restaurants-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-google-search-restaurants-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-imdb.manifest
   * **playback recordings**: mitm6-android-fenix-imdb.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.imdb.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-imdb-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-imdb-e10s


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
   * **playback pageset manifest**: mitm6-g5-fenix-instagram.manifest
   * **playback recordings**: mitm6-g5-fenix-instagram.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.instagram.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-instagram-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-instagram-e10s


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
   * **playback pageset manifest**: mitm4-pixel2-fennec-jianshu.manifest
   * **playback recordings**: mitm4-pixel2-fennec-jianshu.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.jianshu.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-jianshu-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-jianshu-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-microsoft-support.manifest
   * **playback recordings**: mitm6-android-fenix-microsoft-support.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://support.microsoft.com/en-us>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-microsoft-support-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-microsoft-support-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-reddit.manifest
   * **playback recordings**: mitm6-android-fenix-reddit.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.reddit.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-reddit-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-reddit-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-stackoverflow.manifest
   * **playback recordings**: mitm6-android-fenix-stackoverflow.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://stackoverflow.com/>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-stackoverflow-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-stackoverflow-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-web-de.manifest
   * **playback recordings**: mitm6-android-fenix-web-de.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://web.de/magazine/politik/politologe-glaubt-grossen-koalition-herbst-knallen-33563566>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-web-de-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-web-de-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-wikipedia.manifest
   * **playback recordings**: mitm6-android-fenix-wikipedia.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://en.m.wikipedia.org/wiki/Main_Page>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-wikipedia-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-wikipedia-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-youtube.manifest
   * **playback recordings**: mitm6-android-fenix-youtube.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://m.youtube.com>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-chrome-m-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-chrome-m-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-refbrow-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-fenix-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-profiling-geckoview-youtube-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-refbrow-youtube-e10s


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
   * **playback pageset manifest**: mitm6-android-fenix-youtube-watch.manifest
   * **playback recordings**: mitm6-android-fenix-youtube-watch.mp
   * **playback version**: 6.0.2
   * **test url**: `<https://www.youtube.com/watch?v=COU5T-Wafa4>`__
   * **type**: pageload
   * **unit**: ms
   * **use live sites**: false
   * **Test Task**:
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-qr/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable-qr/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-g5-7-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-qr/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable-qr/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-chrome-m-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64-shippable/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-android-aarch64/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-chrome-m-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-chrome-m-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16-shippable/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-geckoview-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-essential-refbrow-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-fenix-youtube-watch-e10s
      * test-android-hw-p2-8-0-arm7-api-16/opt-browsertime-tp6m-live-geckoview-youtube-watch-e10s



Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc.


Unittests
---------
These tests aren't used in standard testing, they are only used in the Raptor unit tests (they are similar to raptor-tp6 tests though).



The methods for calling the tests can be found in the `Raptor wiki page <https://wiki.mozilla.org/TestEngineering/Performance/Raptor>`_.
