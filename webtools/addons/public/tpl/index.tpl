{if $app eq "firefox"}
<h1>Welcome to Firefox Add-ons</h1>

<a href="{$config.webpath}/{$app}/recommended/" id="home-rec-link"><img src="{$config.webpath}/images/rustico/addons/firefox-addons-puzzle-ico.png" width="50" height="50" alt="" /></a>
  
<p class="frontpage-intro">Add-ons extend Firefox, letting you personalize your browsing experience.  They can make a tiny tweak, or bring a whole new range of features to your favorite browser.  Take a look around and make Firefox your own.</p>

<p>We've got a list of some of our <a href="{$config.webpath}/{$app}/recommended/">favorite add-ons</a> to get you started, including the one featured below.</p>

<div class="corner-box">
<h3 style="font-weight: bold; color: #f7941d;">Hey, where's the new site?</h3>
<p>The new add-ons site is a little behind, but it's coming soon.  <a href="http://blog.mozilla.com/webdev/2007/02/12/amo-v3-remora-launch-delayed-24-hours/">See our announcement</a> for more information.</p>
</div>

<div class="addon-feature clearfix">
	<div class="corner-box">
		<h3>Featured Add-on: <a href="{$config.webpath}/{$app}/{$feature.id}/">{$feature.name}</a></h3>
		<img src="{$config.webpath}{$feature.previewuri}" alt="" class="addon-feature-image" />
		<div class="addon-feature-text">
			<p>{$feature.body} <a href="{$config.webpath}/{$app}/{$feature.id}/">Learn more&hellip;</a></p>
			<p class="install-button"><a href="{$feature.uri}" onclick="return install(event,'{$feature.name|escape:"quotes"} {$feature.version}', '{$config.webpath}/images/default.png', '{$feature.hash}');" title="Install {$feature.name} {$feature.version} (Right-click to download)"><span class="install-green-button"><span class="install-button-text">Install now ({$feature.size}&nbsp;KB)</span></span></a></p>
      {if $feature.id eq SUPER_MAGIC_YAHOO_ID}
      <p class="eula">By clicking Install, you agree to the <a href="http://del.icio.us/help/firefox/bookmarks/license">Software License</a> for this add-on.</p>
      {/if}
		</div>
	</div>
</div>

{include file="inc/search-box.tpl"}

{else} 
<div class="split-feature">
    <div class="split-feature-one">
        <div class="feature-download">
            <!-- Feature Image must be 200px wide... any height is fine, but around 170-200 is preferred -->
            <a href="{$config.webpath}/{$app}/{$feature.id}/"><img src="{$config.webpath}{$feature.previewuri}" alt="{$feature.name} Extension"></a>
            <h3><a href="{$feature.uri}" onclick="return install(event,'{$feature.name|escape:"quotes"} {$feature.version}', '{$config.webpath}/images/default.png', '{$feature.hash}');" title="Install {$feature.name} {$feature.version} (Right-Click to Download)">Install Now ({$feature.size} KB)</a> </h3>

        </div>
        <h2>Featured Add-on</h2>
        <h2><a href="{$config.webpath}/{$app}/{$feature.id}/">{$feature.name}</a></h2>
        <p class="first">{$feature.body|nl2br} <a href="{$config.webpath}/{$app}/{$feature.id}/"><br/>Learn more...</a></p>
    </div>
    <a class="top-feature" href="{$config.webpath}/{$app}/recommended/"><img src="{$config.webpath}/images/feature-recommend.png" width="213" height="128" style="padding-left: 12px;" alt="We Recommend: See some of our favorite add-ons to get you started."></a>

    <div class="split-feature-two">
    <h2><img src="{$config.webpath}/images/title-topdownloads.gif" width="150" height="24" alt="Top Downloads"></h2>

    <ol class="top-10">        
{section name=pe loop=$popularExtensions step=1 start=0}
        <li class="top-10-{$smarty.section.pe.iteration}"><a href="{$config.webpath}/{$app}/{$popularExtensions[pe].ID}/"><strong>{$popularExtensions[pe].name}</strong> {$popularExtensions[pe].dc}</a></li>
{/section}
    </ol>    
    </div>

</div>

<form id="front-search" method="get" action="{$config.webpath}/search.php" title="Search Mozilla Add-ons">
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
    <li><a href="{$config.webpath}/developers/">Developer Control Panel</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Extensions">Extension Documentation on MDC</a></li>
    <li><a href="http://developer.mozilla.org/en/docs/Themes">Theme Documentation on MDC</a></li>
    </ul>
</div>
{/if}
