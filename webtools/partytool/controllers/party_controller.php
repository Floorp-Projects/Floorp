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
vendor('webServices');
vendor('mail');
uses('sanitize');
class PartyController extends AppController {
  var $name = 'Party';
  var $pageTitle;
  var $components = array('Security');

  function beforeFilter() {
    $this->Security->requirePost('unrsvp');
  }

  function index() {
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="initMashUp()" onunload="GUnload()"');
    $this->pageTitle = APP_NAME." - Party Map";
    $this->set('current', "map");
  }

  function register() {
    if (!$this->Session->check('User')) {
      $this->redirect('/user/login');
    }

    $this->pageTitle = APP_NAME." - Register";
    $this->set('current', "create");

    $this->set('error', false);
    if (GMAP_API_KEY != null)
      $this->set('body_args', ' onload="mapInit(14.944785, -156.796875, 1)" onunload="GUnload()"');

    if (!empty($this->data)) {
      $clean = new Sanitize();
      $temp = array('lat'  => $clean->sql($this->data['Party']['lat']),
                    'long' => $clean->sql($this->data['Party']['long']),
                    'tz'   => $clean->sql($this->data['Party']['tz']));

      $clean->cleanArray($this->data);
 
      $this->data['Party']['lat'] = floatval($temp['lat']);
      $this->data['Party']['long'] = floatval($temp['long']);
      $this->data['Party']['tz'] = intval($temp['tz']);

      $secoffset = ($this->data['Party']['tz'] * 60 * 60);

      $offsetdate = gmmktime($this->data['Party']['hour_hour'],
                             $this->data['Party']['minute_min'],
                             0,
                             $this->data['Party']['month_hour'],
                             $this->data['Party']['day_day'],
                             $this->data['Party']['year_year']);

      $this->data['Party']['date'] = ($offsetdate + $secoffset);
      $this->data['Party']['owner'] = $_SESSION['User']['id'];
      $this->data['Party']['duration'] = intval($this->data['Party']['duration']);

      $key = null;
      $chars = "1234567890abcdefghijklmnopqrstuvwxyz";
      for ($i = 0; $i < 10; $i++) {
        $key .= $chars{rand(0,35)};
      }

      $this->data['Party']['invitecode'] = $key;
      
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
          $geocoder = new webServices(array('type' => 'geocode'));
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

  function edit($id = null) {
    $this->Party->id = $id;
    $party = $this->Party->read();
    $this->set('party', $party);
    $this->pageTitle = APP_NAME." - Edit Party";
    $this->set('current', "create");

    if (empty($_SESSION['User']['id']))
      $this->redirect('/user/login/');

    if ($party['Party']['owner'] != $_SESSION['User']['id'])
      $this->redirect('/party/view/'.$id);

    else {
      if (empty($this->data)) {
        $this->data = $party;
        
        $date = array('hour' => intval(date('h', $party['Party']['date'])),
                      'min'  => intval(date('i', $party['Party']['date'])),
                      'mon'  => intval(date('m', $party['Party']['date'])),
                      'day'  => intval(date('d', $party['Party']['date'])),
                      'year' => intval(date('Y', $party['Party']['date'])),
                      'tz'   => $party['Party']['tz']);
                      
        $this->set('date', $date);

        if (GMAP_API_KEY != null) {
          if ($this->data['Party']['lat'])
            $this->set('body_args',
                       ' onload="mapInit('.$this->data["Party"]["lat"].', '.$this->data["Party"]["long"].', '.$this->data["Party"]["zoom"].');" onunload="GUnload()"');
          else
            $this->set('body_args',
                       ' onload="mapInit(1, 1, 1);" onunload="GUnload()"');
        }
      }

      else {
        $clean = new Sanitize();
        $temp = array('lat'  => $clean->sql($this->data['Party']['lat']),
                      'long' => $clean->sql($this->data['Party']['long']),
                      'tz'   => $clean->sql($this->data['Party']['tz']));

        $clean->cleanArray($this->data);
 
        $this->data['Party']['lat'] = floatval($temp['lat']);
        $this->data['Party']['long'] = floatval($temp['long']);
        $this->data['Party']['tz'] = intval($temp['tz']);

        $secoffset = ($this->data['Party']['tz'] * 60 * 60);

        $offsetdate = gmmktime($this->data['Party']['hour_hour'],
                               $this->data['Party']['minute_min'],
                               0,
                               $this->data['Party']['month_hour'],
                               $this->data['Party']['day_day'],
                               $this->data['Party']['year_year']);

        $this->data['Party']['date'] = ($offsetdate - $secoffset);
        $this->data['Party']['owner'] = $party['Party']['owner'];
        $this->data['Party']['duration'] = intval($this->data['Party']['duration']);

        if ($this->data['Party']['flickrusr'] != $party['Party']['flickrusr']) {
          $params = array('type' => 'flickr', 'username' => $this->data['Party']['flickrusr']);
          $flick = new webServices($params);
          $this->data['Party']['flickrid'] = $flick->getFlickrId();
        }

        if ($this->Party->save($this->data))
          $this->redirect('party/view/'.$id);
      }
    }
  }

  function view($id = null, $page = null) {
    if ($id == "all") {
      $this->pageTitle = APP_NAME." - All Parties";
      $this->set('current', "parties");
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

      $this->pageTitle = APP_NAME." - ".$party['Party']['name'];
      $this->set('current', "parties");

      if (FLICKR_API_KEY != null) {
        if ($party['Party']['useflickr'] == 1) {
          $data = array('type' => 'flickr', 'userid' => $party['Party']['flickrid']);
          $flickr = new webServices($data);
          $photoset = $flickr->fetchPhotos(FLICKR_TAG_PREFIX.$party['Party']['id'], 15, (($party['Party']['flickrperms']) ? false : true));
          $this->set('flickr', array_slice($photoset, 0, 9));
        }
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
  
  function rsvp($aParty = null, $icode = null) {
    if (!is_numeric($aParty))
      $this->redirect('/');

    $invited = false;

    if ($icode != null) {
      $party = $this->Party->findByInvitecode($icode);
      if ($aParty != $party['Party']['id'])
        $this->redirect('/party/view/'.$aParty);
      else
        $invited = true;
    }
    
    else
      $party = $this->Party->findById($aParty);
      
    $user = $this->Session->read('User');
    if (empty($_SESSION['User']['id']))
      $this->redirect('/user/login');
    
    else if ($party['Party']['inviteonly'] != 1 || $invited === true) {
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

    else
      $this->redirect('/party/view/'.$aParty);
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

  function invite($id = null) {
    $this->pageTitle = APP_NAME." - Invite a guest";
    if (is_numeric($id) && isset($_SESSION['User'])) {
      $party = $this->Party->findById($id);
      if ($party['Party']['owner'] === $_SESSION['User']['id']) {
        $this->set('partyid', $party['Party']['id']);
        $this->set('inviteurl', APP_BASE.'/register/'.$party['Party']['invitecode']);

        if (!empty($this->data)) {
          if ($this->Party->validates($this->data)) {
            $message = array('from' => APP_NAME.' <'.APP_EMAIL.'>',
                             'envelope' => APP_EMAIL,
                             'to'   => $this->data['Party']['einvite'],
                             'subject' => 'You\'ve been invited to '.APP_NAME.'!',
                             'link' => APP_BASE.'/user/register/'.$party['Party']['invitecode'],
                             'type' => 'invite');

            $mail = new mail($message);
            $mail->send();
            $this->set('preamble', array($this->data['Party']['einvite'], $id));
          }
          else {
            $this->validateErrors($this->Party);
            $this->render();
          }
        }
      }
      else
        $this->redirect('/party/view/'.$id);
    }
    else
      $this->redirect('/user/login');
  }
  
  function js() {
    $this->layout = 'ajax';
    $this->set('parties', $this->Party->findAll());
  }
}
?>