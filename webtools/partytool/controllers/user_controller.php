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
vendor('mail');
uses('sanitize');
class UserController extends AppController {
  var $name = 'User';
  var $pageTitle;

  function index() {
    if (!isset($_SESSION['User'])) {
      $this->redirect('/user/login');
    }
    
    $this->pageTitle = APP_NAME." - My Profile";
    
    $user = $this->Session->read('User');
    $this->set('parties', $this->User->memberOf($user['id']));
    $this->set('hparties', $this->User->hostOf($user['id']));
  }

  function edit() {
    if (!isset($_SESSION['User'])) {
      $this->redirect('/user/login');
    }
    $this->set('error', false);
    $this->pageTitle = APP_NAME." - Edit My Account";
    if (empty($this->data)) {
      $this->User->id = $_SESSION['User']['id'];
      $this->data = $this->User->read();
      $this->data['User']['password'] = "";
      $this->set('utz', $this->data['User']['tz']);
      
      if (GMAP_API_KEY != null && !empty($this->data['User']['lat']))
      $this->set('body_args',
                 ' onload="mapInit('.$this->data["User"]["lat"].', '.$this->data["User"]["long"].', '.$this->data["User"]["zoom"].');" onunload="GUnload()"');
    }

    else {
      $user = $this->User->findById($_SESSION['User']['id']);
      $this->User->id = $user['User']['id'];
      
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

      if (!empty($this->data['User']['password'])) {
        if ($this->data['User']['password'] === $this->data['User']['confpassword']) {
          $string = $user['User']['email'].uniqid(rand(), true).$this->data['User']['password'];
          $this->data['User']['salt'] = substr(md5($string), 0, 9);
          $this->data['User']['password'] = sha1($this->data['User']['password'] . $this->data['User']['salt']);
        }
        else {
          $this->set('error', true);
          $this->render();
        }
      }
      else
        $this->data['User']['password'] = $user['User']['password'];

      if ($this->User->save($this->data)) {
        $sess = $this->User->findById($user['User']['id']);
        $this->Session->destroy();
        $this->Session->delete('User');
        $this->Session->write('User', $sess['User']);
        $this->redirect('/user/');
      }
    }
  }

  function view($aUid = null) {
    if (!is_numeric($aUid))
      $this->redirect('/');

    else {
      $user = $this->User->findById($aUid);
      $this->pageTitle = APP_NAME." - ".$user['User']['name'];
      $this->set('user', $user);
      if (GMAP_API_KEY != null && !empty($user['User']['lat']))
        $this->set('body_args',
                   ' onload="mapInit('.$user["User"]["lat"].', '.$user["User"]["long"].', '.$user["User"]["zoom"].', \'stationary\');" onunload="GUnload()"');

      $this->set('parties', $this->User->memberOf($user['User']['id']));
      $this->set('hparties', $this->User->hostOf($user['User']['id']));
    }
  }

  function register($invite = null) {
    $this->set('error', false);
    if (isset($_SESSION['User'])) {
      if ($invite != null) {
        $this->redirect('/party/rsvp/'.$this->User->getPartyId($invite).'/'.$invite);
      }
      else
        $this->redirect('/user/');
    }

    if (empty($this->data))
      $this->set('icode', $invite);

    $this->pageTitle = APP_NAME." - Register";
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="mapInit()" onunload="GUnload()"');
    
    if (!empty($this->data)) {
      $clean = new Sanitize();
      $temp = array('email' => $this->data['User']['email'],
                    'cemail' => $this->data['User']['confemail'],
                    'password' => $this->data['User']['password'],
                    'confpassword' => $this->data['User']['confpassword'],
                    'lat' => $clean->sql($this->data['User']['lat']),
                    'long' => $clean->sql($this->data['User']['long']),
                    'tz' => $clean->sql($this->data['User']['tz']));
      //Nuke everything else
      $clean->cleanArray($this->data);

      $this->data['User']['email'] = $temp['email'];
      $this->data['User']['confemail'] = $temp['cemail'];
      $this->data['User']['password'] = $temp['password'];
      $this->data['User']['confpassword'] = $temp['confpassword'];
      $this->data['User']['lat'] = floatval($temp['lat']);
      $this->data['User']['long'] = floatval($temp['long']);
      $this->data['User']['role'] = 0;
      $this->data['User']['tz'] = intval($temp['tz']);

      if ($this->data['User']['email'] === $temp['cemail']) {
        if (!$this->User->findByEmail($this->data['User']['email'])) {
          if ($this->data['User']['password'] === $this->data['User']['confpassword']) {
            if ($this->User->validates($this->data)) {
              $string = $this->data['User']['email'].uniqid(rand(), true).$this->data['User']['password'];
              $this->data['User']['salt'] = substr(md5($string), 0, 9);
              $this->data['User']['password'] = sha1($this->data['User']['password'] . $this->data['User']['salt']);

              $key = null;
              $chars = "1234567890abcdefghijklmnopqrstuvwxyz";
              for ($i = 0; $i < 10; $i++) {
                $key .= $chars{rand(0,35)};
              }

              $this->data['User']['active'] = $key;

              if ($this->User->save($this->data)) {
                $message = array('from' => APP_NAME.' <'.APP_EMAIL.'>',
                                 'envelope' => APP_EMAIL,
                                 'to'   => $this->data['User']['email'],
                                 'subject' => 'Your '.APP_NAME.' Registration',
                                 'link' => APP_BASE.'/user/activate/'.$key,
                                 'type' => 'act');

                $mail = new mail($message);
                $mail->send();

                if (!empty($this->data['User']['icode']))
                  $this->User->addToParty($this->data['User']['icode'], $this->User->getLastInsertID());

                $this->redirect('/user/login/new');
              }
            }

            else {
              $this->validateErrors($this->User);
              $this->data['User']['password'] = null;
              $this->data['User']['confpassword'] = null;
              $this->render();
            }
          }
        
          else {
            $this->User->invalidate('confpassword');
            $this->data['User']['password'] = null;
            $this->data['User']['confpassword'] = null;
            $this->render();
          }
        }

        else {
          $this->User->invalidate('email');
          $this->data['User']['password'] = null;
          $this->data['User']['confpassword'] = null;
          $this->render();
        }
      }
      
      else {
        $this->User->invalidate('confemail');
        $this->data['User']['password'] = null;
        $this->data['User']['confpassword'] = null;
        $this->render();
      }
    }
  }
  
  function activate($aKey = null) {
    if ($aKey == null)
      $this->redirect('/');

    else {
      $this->data = $this->User->findByActive($aKey);
      $this->data['User']['active'] = 1;
      
      if ($this->User->save($this->data)) {
        $this->redirect('/user/login/active');
      }
    }
  }

  function login($isNew = null) {
    if (isset($_SESSION['User'])) {
      $this->redirect('/user/');
    }

    $this->pageTitle = APP_NAME." - Login";
    $this->set('error', false);

    if ($isNew !== null) {
      switch($isNew) {
        case "new":
          $this->set('preamble', 'Thank you for registering! To login, you\'ll need to activate your account. Please check your email for your activation link.');
          break;

        case "rnew":
          $this->set('preamble', 'An email with instructions on how to reset your password has been sent.');
          break;

        case "active":
          $this->set('preamble', 'Your account has been activated. You may now login.');
          break;

        case "reset":
          $this->set('preamble', 'Your password has been reset.');
          break;
      }
      $this->render();
    }
    
    if (!empty($this->data)) {
      $user = $this->User->findByEmail($this->data['User']['email']);

      if ($user['User']['active'] != 1) {
        $this->set('preamble', 'Your account hasn\'t been activated yet. 
        Please check your email (including junk/spam folders) for your
        activation link, or click <a href="/cake/user/recover/activate">here</a>
        to resend your activation details.');
        $this->render();
      }

      if ($user['User']['active'] == 1 && $user['User']['password'] == sha1($this->data['User']['password'].$user['User']['salt'])) {
        $this->Session->write('User', $user['User']);
        $this->redirect('/user/');
      }

      else {
        $this->set('error', true);
      }
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
        $this->pageTitle = APP_NAME." - Password Recovery";
        $this->set('atitle', 'Password Recovery');
        $this->set('hideInput', false);
        $this->set('url', 'password');
        
        if (!empty($this->data)) {
          $user = $this->User->findByEmail($this->data['User']['email']);

          if (!isset($user['User']['email'])) {
            $this->set('error', 'Could not find a user with that email address. Please check it and try again.');
            $this->render();
          }
          else {
            $code = md5($user['User']['salt'].$user['User']['email'].$user['User']['password']);
            $message = array('from'   => APP_NAME.'<'.APP_EMAIL.'>',
                           'envelope' => APP_EMAIL,
                           'to'       => $user['User']['email'],
                           'subject'  => APP_NAME.' Password Request',
                           'link'     => APP_BASE.'/user/recover/password/'.$code.'/'.$user['User']['id'],
                           'type'     => 'prec');

            $mail = new mail($message);
            $mail->send();
            $this->redirect('user/login/rnew');
          }
        }

        if ($aCode !== null && $aId !== null) {
          $this->set('hideInput', true);
          $this->set('reset', false);
          $user = $this->User->findById($aId);

          if (!$user) {
            $this->set('error', 'Invalid request. Please check the URL and try again.');
            $this->render();
          }

          if ($aCode == md5($user['User']['salt'].$user['User']['email'].$user['User']['password'])) {
            $this->set('reset', true);
            $this->set('code', $aCode."/".$aId);
            $this->render();
          }

          else {
            $this->set('error', 'Invalid request. Please check the URL and try again.');
            $this->render();
          }
        }
        break;
      case "activate":
        $this->pageTitle = APP_NAME." - Resend Activation Code";
        $this->set('atitle', 'Resend Activation Code');
        $this->set('hideInput', false);
        $this->set('url', 'activate');

        if (!empty($this->data)) {
          $user = $this->User->findByEmail($this->data['User']['email']);
          
          if (!$user) {
            $this->set('error', 'Could not find a user with that email address. Please check it and try again.');
            $this->render();
          }

          if ($user['User']['active'] == 1)
            $this->redirect('/user/login/active');

          else {
            $message = array('from'     => APP_NAME.' <'.APP_EMAIL.'>',
                             'envelope' => APP_EMAIL,
                             'to'       => $this->data['User']['email'],
                             'subject'  => 'Your '.APP_NAME.' Registration',
                             'link'     => APP_BASE.'/user/activate/'.$user['User']['active'],
                             'type'     => 'act');
            $mail = new mail($message);
            $mail->send();
            $this->redirect('user/login/new');
          }
        }
        break;
      case "reset":
        if ($aCode !== null && $aId !== null) {
          if (!empty($this->data)) {
            $user = $this->User->findById($aId);
            if (!$user) {
              $this->set('error', 'Invalid request. Please check the URL and try again.');
              $this->render();
            }

            if ($aCode == md5($user['User']['salt'].$user['User']['email'].$user['User']['password'])) {
              $string = $user['User']['email'] . uniqid(rand(), true) . $this->data['User']['password'];
              $this->data['User']['salt'] = substr(md5($string), 0, 9);
              $this->data['User']['password'] = sha1($this->data['User']['password'] . $this->data['User']['salt']);
              $this->data['User']['id'] = $aId;
              if ($this->User->save($this->data))
                $this->redirect('/user/login/reset');
            }
          }
        }
        break;
      default:
        $this->redirect('/');
        break;
    }
  }

  function delete($id) {
    $role = $this->Session->read('User');
    if ($role['role'] != 2)
      die("Access denied.");
    else {
      $this->User->del($id);
      $this->redirect('/');
    }
  }
}
?>