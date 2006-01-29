<h1 class="first">{$title}</h1>
<div class="front-section">
{if $account_created}
    <p>Your account has been created successfully. An e-mail has been sent to you
    with instructions on how to activate your account so you can begin using it.</p>

{else}
    
    {if $bad_input}
        <p>Errors were found with your input.  Please review the messages below.</p>
    {else}
        <p>Joining Mozilla Update is easy!  Just fill out the form below and click the join button.</p>
    {/if}

    <form name="createaccount" method="post" action="">
        <p>Your e-mail address is used as your username to login. You'll also receive a
        confirmation e-mail to this address. In order for your account to be activated
        succesfully, you must specify a valid e-mail address.</p>
        {if $error_email_empty}
            <p>E-Mail address is a required field.</p>
        {/if}
        {if $error_email_malformed}
            <p>A valid E-Mail address is required.</p>
        {/if}
        {if $error_emailconfirm_nomatch}
            <p>The E-Mail addresses do not match.</p>
        {/if}
        {if $error_email_duplicate}
            <p>The E-Mail address you entered is already in use.  If this is your
            account, you can <a href="recoverpassword.php?email={$email_value|escape:"url"}">
            send yourself a password recovery e-mail</a>.</p>
        {/if}
        <label for="email">E-Mail Address:</label>
        <input id="email" name="email" type="text" value="{$email_value|escape}"/>

        <label for="emailconfirm">Confirm E-Mail:</label>
        <input id="emailconfirm" name="emailconfirm" type="text" value="{$emailconfirm_value|escape}"/>

        <p>How do you want to be known to visitors of Mozilla Update? This is your "author
        name" and it will be shown with your extension/theme listings on the Mozilla Update
        web site.</p>
        {if $error_name_empty}
            <p>Name is a required field.</p>
        {/if}
        <label for="name">Your Name</label>
        <input id="name" name="name" type="text" value="{$name_value|escape}"/>

        <p>If you have a website, enter the URL here. (including the http:// ) Your website
        will be shown to site visitors on your author profile page. This field is optional;
        if you don't have a website or don't want it linked to from Mozilla Update, leave 
        this box blank.</p>
        <label for="website">Your Website</label>
        <input id="website" name="website" type="text" value="{$website_value|escape}"/>

        <p>Your desired password. This along with your e-mail will allow you to login, so
        make it something memorable but not easy to guess. Type it in both fields below.  The
        two fields must match.</p>
        {if $error_password_empty}
            <p>Password is a required field.</p>
        {/if}
        {if $error_passwordconfirm_nomatch}
            <p>The passwords do not match.</p>
        {/if}
        <label for="password">Password:</label>
        <input id="password" name="password" type="password" />

        <label for="passwordconfirm">Confirm Password:</label>
        <input id="passwordconfirm" name="passwordconfirm" type="password" />

        <p>Review what you entered above. If everything's correct, click the "Join Mozilla
        Update" button. If you want to start over, click "Clear Form".</p>
        <input name="submit" type="submit" value="Join Mozilla Update" />
        <input name="reset" type="reset" value="Clear Form" />
    </form>
{/if}
</div>
