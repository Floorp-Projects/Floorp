
{* @todo fix the javascript hide/show so it's not n00bl3t1z3t3d *}
<h2>Find an Addon</h2>

<form action="{$smarty.server.PHP_SELF}" method="get">

<div class="key-point">
<input type="text" name="q">
<div id="advanced-search" style="display:none;">
<a href="#" onclick="getElementById('advanced-search').style.display='none';getElementById('toggle-advanced-search').style.display='inline';">Hide Search Options</a>
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
    <label for="app">Application:</label>
    <select id="app" name="app">
    <option>Any</option>
    <option>Firefox</option>
    <option>Thunderbird</option>
    <option>Mozilla</option>
    </select>
    </div>

    <div class="search-option">   
    <label for="os">Operating System:</label>
    <select id="os" name="os">
    <option>Windows</option>
    <option>Linux</option>
    <option>OSX</option>
    </select>
    </div>

    <div class="search-option">
    <label for="date">Date Modified:</label>
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

</div>
<input type="submit" value="Search"><br>

<a id="toggle-advanced-search" href="#" onclick="this.style.display='none';getElementById('advanced-search').style.display='block';">Show Search Options</a>
</div>


</form>

