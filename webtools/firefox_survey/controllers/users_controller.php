<?php
vendor('mail/mail');
class UsersController extends AppController {

    var $name = 'Users';

    var $helpers = array('html');

    function index() 
    {
    }

    function add()
    {
        if (empty($this->params['data']))
        {
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
                        'from' => '"Firefox Surveys" <nobody@mozilla.com>',
                        'to'   => $this->params['data']['User']['email'],
                        'subject' => 'Firefox Surveys',
                        'message' => "
<p>Thanks for volunteering to be a part of the Firefox Survey!</p>

<p>We'll start with this profile survey and go from there. We'll send out a few more
surveys in the following weeks to the user panel to help us define the next
generation of Firefox. Thanks again for making the browser a better place!</p>

<p><a href=\"http://is4.instantsurvey.com/FirefoxSurveyNumeroUno?email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Start the first survey</a></p>


<p><small>Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></small></p>

<p><small>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</small></p>

<p><small>If you choose not to take this survey, you will be automatically unsubscribed from the Firefox Survey list.</small></p>

<p><small>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</small></p>
"
                    );

                    $mail = new mail($mail_params);

                    $mail->send();

                    // rather than just flash a screen, we want to show a page with
                    // some content.  We do a little trickery here, but it gets the
                    // job done.  content is in /views/layouts/flash.thtml
                    $this->Session->setFlash('
                    <p>You are already signed up for the Firefox Survey!</p>

                    <p>If you don\'t receive an email with a link to the survey
                    within 24 hours, please <a href="mailto:firefoxsurvey@mozilla.com?subject=invalid email - firefox survey">let us know</a> 
                    and we\'ll send another copy.</p>

                    <p>Continue to the <a href="http://google.com/firefox/">Firefox Start Page</a></p> 
                    ');
                    $this->flash(null,null,0);
                    return;

            }
            if ($this->User->save($this->params['data'])) {
                    $mail_params = array(
                        'from' => '"Firefox Surveys" <nobody@mozilla.com>',
                        'to'   => $this->params['data']['User']['email'],
                        'subject' => 'Firefox Surveys',
                        'message' => "
<p>Thanks for volunteering to be a part of the Firefox Survey!</p>

<p>We'll start with this profile survey and go from there. We'll send out a few more
surveys in the following weeks to the user panel to help us define the next
generation of Firefox. Thanks again for making the browser a better place!</p>

<p><a href=\"http://is4.instantsurvey.com/FirefoxSurveyNumeroUno?email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Start the first survey</a></p>


<p><small>Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></small></p>

<p><small>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</small></p>

<p><small>If you choose not to take this survey, you will be automatically unsubscribed from the Firefox Survey list.</small></p>

<p><small>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</small></p>
"
                    );


                    $mail = new mail($mail_params);

                    $mail->send();

                    // rather than just flash a screen, we want to show a page with
                    // some content.  We do a little trickery here, but it gets the
                    // job done.  content is in /views/layouts/flash.thtml
                    $this->Session->setFlash('Thanks for helping to make a better browsing experience.<br />Continue to the <a href="http://google.com/firefox/">Firefox Start Page</a>');
                    $this->flash(null,null,0);
            }
        }

    }

}
?>
