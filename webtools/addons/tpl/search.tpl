
<div id="side">
{* @todo fix the javascript hide/show so it's not n00bl3t1z3t3d *}

<div id="nav">

<form id="search-side" action="{$smarty.server.PHP_SELF}" method="get">

<input type="text" name="q">
<input type="submit" value="Search"><br><br>

<fieldset>
    <legend>Search Options</legend>

    <div class="search-option">
    <label for="cat">Category:</label>
    <select name="cat" id="cat">
    <option>Any</option>
    <option>Foo</option>
    </select>
    </div>

    <div class="search-option">
    <label for="type">Type:</label>
    <select id="type" name="type">
    <option>Any</option>
    <option>Extensions</option>
    <option>Themes</option>
    </select>
    </div>

    <div class="search-option">
    <label for="app">App:</label>
    <select id="app" name="app">
    <option>Any</option>
    <option>Firefox</option>
    <option>Thunderbird</option>
    <option>Mozilla</option>
    </select>
    </div>

    <div class="search-option">   
    <label for="os">Platform:</label>
    <select id="os" name="os">
    <option>Windows</option>
    <option>Linux</option>
    <option>OSX</option>
    </select>
    </div>

    <div class="search-option">
    <label for="date">Date:</label>
    <select id="date" name="date">
    <option>Any</option>
    <option>Today</option>
    <option>This Week</option>
    <option>This Month</option>
    <option>This Year</option>
    </select>
    </div>

    <div class="search-option">
    <label for="sort">Sort by:</label>
    <select id="sort" name="sort">
    <option>Name</option>
    <option>Date</option>
    <option>Rating</option>
    <option>Popularity</option>
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

