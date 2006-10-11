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
vendor('webServices');
class PartiesController extends AppController {
  var $name = 'Parties';
  var $uses = array('Party', 'Comment');
  var $helpers = array('Html', 'Form');
  var $components = array('Hash', 'Mail', 'Unicode');

  function index() {
    $this->pageTitle = 'Party Map';
    $this->set('current', 'map');
    $this->set('map', 'initMashUp()');
  }

  function add() {
    if (!$this->Session->check('User'))
      $this->redirect('/users/login');

    $this->pageTitle = 'Create Party';
    $this->set('current', 'create');
    $this->set('map', 'mapInit()');

    if(empty($this->data)) {
      $this->set('utz', $_SESSION['User']['tz']);
      $this->render();
    }

    else {
        $temp = array('lat'  => $this->data['Party']['lat'],
                      'long' => $this->data['Party']['long'],
                      'tz'   => $this->data['Party']['tz']);

        $clean = new Sanitize();
        $clean->cleanArray($this->data);

        $this->data['Party']['lat']  = floatval($temp['lat']);
        $this->data['Party']['long'] = floatval($temp['long']);
        $this->data['Party']['tz']   = intval($temp['tz']);
        $this->set('utz', $this->data['Party']['tz']);

        // Convert the selected time to GMT
        $secoffset = ($this->data['Party']['tz'] * 60 * 60);
        $offsetdate = gmmktime($this->data['Party']['hour_hour'],
                             $this->data['Party']['minute_min'],
                             0,
                             $this->data['Party']['month_hour'],
                             $this->data['Party']['day_day'],
                             $this->data['Party']['year_year']);
        $this->data['Party']['date'] = ($offsetdate + $secoffset);
        $this->data['Party']['duration'] = intval($this->data['Party']['duration']);

        $this->data['Party']['invitecode'] = $this->Hash->keygen(10);
        $this->data['Party']['owner'] = $_SESSION['User']['id'];

        if ($this->Party->validates($this->data)) {
          if($this->Party->save($this->data)) {
            $this->Session->setFlash('Your party has been created!', 'infoFlash');
            $this->redirect('/parties/view/'.$this->Party->getLastInsertId());
          }
        }
        
        else {
          $this->Session->setFlash('Please correct errors below.', 'errorFlash');
        }
    }
  }

  function edit($id) {
      $this->Party->id = $id;
    $party = $this->Party->read();
    $this->set('party', $party);
    $this->pageTitle = 'Edit Party';
    $this->set('current', 'create');

    if (empty($_SESSION['User']['id']))
      $this->redirect('/users/login/');

    if ($party['Party']['owner'] != $_SESSION['User']['id'])
      $this->redirect('/parties/view/'.$id);

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
        $this->data['Party']['name'] = preg_replace("/&#(\d{2,5});/e", 
                                                    '$this->Unicode->unicode2utf(${1})',
                                                    html_entity_decode($this->data['Party']['name']));
        $this->data['Party']['vname'] = preg_replace("/&#(\d{2,5});/e", 
                                                     '$this->Unicode->unicode2utf(${1})',
                                                     html_entity_decode($this->data['Party']['vname']));
        $this->data['Party']['website'] = preg_replace("/&#(\d{2,5});/e", 
                                                       '$this->Unicode->unicode2utf(${1})',
                                                       html_entity_decode($this->data['Party']['website']));
        $this->data['Party']['address'] = preg_replace("/&#(\d{2,5});/e", 
                                                       '$this->Unicode->unicode2utf(${1})',
                                                       html_entity_decode($this->data['Party']['address']));
        $this->data['Party']['notes'] = preg_replace("/&#(\d{2,5});/e", 
                                                     '$this->Unicode->unicode2utf(${1})',
                                                     html_entity_decode($this->data['Party']['notes']));
        $this->data['Party']['flickrusr'] = preg_replace("/&#(\d{2,5});/e", 
                                                         '$this->Unicode->unicode2utf(${1})',
                                                         html_entity_decode($this->data['Party']['flickrusr']));

        if (GMAP_API_KEY != null) {
          if ($this->data['Party']['lat'])
            $this->set('map', 'mapInit('.$this->data['Party']['lat'].','.$this->data['Party']['long'].','.$this->data['Party']['zoom'].')');
          else
            $this->set('map', 'mapInit()');
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

        if ($this->Party->save($this->data)) {
          $this->Session->setFlash('Party edited successfully.', 'infoFlash');
          $this->redirect('parties/view/'.$id);
        }
      }
    }
  }

  function view($id = null, $page = null) {
    if ($id == 'all') {
      $this->pageTitle = 'All Parties';
      $this->set('current', 'parties');

      //Paginate!
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
      if (empty($party['Party']['id']))
        $this->redirect('/parties/view/all');

      $this->set('current', 'parties');
      $this->set('host', $this->Party->getHost($party['Party']['owner']));
      $this->set('party', $party);
      $this->set('isguest', $this->Party->isGuest($id, @$_SESSION['User']['id']));
      $this->pageTitle = $party['Party']['name'];
      $this->set('map', 'mapInit('.$party['Party']['lat'].','.$party['Party']['long'].
                        ','.$party['Party']['zoom'].',\'stationary\')');
      $this->set('guests', $this->Party->getGuests($party['Party']['id']));
      $this->set('comments', $this->Party->getComments($id));

      if (FLICKR_API_KEY != null) {
        if ($party['Party']['useflickr'] == 1) {
          $data = array('type' => 'flickr', 'userid' => $party['Party']['flickrid'], 'randomize' => true);
          $flickr = new webServices($data);
          $photoset = $flickr->fetchPhotos(FLICKR_TAG_PREFIX.$party['Party']['id'], 15, (($party['Party']['flickrperms']) ? false : true));
          $this->set('flickr', array_slice($photoset, 0, 9));
        }
      }
    }

    else
      $this->redirect('/parties/view/all');
  }

  function invite($id = null) {
    $this->pageTitle = "Invite a Guest";
    if (is_numeric($id)) {
      $party = $this->Party->findById($id);
      if (empty($party['Party']['id']) ||
          $party['Party']['owner'] != $_SESSION['User']['id'])
        $this->redirect('/parties/view/all');

      else {
        $this->set('partyid', $party['Party']['id']);
        $this->set('inviteurl', APP_BASE.'/parties/invited/'.$party['Party']['invitecode']);

        $clean = new Sanitize();
        $uid = $clean->sql($_SESSION['User']['id']);
        $email = $this->Party->query("SELECT email FROM users WHERE id = ".$uid);

        if (!empty($this->data)) {
          if ($this->Party->validates($this->data)) {
            $message = array('from'     => APP_NAME.' <'.APP_EMAIL.'>',
                             'envelope' => APP_EMAIL,
                             'to'       => $this->data['Party']['einvite'],
                             'reply'    => $email[0]['users']['email'],
                             'subject'  => 'You\'ve been invited to '.APP_NAME.'!',
                             'link'     => APP_BASE.'/parties/invited/'.$party['Party']['invitecode'],
                             'type'     => 'invite');

            $this->Mail->mail($message);
            $this->Mail->send();

            $this->Session->setFlash($this->data['Party']['einvite'].' has been
              invited. You can invite another guest below or <a href="'.APP_BASE.'/parties/view/'.$id.'/">click here</a>
              to return to your party.', 'infoFlash');
            $this->data['Party']['einvite'] = null;
          }
          else {
            $this->validateErrors($this->Party);
            $this->render();
          }
        }
      }
    }
  }

  function invited($icode = null, $conf = null) {
    $this->pageTitle = "Confirm Invite";
    if ($icode == 'cancel') {
      $this->Session->delete('invite');
      $this->Session->delete('invitestep');
      $this->redirect('/');
    }

    else {
      $clean = new Sanitize();
      $icode = $clean->sql($icode);
      $party = $this->Party->findByInvitecode($icode);

      if (empty($party['Party']['id'])) {
        $this->Session->setFlash('Could not find a party matching that invite code, please check it and try again.', 'errorFlash');
      }

      else {
        if (!empty($_SESSION['User']['id']) && !empty($_SESSION['invitestep']) && $conf == 'confirm') {
          $this->Party->addGuest($_SESSION['User']['id'], $_SESSION['invite']);
          $this->Session->setFlash('You have been successfully added to this party.', 'infoFlash');
          $this->redirect('/parties/view/'.$party['Party']['id']);
        }

        else if (!empty($_SESSION['User']['id'])) {
          $this->set('confirm_only', true);
          $this->set('party', $party);
          $this->set('icode', $icode);
          $this->Session->write('invitestep', 'true');
          $this->Session->write('invite', $icode);
        }

        else {
          $this->Session->write('invite', $icode);
          $this->set('party', $party);
          $this->set('icode', $icode);
        }
      }
    }
  }

  function rsvp($pid) {
    if (is_numeric($pid) && isset($_SESSION['User']['id'])) {
      $party = $this->Party->findById($pid);
      if (empty($party['Party']['id'])) {
        $this->Session->setFlash('Invalid party id.', 'errorFlash');
        $this->redirect('/parties/view/all');
      }

      else {
        if ($party['Party']['inviteonly']) {
          $this->Session->setFlash('This party invite only, you\'ll need an
            invitation from the host to join in', 'errorFlash');
        }

        else {
          $this->Party->rsvp($pid, $_SESSION['User']['id']);
          $this->Session->setFlash('You have been successfully added to this party.', 'infoFlash');
          $this->redirect('/parties/view/'.$pid);
        }
      }
    }

    else
      $this->redirect('/parties/view/all');
  }

  function unrsvp($pid) {
    if (is_numeric($pid) && isset($_SESSION['User']['id'])) {
      $party = $this->Party->findById($pid);
      if (empty($party['Party']['id'])) {
        $this->Session->setFlash('Invalid party id.', 'errorFlash');
        $this->redirect('/parties/view/all');
      }

      else {
        $this->Party->unrsvp($pid, $_SESSION['User']['id']);
        $this->Session->setFlash('You have been successfully removed from this party.', 'infoFlash');
        $this->redirect('/parties/view/'.$pid);
      }
    }

    else
      $this->redirect('/parties/view/all');
  }

  function js() {
    $this->layout = 'ajax';
    header('Content-type: text/javascript');
    $parties = $this->Party->findAll();
    $this->set('parties', $parties);
  }
}
?>
