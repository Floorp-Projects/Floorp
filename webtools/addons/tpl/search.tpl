
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
    <option>Any</option>
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

</fieldset>

<input type="submit" value="Search"><br><br>

</form>

</div>
<!-- end nav -->

</div>
<!-- end side -->

<div id="mainContent">

<h2>Addon Search</h2>
{if $results}
{else}
<p class="first">There are currently no results.  Please use the options at the left to begin a search or try adjusting your search terms.</p>
{/if}

</div>
<!-- end mainContent -->

