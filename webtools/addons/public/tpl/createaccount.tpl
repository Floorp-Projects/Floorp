<div id="mBody">
<h1>{$title}</h1>
{if $account_created}
    <p>Your account has been created successfully. An e-mail has been sent to you
    with instructions on how to activate your account so you can begin using it.</p>

{else}
    
    {if $bad_input}
        <div class="amo-form-error">Errors were found with your input.  Please review the messages below.</div>
    {/if}

    <form name="createaccount" method="post" action="" class="amo-form">

        <p>Your e-mail address is used as your username to login. You'll also receive a
        confirmation e-mail to this address. In order for your account to be activated
        successfully, you must specify a valid e-mail address.</p>

        {if $error_email_empty}
            <div class="amo-form-error">E-Mail address is a required field.</div>
        {/if}
        {if $error_email_malformed}
            <div class="amo-form-error">A valid E-Mail address is required.</div>
        {/if}
        {if $error_emailconfirm_nomatch}
            <div class="amo-form-error">The E-Mail addresses do not match.</div>
        {/if}
        {if $error_email_duplicate}
            <div class="amo-form-error">The E-Mail address you entered is already in use.  If this is your
            account, you can <a href="{$config.webpath}/recoverpassword.php?email={$email_value|escape:"url"}">
            send yourself a password recovery e-mail</a>.</div>
        {/if}

        <div>
        <label class="amo-label-large" for="email">E-Mail Address:</label>
        <input id="email" name="email" type="text" value="{$email_value|escape}" size="40"/>
        </div>

        <div>
        <label class="amo-label-large" for="emailconfirm">Confirm E-Mail:</label>
        <input id="emailconfirm" name="emailconfirm" type="text" value="{$emailconfirm_value|escape}" size="40"/>
        </div>

        <p>How do you want to be known to visitors of Mozilla Add-ons?</p>

        {if $error_name_empty}
            <div class="amo-form-error">Name is a required field.</div>
        {/if}

        <div>
        <label class="amo-label-large" for="name">Your Name:</label>
        <input id="name" name="name" type="text" value="{$name_value|escape}" size="40"/>
        </div>

        <p>If you have a website, enter the URL here. (including the http:// ) Your website
        will be shown to site visitors on your author profile page. This field is optional;
        if you don't have a website or don't want it linked to from Mozilla Add-ons, leave 
        this box blank.</p>

        <div>
        <label class="amo-label-large" for="website">Your Website:</label>
        <input id="website" name="website" type="text" value="{$website_value|escape}" size="40"/>
        </div>

        {if $error_password_empty}
            <div class="amo-form-error">Password is a required field.</div>
        {/if}
        {if $error_passwordconfirm_nomatch}
            <div class="amo-form-error">The passwords do not match.</div>
        {/if}
        
        <div>
        <label class="amo-label-large" for="password">Password:</label>
        <input id="password" name="password" type="password" size="40"/>
        </div>

        <div>
        <label class="amo-label-large" for="passwordconfirm">Confirm Password:</label>
        <input id="passwordconfirm" name="passwordconfirm" type="password" size="40"/>
        </div>

        <p>Review what you entered above. If everything's correct, click the "Join Mozilla
        Add-ons" button.</p>

        <div>
        <input name="submit" type="submit" value="Join Mozilla Add-ons &raquo;" class="amo-submit"/>
        </div>
    </form>
{/if}
</div>
