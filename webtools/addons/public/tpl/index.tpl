
<div class="split-feature">
    <div class="split-feature-one">
        <div class="feature-download">
            <!-- Feature Image must be 200px wide... any height is fine, but around 170-200 is preferred -->
            <a href="./addon.php?id=1532"><img src="images/features/del.icio.us.png" width="200" height="213" alt="del.icio.us Extension"></a>
            <h3><a href="http://releases.mozilla.org/pub/mozilla.org/extensions/del.icio.us/del.icio.us-1.0.2-fx.xpi" onclick="return install(event,'del.icio.us 1.0.2', '{$config.webpath}/images/default.png');" title="Install del.icio.us 1.0.2 (Right-Click to Download)">Install Extension (62 KB)</a> </h3>

        </div>
        <h2>Featured Extension</h2>
        <h2><a href="./addon.php?id=1532">del.icio.us</a></h2>
        <p class="first">Harness the power of social bookmarking right in your browser with the del.icio.us Firefox extension! The del.icio.us extension for Firefox offers everything you need to seamlessly integrate the del.icio.us service with your Firefox browser. <a href="./addon.php?id=1532">Learn more...</a></p>
    </div>
    <a class="top-feature" href="./recommended.php"><img src="./images/feature-recommend.png" width="213" height="128" style="padding-left: 12px;" alt="We Recommend: See some of our favorite extensions to get you started."></a>

    <div class="split-feature-two">
    <h2><img src="images/title-topdownloads.gif" width="150" height="24" alt="Top 10 Downloads"></h2>

    <ol class="top-10">        
{section name=pe loop=$popularExtensions step=1 start=0}
        <li class="top-10-{$smarty.section.pe.iteration}"><a href="./addon.php?id={$popularExtensions[pe].ID}"><strong>{$popularExtensions[pe].name}</strong> {$popularExtensions[pe].dc}</a></li>
{/section}
    </ol>    
    </div>

</div>

<form id="front-search" method="get" action="/quicksearch.php" title="Search Mozilla Update">
    <div>
    <label for="q2" title="Search mozilla.org&quot;s sites">search:</label>
    <input type="hidden" name="cof" value="">
    <input type="hidden" name="domains" value="mozilla.org">
    <input type="hidden" name="sitesearch" value="mozilla.org">
    <input id="q2" type="text" name="q" accesskey="s" size="40">
    <select name="section">
          <option value="A">Entire Site</option>
          <option value="E">Extensions</option>
          <option value="T">Themes</option>
    </select>
    <input type="submit" value="Go">
    </div>
</form>

<div class="front-section-left">
    <h2><img src="images/title-browse.gif" width="168" height="22" alt="Browse By Category"></h2>
    <ul>
    <li><a href="./extensions/showlist.php?application=firefox&amp;category=Popular">Most Popular Add-ons</a></li>
    <li><a href="./extensions/showlist.php?application=firefox&amp;category=Newest">Recently Added</a></li>
    <li><a href="./extensions/showlist.php?application=firefox&amp;category=All">All Categories</a></li>
    </ul>
</div>


<div class="front-section-right">
    <h2><img src="images/title-develop.gif" width="152" height="22" alt="Develop Your Own"></h2>
    <ul>
    <li><a href="./developers/">Login to Submit</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Extensions">Documentation</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Building_an_Extension">Develop Your Own</a></li>
    </ul>
</div>

