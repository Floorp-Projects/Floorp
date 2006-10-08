<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Party Tool
 *
 * The Initial Developer of the Original Code is
 * Ryan Flint <rflint@dslr.net>
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
uses('sanitize');
class UsersController extends AppController {
  var $name = 'Users';
  var $uses = array('User', 'Party');
  var $helpers = array('Html', 'Form');
  var $components = array('Security', 'Hash', 'Mail');

  function index() {
    if (!isset($_SESSION['User'])) {
      $this->redirect('/users/login');
    }

    $this->pageTitle = 'My Profile';
    
    $user = $this->Session->read('User');
    $this->set('parties', $this->User->memberOf($user['id']));
    $this->set('hparties', $this->User->hostOf($user['id']));
    $this->set('iparties', $this->User->invitedTo($user['id']));
  }

  function register() {
    $this->pageTitle = 'Register';
    $this->set('map', 'mapInit()');

    if(empty($this->data)) {
      $this->set('utz', '0');
      $this->render();
    }

    else {
      if ($this->User->findByEmail($this->data['User']['email']))
        $this->User->invalidate('email');

      if ($this->data['User']['email'] !== $this->data['User']['confemail'])
        $this->User->invalidate('confemail');

      if ($this->data['User']['password'] !== $this->data['User']['confpass'])
        $this->User->invalidate('confpass');
      
      if (empty($this->data['User']['password']) || empty($this->data['User']['confpass']))
        $this->User->invalidate('password');

      // Repopulate the timezone with right value in case there's a validation error
      $this->set('utz', $this->data['User']['tz']);

      if ($this->User->validates($this->data)) {
        $clean = new Sanitize();
        // Generate and set the password, salt and activation key
        $pass = $this->Hash->password($this->data['User']['password'],
                                      $this->data['User']['email']);
        $this->data['User']['active'] = $this->Hash->keygen(10);
        $this->data['User']['password'] = $pass['pass'];
        $this->data['User']['salt'] = $pass['salt'];

        // Save a few fields from the wrath of cleanArray()
        $temp = array('lat'   => $this->data['User']['lat'],
                      'long'  => $this->data['User']['long'],
                      'tz'    => $this->data['User']['tz'],
                      'email' => $this->data['User']['email']);
        // Scrub 'a dub
        $clean->cleanArray($this->data);
        $this->data['User']['email'] = $temp['email'];
        $this->data['User']['long']  = floatval($temp['long']);
        $this->data['User']['lat']   = floatval($temp['lat']);
        $this->data['User']['tz']    = intval($temp['tz']);
        $this->data['User']['role']  = 0;

        if($this->User->save($this->data)) {
          $message = array('from' => APP_NAME.' <'.APP_EMAIL.'>',
                           'envelope' => APP_EMAIL,
                           'to'   => $this->data['User']['email'],
                           'subject' => 'Your '.APP_NAME.' Registration',
                           'link' => APP_BASE.'/users/activate/'.$this->data['User']['active'],
                           'type' => 'act');
          $this->Mail->mail($message);
          $this->Mail->send();

          if (isset($_SESSION['invite']))
            $this->Party->addGuest($this->User->getLastInsertId(), $_SESSION['invite']);

          $this->Session->setFlash('Thank you for registering! To login, you\'ll
                                   need to activate your account. Please check
                                   your email for your activation link.', 'infoFlash');
          $this->redirect('/users/login');
        }
        
        else {
          $this->data['User']['password'] = null;
          $this->data['User']['confpass'] = null;
          $this->render();
        }
      }

      else {
        $this->data['User']['password'] = null;
        $this->data['User']['confpass'] = null;
        $this->Session->setFlash('There was an error in your submission. Please
                                  correct the errors shown below and try again.',
                                 'errorFlash');
        $this->render();
      }
    }
  }

  function edit($id) {
    if (!isset($_SESSION['User'])) {
      $this->redirect('/users/login');
    }
    $this->set('error', false);
    $this->pageTitle = 'Edit My Account';
    if (empty($this->data)) {
      $this->User->id = $_SESSION['User']['id'];
      $this->data = $this->User->read();
      $this->data['User']['password'] = "";
      $this->set('utz', $this->data['User']['tz']);

      $this->data['User']['name'] = preg_replace("/&#(\d{2,5});/e", 
                                                 '$this->Unicode->unicode2utf(${1})',
                                                 html_entity_decode($this->data['User']['name']));
      $this->data['User']['website'] = preg_replace("/&#(\d{2,5});/e", 
                                                    '$this->Unicode->unicode2utf(${1})',
                                                    html_entity_decode($this->data['User']['website']));
      $this->data['User']['location'] = preg_replace("/&#(\d{2,5});/e", 
                                                     '$this->Unicode->unicode2utf(${1})',
                                                     html_entity_decode($this->data['User']['location']));

      if (GMAP_API_KEY != null) {
          if ($this->data['User']['lat'])
            $this->set('map', 'mapInit('.$this->data['User']['lat'].','.$this->data['User']['long'].','.$this->data['User']['zoom'].')');
          else
            $this->set('map', 'mapInit()');
        }
    }

    else {
      $user = $this->User->findById($_SESSION['User']['id']);
      $this->User->id = $user['User']['id'];
      $this->set('utz', $user['User']['tz']);

      $clean = new Sanitize();
      $temp = array('password' => $this->data['User']['password'],
                    'confpassword' => $this->data['User']['confpassword'],
                    'lat' => $clean->sql($this->data['User']['lat']),
                    'long' => $clean->sql($this->data['User']['long']),
                    'tz' => $clean->sql($this->data['User']['tz']));
      //Nuke everything else
      $clean->cleanArray($this->data);

      $this->data['User']['email'] = $user['User']['email'];
      $this->data['User']['password'] = $temp['password'];
      $this->data['User']['confpassword'] = $temp['confpassword'];
      $this->data['User']['lat'] = floatval($temp['lat']);
      $this->data['User']['long'] = floatval($temp['long']);
      $this->data['User']['tz'] = intval($temp['tz']);
      $this->data['User']['role'] = $user['User']['role'];

      if (!empty($this->data['User']['password']) && !empty($this->data['User']['confpassword'])) {
        if ($this->data['User']['password'] === $this->data['User']['confpassword']) {
          $string = $user['User']['email'].uniqid(rand(), true).$this->data['User']['password'];
          $this->data['User']['salt'] = substr(md5($string), 0, 9);
          $this->data['User']['password'] = sha1($this->data['User']['password'] . $this->data['User']['salt']);
        }
        else {
          $this->set('error', true);
          $this->User->invalidate('password');
          $this->User->invalidate('confpassword');
        }
      }

      if ($this->User->validates($this->data)) {
        if ($this->User->save($this->data)) {
          $sess = $this->User->findById($user['User']['id']);
          $this->redirect('/users/');
        }
      }
      
      else {
        $this->validateErrors($this->User);
        $this->data['User']['password'] = null;
        $this->data['User']['confpassword'] = null;
        $this->render();
      }
    }
  }

  function login() {
    if ($this->Session->Check('User'))
      $this->redirect('/users');

    $this->pageTitle = 'Login';
    if (!empty($this->data)) {
      if (empty($this->data['User']['email']) || empty($this->data['User']['password']))
        $this->render();

      $user = $this->User->findByEmail($this->data['User']['email']);
      $pass = sha1($this->data['User']['password'].$user['User']['salt']);

      if ($user['User']['password'] == $pass) {
        if ($user['User']['active'] != 1) {
          $this->Session->setFlash('Your account hasn\'t been activated yet. Please
                                  check your email (including junk/spam folders)
                                  for your activation link, or click <a href="'
                                  .APP_BASE.'/users/recover/activate">here</a> to
                                  resend your activation details.', 'infoFlash');
          $this->render();
        }
        
        else {
          if (isset($_SESSION['invite']))
            $this->Party->addGuest($user['User']['id'], $_SESSION['invite']);

          $this->Session->write('User', $user['User']);
          $this->redirect('/users/');
        }
      }

      else {
        $this->Session->setFlash('The email address and password you supplied do
                                 not match. Please try again.', 'errorFlash');
      }
    }
  }

  function view($id = null) {
    if (!is_numeric($id))
      $this->redirect('/');

    else {
      $user = $this->User->findById($id);
      $this->pageTitle = $user['User']['name'];
      $this->set('user', $user);
      if (GMAP_API_KEY != null && !empty($user['User']['lat']))
        $this->set('map', $user['User']['lat'].','.$user['User']['long'].','.$user['User']['zoom'].',\'stationary\'');

      $this->Party->unbindModel(array('hasMany' => array('Comment')));
      $this->set('hparties', $this->Party->findByOwner($id));
      $att = $this->User->query('SELECT parties.id, parties.name
                                 FROM parties 
                                 LEFT JOIN guests
                                  ON parties.id = guests.pid 
                                 WHERE guests.uid = '.$id);
      $this->set('parties', $att);
    }
  }

  function logout() {
    $this->Session->destroy();
    $this->Session->delete('User');
    $this->redirect('/');
  }

   function recover($aType = null, $aCode = null, $aId = null) {
    switch ($aType) {
      case "password":
        $this->pageTitle = "Password Recovery";
        $this->set('atitle', 'Password Recovery');
        $this->set('hideInput', false);
        $this->set('url', 'password');

        if (!empty($this->data)) {
          $user = $this->User->findByEmail($this->data['User']['email']);

          if (!isset($user['User']['email'])) {
            $this->Session->setFlash('Could not find a user with that email address. Please check it and try again.', 'errorFlash');
            $this->render();
          }
          else {
            $code = md5($user['User']['salt'].$user['User']['email'].$user['User']['password']);
            $message = array('from'   => APP_NAME.'<'.APP_EMAIL.'>',
                           'envelope' => APP_EMAIL,
                           'to'       => $user['User']['email'],
                           'subject'  => APP_NAME.' Password Request',
                           'link'     => APP_BASE.'/users/recover/password/'.$code.'/'.$user['User']['id'],
                           'type'     => 'prec');

            $this->Mail->mail($message);
            $this->Mail->send();
            $this->Session->setFlash('An email has been sent to '.$user['User']['email'].' with reset instructions.', 'errorFlash');
            $this->redirect('users/login');
          }
        }

        if ($aCode !== null && $aId !== null) {
          $this->set('hideInput', true);
          $this->set('reset', false);
          $user = $this->User->findById($aId);

          if (!$user) {
            $this->Session->setFlash('Invalid request. Please check the URL and try again.', 'errorFlash');
            $this->render();
          }

          if ($aCode == md5($user['User']['salt'].$user['User']['email'].$user['User']['password'])) {
            $this->set('reset', true);
            $this->set('code', $aCode."/".$aId);
            $this->render();
          }

          else {
            $this->Session->setFlash('Invalid request. Please check the URL and try again.', 'errorFlash');
            $this->render();
          }
        }
        break;
      case "activate":
        $this->pageTitle = 'Resend Activation Code';
        $this->set('atitle', 'Resend Activation Code');
        $this->set('hideInput', false);
        $this->set('url', 'activate');

        if (!empty($this->data)) {
          $user = $this->User->findByEmail($this->data['User']['email']);
          
          if (!$user) {
            $this->Session->setFlash('Could not find a user with that email address. Please check it and try again.', 'errorFlash');
            $this->render();
          }

          if ($user['User']['active'] == 1)
            $this->redirect('/users/login');

          else {
            $message = array('from'     => APP_NAME.' <'.APP_EMAIL.'>',
                             'envelope' => APP_EMAIL,
                             'to'       => $this->data['User']['email'],
                             'subject'  => 'Your '.APP_NAME.' Registration',
                             'link'     => APP_BASE.'/users/activate/'.$user['User']['active'],
                             'type'     => 'act');
            $this->Mail->mail($message);
            $this->Mail->send();
            $this->Session->setFlash('Your activation code has been resent.', 'infoFlash');
            $this->redirect('users/login');
          }
        }
        break;
      case "reset":
        if ($aCode !== null && $aId !== null) {
          if (!empty($this->data)) {
            $user = $this->User->findById($aId);
            if (!$user) {
              $this->Session->setFlash('Invalid request. Please check the URL and try again.', 'errorFlash');
              $this->render();
            }

            if ($aCode == md5($user['User']['salt'].$user['User']['email'].$user['User']['password'])) {
              $string = $user['User']['email'] . uniqid(rand(), true) . $this->data['User']['password'];
              $this->data['User']['salt'] = substr(md5($string), 0, 9);
              $this->data['User']['password'] = sha1($this->data['User']['password'] . $this->data['User']['salt']);
              $this->data['User']['id'] = $aId;
              if ($this->User->save($this->data)) {
                $this->Session->setFlash('Your password has been reset.', 'infoFlash');
                $this->redirect('/users/login');
              }
            }
          }
        }
        break;
      default:
        $this->redirect('/');
        break;
    }
  }

  function activate($aKey = null) {
    if ($aKey == null)
      $this->redirect('/');

    else {
      $user = $this->User->findByActive($aKey);
      if (empty($user['User']['id'])) {
        $this->Session->setFlash('Your account could not be activated. Please make
          sure the URL entered is correct and try again.', 'errorFlash');
        $this->redirect('/users/login');
      }

      else {
        $this->data = $user;
        $this->data['User']['active'] = 1;

        if ($this->User->save($this->data)) {
          $this->Session->setFlash('Your account was successfully activated.', 'infoFlash');
          $this->redirect('/users/login');
        }
      }
    }
  }
}
?>
