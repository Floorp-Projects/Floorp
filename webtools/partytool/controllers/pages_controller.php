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
class PagesController extends AppController {
  var $name = 'Pages';
  var $components = array('Unicode');

  function display() {
    $this->pageTitle = 'Home';
    $this->set('current', 'home');
    $this->set('pcount', $this->Page->findCount());
    $this->set('ucount', $this->Page->getUsers());
    $text = $this->Page->query('SELECT text FROM pages WHERE id = 1');
    $time = $this->Page->query('SELECT text FROM pages WHERE id = 2');
    $this->set('time', $time[0]['pages']['text']);
    $this->set('front_text', preg_replace("/&#(\d{2,5});/e", 
                                          '$this->Unicode->unicode2utf(${1})',
                                          html_entity_decode($text[0]['pages']['text'])));
  }

  function privacy() {
    $this->pageTitle = 'Privacy Policy';
  }
  
  function edit() {
    if (isset($_SESSION['User']['id']) && $_SESSION['User']['role'] == 1) {
      if (empty($this->data)) {
        $text = $this->Page->query('SELECT text FROM pages WHERE id = 1');
        $time = $this->Page->query('SELECT text FROM pages WHERE id = 2');
        $this->data['Pages']['text'] = preg_replace("/&#(\d{2,5});/e", 
                                                    '$this->Unicode->unicode2utf(${1})',
                                                    html_entity_decode($text[0]['pages']['text']));
        $this->set('selected', date('Y-m-d H:i:s', $time[0]['pages']['text']));
      }
    
      else {
        // Paranoid? Nah...
        if ($_SESSION['User']['role'] == 1) {
          $clean = new Sanitize();
          $clean->cleanArray($this->data);
          $date = mktime($this->data['Pages']['date_hour'],
                         $this->data['Pages']['date_min'],
                         0,
                         $this->data['Pages']['date_month'],
                         $this->data['Pages']['date_day'],
                         $this->data['Pages']['date_year']);

          $this->Page->execute('UPDATE pages SET text = "'.$this->data['Pages']['text'].'" WHERE pages.id = 1');
          $this->Page->execute('UPDATE pages SET text = "'.$date.'" WHERE pages.id = 2');
          $this->redirect('/');
        }
      }
    }
    else
     die();
  }
}
?>
