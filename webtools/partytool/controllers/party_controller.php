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
vendor('webServices');
uses('sanitize');
class PartyController extends AppController {
  var $name = 'Party';
  var $components = array('RequestHandler');
  var $components = array('Security');
  
  function beforeFilter() {
    $this->Security->requirePost('rsvp','unrsvp');
  }
  
  function index() {
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="initMashUp()" onunload="GUnload()"');
  }
  
  function register() {
    if (!$this->Session->check('User')) {
      $this->redirect('/user');
    }
    
    $this->set('error', false);
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="mapInit(14.944785, -156.796875, 1)" onunload="GUnload()"');
    
    if (!empty($this->data)) {
      $clean = new Sanitize();
      $clean->cleanArray($this->data);
      
      $this->data['Party']['date'] = mktime($this->data['Party']['hour_hour'],
                                            $this->data['Party']['minute_min'],
                                            0,
                                            $this->data['Party']['month_hour'],
                                            $this->data['Party']['day_day'],
                                            $this->data['Party']['year_year']);
      
      $this->data['Party']['owner'] = $_SESSION['User']['id'];
      
      if (empty($this->data['Party']['lat']) && !empty($this->data['Party']['address']) &&
          $this->data['Party']['geocoded'] == 0) {
        
        // Attempt to geocode the address again
        $geocoder = new webServices(array('type' => 'geocode'));
        if ($ll = $geocoder->geocode($this->data['Party']['address']) != 0) {
          $this->data['Party']['lat'] = $ll['lat'];
          $this->data['Party']['long'] = $ll['lng'];
        }
        else {
          // May not come back with exactly what the user was looking for, but they can always edit
          $suggest = new webServices(array('type' => 'gsuggest'));
          if ($suggestion = $suggest->GSuggest($this->data['Party']['address']) != 0) {
            $this->data['Party']['address'] = $suggestion;
            if ($ll = $geocoder->geocode($suggestion) != 0) {
              $this->data['Party']['lat'] = $ll['lat'];
              $this->data['Party']['long'] = $ll['lng'];
            }
          }
        }
      }
      
      if ($this->Party->save($this->data))
        $this->redirect('party/view/'.$this->Party->getInsertId());
    }
  }
  
  function view($id = null, $page = null) {
    if ($id == "all") {
      $count = $this->Party->findCount();
      $pages = ceil($count/10);
      if ($page == null)
        $page = 1;
        
      if ($page > 1)
        $this->set('prev', $page - 1);
      if ($page < $pages)
        $this->set('next', $page + 1);
        
      $this->set('parties', $this->Party->findAll(null, null, "name ASC", 10, $page));
    }
    
    else if (is_numeric($id)) {
      $party = $this->Party->findById($id);
      $this->set('party', $party);
      
      if ($party['Party']['useflickr'] == 1) {
        $data = array('type' => 'flickr', 'userid' => $party['Party']['flickrid']);
        $flickr = new webServices($data);
        $this->set('flickr', ($flickr->fetchPhotos(FLICKR_TAG_PREFIX.$party['Party']['id'], 8)));
      }
      if (!empty($party['Party']['guests'])) {
        $guests = explode(',', $party['Party']['guests']);
        $names = array();
        
        for ($i = 0; $i < count($guests); $i++)
          array_push($names, $this->Party->getUserName($guests[$i]));
        
        $this->set('guests', $guests);
        $this->set('names', $names);
      }
      
      $this->set('host', $this->Party->getUserName($party['Party']['owner']));
      $this->set('comments', $this->Party->getComments($party['Party']['id']));      
      $this->set('body_args', ' onload="mapInit('.$party['Party']['lat'].', '.$party['Party']['long'].', '.$party['Party']['zoom'].', \'stationary\')" onunload="GUnload()"');
    }
    
    else {
      $this->redirect('/party/view/all');
    }
  }
  
  function rsvp($aParty = null) {
    if (!is_numeric($aParty))
      $this->redirect('/');

    $party = $this->Party->findById($aParty);
    $user = $this->Session->read('User');
    if (empty($user['id']))
      $this->redirect('/user/login');
    
    else {
      if (empty($party['Party']['guests'])) {
        $this->data['Party']['guests'] = $user['id'];
        $this->data['Party']['id'] = $aParty;
      }
    
      else {
        $attendees = explode(',', $party['Party']['guests']);
        if (in_array($user['id'], $attendees))
          $this->redirect('/party/view/'.$aParty);
    
        else {
          array_push($attendees, $user['id']);
          $csv = implode(',', $attendees);
          $this->data['Party']['guests'] = $csv;
        }
      }
    
      if ($this->Party->save($this->data))
        $this->redirect('/party/view/'.$aParty.'/added');
    }
  }
  
  function unrsvp($aParty) {
    $user = $this->Session->read('User');
    if (empty($user)) {
      $this->redirect('/user/login');
    }
    
    if (is_numeric($aParty)) {
      $party = $this->Party->findById($aParty);
      $temp = explode(',', $party['Party']['guests']);
      $id = array_search($user['id'], $temp);
      
      if (!empty($temp[$id])) { 
        unset($temp[$id]);
        $this->data['Party']['guests'] = implode(',', $temp);
        $this->data['Party']['id'] = $aParty;
        
        if ($this->Party->save($this->data))
          $this->redirect('/party/view/'.$aParty.'/removed');
      }
    }
    else
      $this->redirect('/');
  }
  
  function invite() {
    //XXX TODO
  }
  
  function js() {
    $this->layout = 'ajax';
    $this->set('parties', $this->Party->findAll());
  }
}
?>