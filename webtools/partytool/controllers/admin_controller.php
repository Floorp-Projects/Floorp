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
class AdminController extends AppController {
  var $name = 'Admin';
  var $uses = array('Party', 'User', 'Comment');

  function beforeFilter() {
    if (empty($_SESSION['User']) || $_SESSION['User']['role'] != 1) {
      $this->redirect('/');
      die();
    }
  }

  function index() {
    $this->set('parties', $this->Party->findAll());
  }
  
  function users() {
    $this->set('users', $this->User->findAll());
  }
  
  function comments() {
    $this->set('comments', $this->Comment->findAll());
  }
  
  function edit($type, $id) {
    if (empty($this->data)) {
      switch($type) {
        case 'user':
          $this->User->id = $id;
          $user = $this->User->read();
          $this->set('user', $user);
          $this->data = $user;
          break;
      
        case 'party':
          $this->Party->id = $id;
          $party = $this->Party->read();
          $this->set('party', $party);
          $this->data = $party;
          break;
        
        case 'comment':
          $this->Comment->id = $id;
          $comment = $this->Comment->read();
          $this->set('comment', $comment);

          $uid = $this->User->findById($comment['Comment']['owner']);
          $this->set('owner', $uid['User']['name']);

          $this->data = $comment;
          break;
      }
    }
    
    else {
      switch($type) {
        case 'user':
          $this->User->id = $id;
          $this->User->save($this->data);
          break;
      
        case 'party':
          $this->Party->id = $id;
          $this->Party->save($this->data);
          break;
        
        case 'comment':
          $this->Comment->id = $id;
          $this->Comment->save($this->data);
          break;
      }
    
      if ($type != 'party')
        $this->redirect('/admin/'.$type.'s');

      else
        $this->redirect('/admin/');
    }
  }
  
  function delete($type, $id) {
    switch($type) {
      case 'user':
        $this->User->del($id);
        $this->User->query("DELETE FROM guests WHERE uid = $id");
        break;
      
      case 'party':
        $this->Party->del($id);
        $this->Party->query("DELETE FROM guests WHERE pid = $id");
        $this->Party->query("DELETE FROM comments WHERE assoc = $id");
        break;
        
      case 'comment':
        $this->Comment->del($id);
        break;
    }
    
    if ($type != 'party')
      $this->redirect('/admin/'.$type.'s');

    else
      $this->redirect('/admin/');
  }
}
?>
