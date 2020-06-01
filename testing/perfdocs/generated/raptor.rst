######
Raptor
######

The following documents all testing we have for Raptor.

Benchmarks
----------
Standard benchmarks are third-party tests (i.e. Speedometer) that we have integrated into Raptor to run per-commit in our production CI. 


Desktop
-------
Tests for page-load performance.

* Browsertime amazon
* Browsertime bing-search
* Browsertime facebook
* Browsertime google-search
* Browsertime google-slides
* raptor-tp6-amazon
* raptor-tp6-apple
* raptor-tp6-binast-instagram-firefox
* raptor-tp6-bing
* raptor-tp6-docs
* raptor-tp6-ebay
* raptor-tp6-facebook
* raptor-tp6-fandom
* raptor-tp6-google
* raptor-tp6-google-mail
* raptor-tp6-imdb
* raptor-tp6-imgur
* raptor-tp6-instagram
* raptor-tp6-linkedin
* raptor-tp6-microsoft
* raptor-tp6-netflix
* raptor-tp6-office
* raptor-tp6-outlook
* raptor-tp6-paypal
* raptor-tp6-pinterest
* raptor-tp6-reddit
* raptor-tp6-sheets
* raptor-tp6-slides
* raptor-tp6-tumblr
* raptor-tp6-twitch
* raptor-tp6-twitter
* raptor-tp6-wikipedia
* raptor-tp6-yahoo-mail
* raptor-tp6-yahoo-news
* raptor-tp6-yandex
* raptor-tp6-youtube
* Browsertime wikipedia
* Browsertime yahoo-news
* Browsertime youtube

Live
----
A set of test pages that are run as live sites instead of recorded versions. These tests are available on all browsers, on all platforms.


Mobile
------
Page-load performance test suite on Android.

* Browsertime allrecipes
* Browsertime amazon
* Browsertime amazon-search
* Browsertime bbc
* Browsertime bing
* Browsertime bing-search-restaurants
* Browsertime booking
* Browsertime cnn
* Browsertime cnn-ampstories
* Browsertime ebay-kleinanzeigen
* Browsertime ebay-kleinanzeigen-search
* Browsertime espn
* Browsertime facebook
* Browsertime facebook-cristiano
* Browsertime google
* Browsertime google-maps
* Browsertime google-search-restaurants
* Browsertime imdb
* Browsertime instagram
* Browsertime jianshu
* Browsertime microsoft-support
* raptor-tp6m-allrecipes
* raptor-tp6m-amazon
* raptor-tp6m-amazon-search
* raptor-tp6m-bbc
* raptor-tp6m-bing
* raptor-tp6m-bing-restaurants
* raptor-tp6m-booking
* raptor-tp6m-cnn
* raptor-tp6m-cnn-ampstories
* raptor-tp6m-ebay-kleinanzeigen
* raptor-tp6m-espn
* raptor-tp6m-facebook
* raptor-tp6m-facebook-cristiano
* raptor-tp6m-google
* raptor-tp6m-google-maps
* raptor-tp6m-google-restaurants
* raptor-tp6m-imdb
* raptor-tp6m-instagram
* raptor-tp6m-jianshu
* raptor-tp6m-microsoft-support
* raptor-tp6m-reddit
* raptor-tp6m-stackoverflow
* raptor-tp6m-web-de
* raptor-tp6m-wikipedia
* raptor-tp6m-youtube
* raptor-tp6m-youtube-watch
* Browsertime reddit
* Browsertime stackoverflow
* Browsertime web-de
* Browsertime wikipedia
* Browsertime youtube
* Browsertime youtube-watch

Scenario
--------
Tests that perform a specific action (a scenario), i.e. idle application, idle application in background, etc.


Unittests
---------
These tests aren't used in standard testing, they are only used in the Raptor unit tests (they are similar to raptor-tp6 tests though).



The methods for calling the tests can be found in the `Raptor wiki page <https://wiki.mozilla.org/TestEngineering/Performance/Raptor>`_.
