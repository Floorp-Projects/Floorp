<?php

// Used to send mail
vendor('mail/mail');

class UsersController extends AppController {

    var $name = 'Users';

    var $helpers = array('html','javascript');

    /**
     * Nothing to see here
     */
    function index() 
    {
    }

    /**
     * Adding users to the database
     */
    function add()
    {
        if (empty($this->params['data']))
        {
            $this->set('tracking_image','userpanel1-form.gif');
            $this->render();
        }
        else
        {
            /**
             * @todo The captcha stuff should be moved to a component (instead of a vendor
             * package).  The manual error handling and vendor code was added because
             * of time constraints (namely, this needs to be done in the next 22
             * minutes)
             */

            // They didn't fill in a value
            if (empty($_SESSION['freecap_word_hash']) || empty($this->params['data']['captcha'][0])) {
                $form_captcha_error = 'You must enter the code above.  If you are unable to see the code, please <a href="mailto:firefoxsurvey@mozilla.com">email us.</a>';
                $this->set('form_captcha_error',$form_captcha_error);
                return;
            }

            // Just some sanity checking.  If a user messes with their cookie
            // manually, they could be trying to execute custom functions
            if (!in_array($_SESSION['hash_func'], array('sha1','md5','crc32'))) {
                // fail silently?
                return;
            }

            // Check the captcha values
            if( $_SESSION['hash_func'](strtolower($this->params['data']['captcha'][0])) != $_SESSION['freecap_word_hash']) {
                $form_captcha_error = 'The code you entered did not match the picture.  Please try again.';

                $this->set('form_captcha_error',$form_captcha_error);
                return;
            } else {
                //reset session values
                $_SESSION['freecap_attempts'] = 0;
                $_SESSION['freecap_word_hash'] = false;
            }

            // If they've already signed up, send them another email
            if ($this->User->findByEmail($this->params['data']['User']['email'])) {
                    $mail_params = array(
                        'from' => 'Firefox User Panel <nobody@mozilla.com>',
                        'envelope' => 'nobody@mozilla.com',
                        'to'   => $this->params['data']['User']['email'],
                        'subject' => 'Firefox User Panel',
                        'message' => "
<p>Thanks for volunteering to be a part of the Firefox User Panel!</p>

<p>Please start by taking this short initial survey.  
We're anxious to get your feedback on a variety of areas, so we'll send out a few
more surveys in the following weeks to the user panel to help us define the next
generation of Firefox.  Thanks again for making the browser a better place!</p>

<p><a href=\"https://is4.instantsurvey.com/take?i=105913&h=0yU72vyhajHZpq6bAu_RGQ&email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Join the User Panel</a></p>

<p><small>Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></small></p>

<p><small>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</small></p>

<p><small>If you choose not to take this survey, you will be automatically unsubscribed from the Firefox User Panel.</small></p>

<p><small>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</small></p>
"
                    );

                    $mail = new mail($mail_params);

                    $mail->send();

                    $this->layout = 'submit';
                    $this->set('content','
                        <p>You are already signed up for the Firefox User Panel!</p>

                        <p>If you don\'t receive an email with a link to the survey within 24 hours, please check
                        your junk mail.  If you still haven\'t received an email with a link to the survey
                        please <a href="mailto:firefoxsurvey@mozilla.com?subject=invalid email - firefox user panel">let us know</a> 
                        and we\'ll send another copy.</p>

                        <p>Continue to <a href="http://www.mozilla.com/firefox/central/">Firefox Central</a>.</p> 
                    ');
                    $this->set('tracking_image','userpanel3-alreadysignedup.gif');
                    $this->render();
                    return;

            }
            if ($this->User->save($this->params['data'])) {
                    $mail_params = array(
                        'from' => 'Firefox User Panel <nobody@mozilla.com>',
                        'to'   => $this->params['data']['User']['email'],
                        'envelope' => 'nobody@mozilla.com',
                        'subject' => 'Firefox User Panel',
                        'message' => "
<p>Thanks for volunteering to be a part of the Firefox User Panel!</p>

<p>Please start by taking this short initial survey.  
We're anxious to get your feedback on a variety of areas, so we'll send out a few
more surveys in the following weeks to the user panel to help us define the next
generation of Firefox.  Thanks again for making the browser a better place!</p>

<p><a href=\"https://is4.instantsurvey.com/take?i=105913&h=0yU72vyhajHZpq6bAu_RGQ&email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Join the User Panel</a></p>

<p><small>Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></small></p>

<p><small>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</small></p>

<p><small>If you choose not to take this survey, you will be automatically unsubscribed from the Firefox User Panel.</small></p>

<p><small>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</small></p>
"
                    );


                    $mail = new mail($mail_params);

                    $mail->send();

                    $this->layout = 'submit';
                    $this->set('content','
                        <p>Thanks for helping us make a better browser!</p>

                        <p>Be sure to check your email for a link to the first survey.<br />
                        Continue to <a href="http://www.mozilla.com/firefox/central/">Firefox Central</a>.</p>

                        <p class="subtext">If you don\'t receive an email with a link to
                        the survey within 24 hours, please check your junk mail or let us
                        know and we\'ll send another copy.</p>
                    ');
                    $this->set('tracking_image','userpanel2-response.gif');
                    $this->render();
            }
        }

    }

}
?>
