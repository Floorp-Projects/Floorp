
<h1>{$addon->Name|escape} - Firefox Extension</h1>
<p>{$addon->Name|escape} {$addon->Version|escape}, by {$addon->UserName|escape} released on {$addon->DateUpdated|date_format:"%B %d, %Y"}</p>
<h2 class="first">Your comments about {$addon->Name|escape}</h2>
<div class="front-section">
{if $c_added_comment}
<p>You comment has been added successfully.</p>
<ul>
<li><a href="addon.php?id={$addon->ID}">Return to {$addon->Name|escape}</a></li>
</ul>
{else}
<form id="commentform" name="commentform" method="post" action="">
    <label for="c_rating">Rating:</label>
    <select id="c_rating" name="c_rating">
        {html_options values=$rate_select_value output=$rate_select_name selected=$c_rating_value}
    </select>
    {if $c_errors.c_rating}
        <div>
            <p>Please choose a rating.</p>
        </div>
    {/if}
    <label for="c_title">Title:</label>
    <input type="text" id="c_title" name="c_title" value="{$c_title_value|escape}"/>
    {if $c_errors.c_title}
        <div>
            <p>Please provide a title with your comments.</p>
        </div>
    {/if}
    <label for="c_comments">Comments:</label>
    <textarea id="c_comments" name="c_comments">{$c_comments_value|escape}</textarea>
    {if $c_errors.c_comments}
        <div>
            <p>Please include some valid comments.</p>
        </div>
    {/if}
    <input type="submit" id="c_submit" name="c_submit" value="Post" />
    <p>All fields are required.</p>
</form>
{/if}
</div>


