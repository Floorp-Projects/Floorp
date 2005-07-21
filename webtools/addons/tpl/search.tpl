
<h2>Find an Addon</h2>

<div class="key-point">
<form action="{$smarty.server.PHP_SELF}" method="post">
<fieldset>
<legend>Addon {if $advanced}Advanced{/if} Search Options</legend>

<div>

<!-- JUST AN EXAMPLE FOR NOW -->
<label for="app">App:</label>
<select id="app" name="app">
<option>Firefox</option>
<option>Thunderbird</option>
<option>Mozilla</option>
</select>

<label for="type">Addon Type:</label>
<select id="app" name="app">
<option>Any</option>
<option>Extension</option>
<option>Theme</option>
</select>
</div>

<div>
<label for="cat">Category: </label>
<select id="cat" name="cat">
<option>Tools</option>
<option>Other Category</option>
<option>Blah</option>
</select>

<label for="sort">Sort by:</label>
<select id="sort" name="sort">
<option>Popularity</option>
<option>User Rating</option>
<option>Date Submitted</option>
<option>Date Updated</option>
</select>
</div>

</fieldset>

<input type="submit" value="Go">

</form>

<a href="./search.php?adv=1" class="right">Advanced Search</a>
</div>

<p>This is just an example...</p>

