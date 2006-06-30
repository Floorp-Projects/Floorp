
<h1 class="first">{$title}</h1>
<div class="front-section">
{if $success}
<p>Your password has been reset.</p>
<ul>
li><a href="login.php">Login to Mozilla Add-ons</a></li>
</ul>
{else}
<form id="resetpassword" name="resetpassword" method="post" action="">
{if $bad_input}
<p>Your passwords did not match.  Please try again.</p>
{/if}
    <label for="password">New Password:</label>
    <input id="password" name="password" type="password" />
    <label for="passwordconfirm">Verify Password:</label>
    <input id="passwordconfirm" name="passwordconfirm" type="password"/>
    <input id="submit" name="submit" type="submit" value="Reset Password" />
</form>
{/if}
</div>

