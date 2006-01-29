
<h1 class="first">{$title}</h1>
<div class="front-section">
{if $success}
<p>An email has been sent.  Please follow the instructions included within it.</p>
{else}
<p>If you've forgotten your password, enter your e-mail address here and you will be
sent an email to help you recover it.</p>
<form id="recoverpassword" name="recoverpassword" method="post" action="">
{if $bad_input}
<p>Please verify your address is correct.</p>
{/if}
    <label for="email">E-Mail Address:</label>
    <input id="email" name="email" type="text" value="{$email|escape}"/>
    <input id="submit" name="submit" type="submit" value="Recover Password" />
</form>
{/if}
</div>

