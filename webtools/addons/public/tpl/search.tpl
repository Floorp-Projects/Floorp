{if $app eq "firefox"}
<div class="corner-box">

<div>
  <form action="{$smarty.server.PHP_SELF}" method="get" class="amo-form">  
<input type="hidden" name="app" value="{$app}"/>
<input type="text" name="q" value="{$clean.q}" maxlength="32"/><input class="amo-submit" type="submit" value="Search"/> 
<span id="show-search-options" class="compact-list"><a href="javascript:toggle('hide-search-options','show-search-options','inline');show('search-options','inline');">Advanced Options &darr;</a></span>
<span id="hide-search-options" class="compact-list"><a href="javascript:toggle('show-search-options','hide-search-options','inline');hide('search-options');">Hide Options &uarr;</a></span>
</div>

<div id="search-options">

<fieldset>

    <legend>Search Options</legend>

    <div>
    <label class="amo-label-small" for="cat">Category:</label>
    <select name="cat" id="cat">
    <option value="null">Any</option>
    {html_options options=$cats selected=$clean.cat}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="type">Type:</label>
    <select id="type" name="type">
    <option value="null">Any</option>
    <option value="E"{if $clean.type eq 'E'} selected="selected"{/if}>Extensions</option>
    <option value="T"{if $clean.type eq 'T'} selected="selected"{/if}>Themes</option>
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="appfilter">App:</label>
    <select id="appfilter" name="appfilter">
    <option value="null">Any</option>
    {html_options options=$apps selected=$clean.appfilter}
    </select>
    </div>

    <div>   
    <label class="amo-label-small" for="platform">Platform:</label>
    <select id="platform" name="platform">
    <option value="null">Any</option>
    {html_options options=$platforms selected=$clean.platform}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="date">Date:</label>
    <select id="date" name="date">
    <option value="null">Any</option>
    {html_options options=$dates selected=$clean.date}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="sort">Sort by:</label>
    <select id="sort" name="sort">
    {html_options options=$sort selected=$clean.sort}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="perpage">Per page:</label>
    <select id="perpage" name="perpage">
    {html_options options=$perpage selected=$clean.perpage}
    </select>
    </div>

</fieldset>

<p><input class="amo-submit" type="submit" value="Search"/></p>
</form>
<!-- end search-options -->
<input type="hidden" name="app" value="{$app}"/>
</div>

{if $results}

<p class="first">Showing results <b>{$page.leftDisplay}</b> to <b>{$page.right}</b> of <b>{$page.resultCount}</b>. <em>(Too many results?  Try using the advanced options to narrow your search.)</em></p>

<table width="100%" class="compact-list">
  <tr><td width="50%" align="left">{if $page.left}
  <a href="./search.php?{$page.url}&amp;left={$page.previous}">&larr; Previous Results</a>
  {else}
  &nbsp;
  {/if}</td>
  <td width="50%" align="right">
    {if $page.next}
    <a href="./search.php?{$page.url}&amp;left={$page.next}">Next Results &rarr;</a>
    {else}
    &nbsp;
    {/if}
  </td>
  <tr>
</table>

<div id="search-results" class="clearfix">
{section name=r loop=$results}
<div class="addon-feature clearfix">
	<div class="corner-box">
		<h2><a href="{$config.webpath}/{$app}/{$results[r]->ID}/">{$results[r]->Name}</a> <span>{$results[r]->Version}</span> <span>by
      {foreach key=key item=item from=$results[r]->Authors}
              <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>{if $item != end($results[r]->Authors)},{/if}
      {/foreach}
		  </span></h2>
      {if $results[r]->PreviewURI}
      	<img src="{$config.webpath}{$results[r]->PreviewURI}" alt="" class="search-result-image" />
      {/if}
		<p class="desc">{$results[r]->Description|nl2br}</p>
	</div>
</div>

{/section}
</div>

<table width="100%" class="compact-list">
  <tr><td width="50%" align="left">{if $page.left}
  <a href="./search.php?{$page.url}&amp;left={$page.previous}">&larr; Previous Results</a>
  {else}
  &nbsp;
  {/if}</td>
  <td width="50%" align="right">
    {if $page.next}
    <a href="./search.php?{$page.url}&amp;left={$page.next}">Next Results &rarr;</a>
    {else}
    &nbsp;
    {/if}
  </td>
  <tr>
</table>

{else}

<h2>Addon Search</h2>
<p class="first">There are currently no results.  Please use the options above to adjust your search terms.</p>

{/if}

{else} {* not firefox *}
<div class="key-point">

<form action="{$smarty.server.PHP_SELF}" method="get" class="amo-form">

<div>
<input type="hidden" name="app" value="{$app}"/>
<input type="text" name="q" value="{$clean.q}" maxlength="32"/><input class="amo-submit" type="submit" value="Search"/> 
<div id="show-search-options"><a href="javascript:toggle('hide-search-options','show-search-options','inline');show('search-options','inline');">Show Options</a></div>
<div id="hide-search-options"><a href="javascript:toggle('show-search-options','hide-search-options','inline');hide('search-options');">Hide Options</a></div>
</div>

<div id="search-options">

<fieldset>

    <legend>Search Options</legend>

    <div>
    <label class="amo-label-small" for="cat">Category:</label>
    <select name="cat" id="cat">
    <option value="null">Any</option>
    {html_options options=$cats selected=$clean.cat}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="type">Type:</label>
    <select id="type" name="type">
    <option value="null">Any</option>
    <option value="E"{if $clean.type eq 'E'} selected="selected"{/if}>Extensions</option>
    <option value="T"{if $clean.type eq 'T'} selected="selected"{/if}>Themes</option>
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="appfilter">App:</label>
    <select id="appfilter" name="appfilter">
    <option value="null">Any</option>
    {html_options options=$apps selected=$clean.appfilter}
    </select>
    </div>

    <div>   
    <label class="amo-label-small" for="platform">Platform:</label>
    <select id="platform" name="platform">
    <option value="null">Any</option>
    {html_options options=$platforms selected=$clean.platform}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="date">Date:</label>
    <select id="date" name="date">
    <option value="null">Any</option>
    {html_options options=$dates selected=$clean.date}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="sort">Sort by:</label>
    <select id="sort" name="sort">
    {html_options options=$sort selected=$clean.sort}
    </select>
    </div>

    <div>
    <label class="amo-label-small" for="perpage">Per page:</label>
    <select id="perpage" name="perpage">
    {html_options options=$perpage selected=$clean.perpage}
    </select>
    </div>

</fieldset>

<p><input class="amo-submit" type="submit" value="Search"/></p>

<!-- end search-options -->
<input type="hidden" name="app" value="{$app}"/>
</div>

</form>

</div>

<div id="mBody">


{if $results}

<h2>Search Results</h2>
<p class="first"><b>{$page.resultCount}</b> Add-ons found.  Showing records <b>{$page.leftDisplay}-{$page.right}</b>. <em>Too many results?  Try narrowing your search.</em></p>

<div class="pages">
<div class="prev">
{if $page.left}
<a href="./search.php?{$page.url}&amp;left={$page.previous}">&laquo; Previous Page</a>
{else}
&laquo; Previous Page
{/if}
</div>

<div class="next">
{if $page.next}
<a href="./search.php?{$page.url}&amp;left={$page.next}">Next Page &raquo;</a>
{else}
Next Page &raquo;
{/if}
</div>
</div>

{section name=r loop=$results}
<div class="item">
    <div class="rating" title="{$results[r]->Rating} out of 5">Rating: {$results[r]->Rating}</div>
    <h2 class="first"><a href="{$config.webpath}/{$app}/{$results[r]->ID}/">{$results[r]->Name} {$results[r]->Version}</a></h2>

    {if $results[r]->PreviewURI}
    <p class="screenshot">
    <a href="{$config.webpath}/{$app}/{$results[r]->ID}/previews/" title="See more {$results[r]->Name} previews.">
    <img src="{$config.webpath}{$results[r]->PreviewURI}" height="{$results[r]->PreviewHeight}" width="{$results[r]->PreviewWidth}" alt="{$results[r]->Name} screenshot">
    </a>
    <strong><a href="{$config.webpath}/{$app}/{$results[r]->ID}/previews/" title="See more {$results[r]->Name} previews.">More Previews &raquo;</a></strong>
    </p>
    {/if}

    <p class="first">By 

{foreach key=key item=item from=$results[r]->Authors}
        <a href="{$config.webpath}/{$app}/{$item.UserID|escape}/author/">{$item.UserName|escape}</a>{if $item != end($results[r]->Authors)},{/if}
{/foreach}

    </p>
    <p class="first">{$results[r]->Description|nl2br}</p>
    <div class="baseline">Last Update:  {$results[r]->DateUpdated|date_format} | Downloads Last 7 Days: {$results[r]->downloadcount} | Total Downloads: {$results[r]->TotalDownloads}</DIV>
</div>
{/section}


<div class="pages">
<div class="prev">
{if $page.left}
<a href="./search.php?{$page.url}&amp;left={$page.previous}">&laquo; Previous Page</a>
{else}
&laquo; Previous Page
{/if}
</div>

<div class="next">
{if $page.next}
<a href="./search.php?{$page.url}&amp;left={$page.next}">Next Page &raquo;</a>
{else}
Next Page &raquo;
{/if}
</div>
</div>

{else}

<h2>Add-on Search</h2>
<p class="first">Sorry, no add-ons found that match your search.</p>

{/if}

<!-- End mBody -->
</div>
{/if}