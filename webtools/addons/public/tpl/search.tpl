
<div id="side">
{* @todo fix the javascript hide/show so it's not n00bl3t1z3t3d *}

<div id="nav">

<form id="search-side" action="{$smarty.server.PHP_SELF}" method="get">

<input type="text" name="q" value="{$clean.q}" maxlength="32">
<input type="submit" value="Search"><br><br>

<fieldset>
    <legend>Search Options</legend>

    <div class="search-option">
    <label for="cat">Category:</label>
    <select name="cat" id="cat">
    <option value="null">Any</option>
    {html_options options=$cats selected=$clean.cat}
    </select>
    </div>

    <div class="search-option">
    <label for="type">Type:</label>
    <select id="type" name="type">
    <option value="null">Any</option>
    <option value="E"{if $clean.type eq 'E'} selected="selected"{/if}>Extensions</option>
    <option value="T"{if $clean.type eq 'T'} selected="selected"{/if}>Themes</option>
    </select>
    </div>

    <div class="search-option">
    <label for="app">App:</label>
    <select id="app" name="app">
    <option value="null">Any</option>
    {html_options options=$apps selected=$clean.app}
    </select>
    </div>

    <div class="search-option">   
    <label for="platform">Platform:</label>
    <select id="platform" name="platform">
    <option value="null">Any</option>
    {html_options options=$platforms selected=$clean.platform}
    </select>
    </div>

    <div class="search-option">
    <label for="date">Date:</label>
    <select id="date" name="date">
    <option value="null">Any</option>
    {html_options options=$dates selected=$clean.date}
    </select>
    </div>

    <div class="search-option">
    <label for="sort">Sort by:</label>
    <select id="sort" name="sort">
    {html_options options=$sort selected=$clean.sort}
    </select>
    </div>

    <div class="search-option">
    <label for="perpage">Per page:</label>
    <select id="perpage" name="perpage">
    {html_options options=$perpage selected=$clean.perpage}
    </select>
    </div>

</fieldset>

<input type="submit" value="Search"><br><br>

</form>

</div>
<!-- end nav -->

</div>
<!-- end side -->

<div id="mainContent">

{if $results}

<h2>Addon Search Results</h2>
<p class="first"><b>{$page.resultCount}</b> Addons found.  Showing records <b>{$page.leftDisplay}-{$page.right}</b>. <em>Too many results?  Try narrowing your search.</em></p>

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
    <div class="rating" title="4.67 Stars out of 5">Rating: {$results[r]->Rating}</div>
    <h2 class="first"><a href="./addon.php?id={$results[r]->ID}">{$results[r]->Name} {$results[r]->Version}</a></h2>

    {if $results[r]->PreviewURI}
    <p class="screenshot">
    <a href="./previews.php?id={$results[r]->ID}" title="See more {$results[r]->Name} previews.">
    <img src="{$config.webpath}{$results[r]->PreviewURI}" height="{$results[r]->PreviewHeight}" width="{$results[r]->PreviewWidth}" alt="{$results[r]->Name} screenshot">
    </a>
    <strong><a href="./previews.php?id={$results[r]->ID}" title="See more {$results[r]->Name} previews.">More Previews &raquo;</a></strong>
    </p>
    {/if}

    <p class="first">By <a href="./author.php?id={$results[r]->UserID}">{$results[r]->UserName}</a></p>
    <p class="first">{$results[r]->Description}</p>
    <div style="margin-top: 30px; height: 34px">
        <div class="iconbar">
            <a href="{$results[r]->URI}" onclick="return install(event,'{$results[r]->Name}', '{$config.webpath}/images/default.png');">
            <img src="{$config.webpath}/images/download.png" height="32" width="32" title="Install {$results[r]->Name}" ALT="">Install</a><br>
            <span class="filesize">&nbsp;&nbsp;{$results[r]->Size} kb</span>
        </div>
        <div class="iconbar">
            <img src="{$config.webpath}/images/{$results[r]->AppName|lower}_icon.png" height="34" width="34" ALT="">&nbsp;For {$results[r]->AppName}:<BR>&nbsp;&nbsp;{$results[r]->MinAppVer} - {$results[r]->MaxAppVer}
        </div>
    </div>
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

<h2>Addon Search</h2>
<p class="first">There are currently no results.  Please use the options at the left to begin a search or try adjusting your search terms.</p>

{/if}

</div>
<!-- end mainContent -->

