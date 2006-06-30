<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:template match="/rss">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
    <title><xsl:value-of select="channel/title"/></title>
    <meta name="keywords" content="mozilla update, mozilla extensions, mozilla plugins, thunderbird themes, thunderbird extensions, firefox extensions, firefox themes" />
    <link rel="stylesheet" type="text/css" href="/css/print.css" media="print" />
    <link rel="stylesheet" type="text/css" href="/css/base/content.css" media="all" />
    <link rel="stylesheet" type="text/css" href="/css/cavendish/content.css" title="Cavendish" media="all" />
    <link rel="stylesheet" type="text/css" href="/css/base/template.css" media="screen" />
    <link rel="stylesheet" type="text/css" href="/css/cavendish/template.css" title="Cavendish" media="screen" />
    <link rel="stylesheet" type="text/css" href="/css/forms.css" media="screen" />
    <link rel="home" title="Home" href="https://addons.mozilla.org/" />
    <link rel="alternate" type="application/rss+xml" href="{self}" title="{title}" />
    <link rel="shortcut icon" href="{channel/image/url}" />

</head>

<body>
    <div id="container">
        <p class="skipLink"><a href="#firefox-feature" accesskey="2">Skip to main content</a></p>
        <div id="mozilla-com"><a href="http://www.mozilla.com/">Visit Mozilla.com</a></div>
        <div id="header">
            <div id="key-title">
                <h1>
                    <a href="/firefox/" title="Return to home page" accesskey="1">
                        <img src="/images/title-firefox.gif" width="276" height="54" alt="Firefox Add-ons Beta" />
                    </a>
                </h1>
                
                <script type="text/javascript">
                    //<![CDATA[
                        addUsernameToHeader();
                    //]]>
                </script>

                <form id="search" method="get" action="/search.php" title="Search Mozilla Update">
                    <div>
                        <label for="q" title="Search Mozilla Update">search:</label>
                        <input type="text" id="q" name="q" accesskey="s" size="10" />
                        <input type="hidden" name="app" value="firefox" />
                        <input type="submit" id="submit" value="Go" />
                    </div>
                </form>
            </div>

            <div id="key-menu"> 
                <ul id="menu-firefox">
                    <li><a href="/firefox/">Home</a></li>
                    <li><a href="/firefox/extensions/">Extensions</a></li>
                    <li><a href="/firefox/plugins/">Plugins</a></li>
                    <li><a href="/firefox/search-engines/">Search Engines</a></li>
                    <li><a href="/firefox/themes/">Themes</a></li>
                </ul>
            </div>
            <!-- end key-menu -->
        </div>
        <!-- end header -->
        <hr class="hide" />
        <div id="mBody">
            <h1><xsl:value-of select="channel/title"/></h1>
            <p>
                This is an RSS feed designed to be read by an RSS reader. Which you aren't.
            </p>
            
            <p>
                <strong>What's an RSS Feed?</strong><br />
                <acronym title="Really Simple Syndication">RSS</acronym> feeds 
                allow you to take the latest content from our site and view it in other places
                such as a feed reader, your browser or your website. Feeds make it easy and 
                convenient to stay on top of the latest from Mozilla Add-ons.
            </p>

            <p>
                <strong>How do I use this feed?</strong><br/>
                To add this feed as a live bookmark in Firefox, simply click on the orange icon 
                (<img src="/images/rss.png" alt="RSS" />) in the address bar. Otherwise, take the
                URL of this feed and add it to your favorite RSS reader.
            </p>
            
            <p>Preview of this feed:</p>
                <ul>
                    <xsl:for-each select="channel/item">
                        <li>
                            <strong><a href="{link}"><xsl:value-of select="title"/></a></strong>
                        </li>
                    </xsl:for-each>
                </ul>

        </div>
        <hr class="hide" />
        
        <div id="footer">

        <p><a href="/firefox/" class="switch-fx">Firefox Add-ons </a><a href="/thunderbird/" class="switch-tb">Thunderbird Add-ons </a><a href="/mozilla/" class="switch-suite">Mozilla Suite Add-ons </a></p>
        <p><a href="/faq.php">FAQ</a> <a href="/feeds.php">Feeds/RSS</a> <a href="/login.php">Log In</a> <a href="/logout.php">Logout</a> <a href="/createaccount.php">Register</a></p>
        <p><a href="http://www.mozilla.org/privacy-policy.html">Privacy Policy</a> <a href="http://www.mozilla.org/foundation/donate.html">Donate to Mozilla</a> <a href="http://mozilla.org/">The Mozilla Organization</a></p>

        <p><span>Copyright &#169; 2004-2006</span>  <a href="http://www.xramp.com/">256-bit SSL Encryption provided by XRamp</a></p>
    </div>
    </div>
    <!-- close container -->

    <div class="disclaimer">
        Mozilla is providing links to these applications as a courtesy, and makes no representations 
        regarding the applications or any information related thereto. Any questions, complaints or 
        claims regarding the applications must be directed to the appropriate software vendor.  See 
        our <a href="/support.php">Support Page</a> for support information and contacts.
    </div>
</body>
</html>
</xsl:template>
</xsl:stylesheet>
