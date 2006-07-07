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
            // If they've already signed up, send them another email
            if ($this->User->findByEmail($this->params['data']['User']['email'])) {
                    $mail_params = array(
                        'from' => 'nobody@mozilla.com',
                        'to'   => $this->params['data']['User']['email'],
                        'subject' => 'Firefox Surveys',
                        'message' => "
<p>Thanks for signing up for the Firefox Survey, Summer '06 edition. We'll start with
this profile survey and go from there.  We'll send out a few more surveys in the
following weeks to certain groups of survey takers.  Thanks again for making the
browser a better place!</p>

<p><a href=\"http://is4.instantsurvey.com/FirefoxSurveyNumeroUno?email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Start the first survey</a>
Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></p>

<p>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</p>

<p>If you choose not to take this survey, you will be automatically unsubscribed from
the Firefox Survey list.</p>

<p>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</p>
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
                        'from' => 'nobody@mozilla.com',
                        'to'   => $this->params['data']['User']['email'],
                        'subject' => 'Firefox Surveys',
                        'message' => "
<p>Thanks for signing up for the Firefox Survey, Summer '06 edition. We'll start with
this profile survey and go from there.  We'll send out a few more surveys in the
following weeks to certain groups of survey takers.  Thanks again for making the
browser a better place!</p>

<p><a href=\"http://is4.instantsurvey.com/FirefoxSurveyNumeroUno?email={$this->params['data']['User']['email']}&first={$this->params['data']['User']['firstname']}&last={$this->params['data']['User']['lastname']}\">Start the first survey</a>
Survey conducted by Mozilla and hosted by <a href=\"http://www.instantsurvey.com/\">Instant Survey</a></p>

<p>Mozilla continues to take security and privacy issues seriously.  Any data or
information provided will never be given or sold to any other outside company for its
use in marketing or solicitation.</p>

<p>If you choose not to take this survey, you will be automatically unsubscribed from
the Firefox Survey list.</p>

<p>If you think you received this in error, please <a href=\"mailto:firefoxsurvey@mozilla.com\">let us know</a>.</p>
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
