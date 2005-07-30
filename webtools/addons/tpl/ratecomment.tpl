{if $error}
<p>Unfortunately, there has been a problem with your submission. Please go back and try again</p>
{else}
<p class="first">
<strong><a href="./addon.php?id={$addon->ID}">{$addon->Name} {$addon->Version}</a></strong>,
by <a href="./author.php?id={$addon->UserID}">{$addon->UserName}</a>,
released on {$addon->VersionDateAdded|date_format}
</p>
<h3>Comment for {$addon->Name} Rated {if $rate}Sucessfully{else}Unsucessfully{/if}</h3>
{if $rate}
<p>You have successfully rated this comment</p>
{else}
<p>Unfortunately, there was an error rating this comment. If this problem persists, please contact the admins.</p>
{/if}
{/if}
<p><a href="./addon.php?id={$addon->ID}">Return to information about{$addon->Name}</a></p>
