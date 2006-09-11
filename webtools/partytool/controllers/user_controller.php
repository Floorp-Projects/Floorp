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
 * The Original Code is Firefox Party Tool
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
  var $helpers = array('Html');
  
  function index() {
    if (!$this->Session->check('User')) {
      $this->redirect('/user/login');
    }
    
    $user = $this->Session->read('User');
    $this->set('parties', $this->User->memberOf($user['id']));
    $this->set('hparties', $this->User->hostOf($user['id']));
  }
  
  function edit() {
    $this->set('error', false);
    $user = $this->User->findById($_SESSION['User']['id']);
    $this->set('user', $user);
    if (GMAP_API_KEY != null && !empty($user['User']['lat']))
      $this->set('body_args',
                 ' onload="mapInit('.$user["User"]["lat"].', '.$user["User"]["long"].', '.$user["User"]["zoom"].');" onunload="GUnload()"');
    
    if (!empty($this->data)) {
      //XXX TODO
      $this->redirect('/user/');
    }
  }
  
  function view($aUid = null) {
    if ($aUid === null || !is_numeric($aUid))
      $this->redirect('/');
    
    else {
      $user = $this->User->findById($aUid);
      $this->set('user', $user);
      if (GMAP_API_KEY != null && !empty($user['User']['lat']))
        $this->set('body_args',
                   ' onload="mapInit('.$user["User"]["lat"].', '.$user["User"]["long"].', '.$user["User"]["zoom"].', \'stationary\');" onunload="GUnload()"');
      $parties = $this->User->memberOf($user['User']['id']);
      $this->set('parties', $parties);
    }
  }
    
  function register() {
    $this->set('error', false);
    if ($this->Session->check('User')) {
      $this->redirect('/user/');
    }
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="mapInit()" onunload="GUnload()"');
    
    if (!empty($this->data)) {
      
      $clean = new Sanitize();
      $temp = array('email' => $this->data['User']['email'],
                    'password' => $this->data['User']['password'],
                    'confpassword' => $this->data['User']['confpassword'],
                    'lat' => $clean->sql($this->data['User']['lat']),
                    'long' => $clean->sql($this->data['User']['long']));
      //Nuke everything else
      $clean->cleanArray($this->data);

      $this->data['User']['email'] = $temp['email'];
      $this->data['User']['password'] = $temp['password'];
      $this->data['User']['confpassword'] = $temp['confpassword'];
      $this->data['User']['lat'] = $temp['lat'];
      $this->data['User']['long'] = $temp['long'];
      $this->data['User']['role'] = 0;
      
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
              $message = array(
                        'from' => 'Firefox Party <noreply@screwedbydesign.com>',
                        'envelope' => 'noreply@screwedbydesign.com',
                        'to'   => $this->data['User']['email'],
                        'subject' => 'Your Firefox Party Registration',
                        'message' => "You're almost ready to party! Just go to http://screwedbydesign.com/cake/user/activate/".$key." to activate your account.");
              
              $mail = new mail($message);
              $mail->send();
              
              $this->redirect('/user/login/new');
            }
          }
        
          else {
            $this->validateErrors($this->User);
            $this->render();
          }
        }
        
        else {
          $this->User->invalidate('password');
          $this->User->invalidate('confpassword');
          $this->render();
        }
      }
      
      else {
        $this->User->invalidate('email');
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
    if ($this->Session->check('User')) {
      $this->redirect('/user/');
    }
    
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
      
      if ($user['User']['active'] == 1 && $user['User']['password'] == sha1($this->data['User']['password'] . $user['User']['salt'])) {
        $this->Session->write('User', $user['User']);
        $this->redirect('/user/');
      }
      
      else {
        $this->set('error', true);
      }
    }
  }
  
  function logout() {
    $this->Session->delete('User');
    $this->redirect('/');
  }
  
  function recover($aType = null, $aCode = null, $aId = null) {
    switch ($aType) {
      case "password":
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
            $message = array('from'     => APP_NAME.'<'.APP_EMAIL.'>',
                           'envelope' => APP_EMAIL,
                           'to'       => $user['User']['email'],
                           'subject'  => APP_NAME.' Password Request',
                           'message'  => "Just go to http://screwedbydesign.com/cake/user/recover/password/".$code."/".$user['User']['id']." to reset your password.");
      
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
            $message = array('from'     => 'Firefox Party <noreply@screwedbydesign.com>',
                             'envelope' => 'noreply@screwedbydesign.com',
                             'to'       => $user['User']['email'],
                             'subject'  => 'Your Firefox Party Registration',
                             'message'  => "You're almost ready to party! Just go to http://screwedbydesign.com/cake/user/activate/".$user['User']['active']." to activate your account.");
      
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