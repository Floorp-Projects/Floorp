
<h1 class="first">{$title}</h1>
<div class="front-section">
{if $confirmed}
<p>The account {$email|escape} has been activated successfully.</p>
<ul>
    <li><a href="login.php">Login to Mozilla Addons</a></li>
</ul>

{else}
<p>There was an error activating {$email|escape}. If the account hasn't been
registered yet, please register before activating.</p>
<ul>
    <li><a href="createaccount.php">Create a Mozilla Addons account</a></li>
</ul>

{/if}
</div>

