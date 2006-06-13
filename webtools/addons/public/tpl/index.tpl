<div><a class="joga-link" href="http://www.mozilla.com/add-ons/jogacompanion/">
    Don't miss a single goal.<br/>
   <small>Scores, videos, fans.  Joga.com Companion - Powered by Firefox.</small>
</a></div>

<div class="split-feature">
    <div class="split-feature-one">
        <div class="feature-download">
            <!-- Feature Image must be 200px wide... any height is fine, but around 170-200 is preferred -->
            <a href="{$config.webpath}/{$app}/{$feature.id}/"><img src="{$config.webpath}{$feature.previewuri}" alt="{$feature.name} Extension"></a>
            <h3><a href="{$feature.uri}" onclick="return install(event,'{$feature.name} {$feature.version}', '{$config.webpath}/images/default.png');" title="Install {$feature.name} {$feature.version} (Right-Click to Download)">Install Extension ({$feature.size} KB)</a> </h3>

        </div>
        <h2>Featured Add-on</h2>
        <h2><a href="{$config.webpath}/{$app}/{$feature.id}/">{$feature.name}</a></h2>
        <p class="first">{$feature.body|nl2br} <a href="{$config.webpath}/{$app}/{$feature.id}/"><br/>Learn more...</a></p>
    </div>
    <a class="top-feature" href="{$config.webpath}/{$app}/recommended/"><img src="{$config.webpath}/images/feature-recommend.png" width="213" height="128" style="padding-left: 12px;" alt="We Recommend: See some of our favorite add-ons to get you started."></a>

    <div class="split-feature-two">
    <h2><img src="{$config.webpath}/images/title-topdownloads.gif" width="150" height="24" alt="Top 10 Downloads"></h2>

    <ol class="top-10">        
{section name=pe loop=$popularExtensions step=1 start=0}
        <li class="top-10-{$smarty.section.pe.iteration}"><a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/"><strong>{$popularExtensions[pe].name}</strong> {$popularExtensions[pe].dc}</a></li>
{/section}
    </ol>    
    </div>

</div>

<form id="front-search" method="get" action="{$config.webpath}/search.php" title="Search Mozilla Addons">
    <div>
    <label for="q2" title="Search addons.mozilla.org">search:</label>
    <input id="q2" type="text" name="q" accesskey="s" size="40"/>
    <select name="type">
          <option value="A">Entire Site</option>
          <option value="E">Extensions</option>
          <option value="T">Themes</option>
    </select>
    <input type="hidden" name="app" value="{$app}"/>
    <input type="submit" value="Go"/>
    </div>
</form>

<div class="front-section-left">
    <h2><img src="{$config.webpath}/images/title-browse.gif" width="168" height="22" alt="Browse By Category"></h2>
    <ul>
    <li><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;sort=downloads">Most Popular Add-ons</a></li>
    <li><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;sort=rating">Highest Rated Add-ons</a></li>
    <li><a href="{$config.webpath}/search.php?app={$app}&amp;appfilter={$app}&amp;sort=newest">Recently Added</a></li>
    </ul>
</div>

<div class="front-section-right">
    <h2><img src="{$config.webpath}/images/title-develop.gif" width="152" height="22" alt="Develop Your Own"></h2>
    <ul>
    <li><a href="{$config.webpath}/developers/">Login to Submit</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Extensions">Documentation</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Building_an_Extension">Develop Your Own</a></li>
    </ul>
</div>

