<div id="mBody">
<h1 class="first">Login</h1>
    {if $login_error}
        <div>
            <p>You were not successfully logged in.  Check your e-mail address and
            password and try again.</p>
        </div>
    {/if}
    <form id="front-login" class="amo-form" method="post" action="" title="Login to Firefox Add-ons">

        <div>
        <label class="amo-label-large" for="username" title="E-Mail Address">E-Mail Address:</label>
        <input id="username" name="username" type="text" accesskey="u" size="40"/>
        </div>

        <div>
        <label class="amo-label-large" for="password" title="Password">Password:</label>
        <input id="password" name="password" type="password" accesskey="p" size="40"/>
        </div>

        <div>
        <input class="amo-submit" type="submit" value="Go &raquo;"/>
        </div>
        
        <p><strong><a href="createaccount.php">Create an account &raquo;</a></strong></p>

    </form>
</div>
