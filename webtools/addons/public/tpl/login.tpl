
<h1 class="first">Login</h1>
<div class="front-section">
    {if $login_error}
        <div>
            <p>You were not successfully logged in.  Check your e-mail address and
            password and try again.</p>
        </div>
    {/if}
    <form id="front-login" method="post" action="" title="Login to Firefox Add-ons">
        <div>
        <label for="username" title="E-Mail Address">E-Mail Address:</label>
        <input id="username" name="username" type="text" accesskey="u" size="40">
        <label for="password" title="Password">Password:</label>
        <input id="password" name="password" type="password" accesskey="p" size="40">
        <input type="submit" value="Go">
        </div><a href="createaccount.php">Create an account</a>
    </form>
</div>

