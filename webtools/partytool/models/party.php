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
class Party extends AppModel {
  var $name = 'Party';
  var $validate = array(
    'name' => VALID_NOT_EMPTY,
    'einvite' => VALID_EMAIL,
    'duration' => VALID_NUMBER
  );

  function getComments($pid) {
    $rv = $this->query("SELECT users.id AS uid, users.name,
                          comments.id AS cid, comments.time, comments.text
                        FROM users, parties, comments
                        WHERE comments.assoc = ".$pid."
                          AND users.id = comments.owner
                          AND parties.id = ".$pid."
                        ORDER BY cid ASC");
    return $rv;
  }

  function getHost($uid) {
    $rv = $this->query("SELECT name FROM users WHERE id = ".$uid);
    return @$rv[0]['users']['name'];
  }

  function isGuest($pid, $uid) {
    $rv = $this->query('SELECT id FROM guests WHERE uid = '.$uid.' AND pid = '.$pid);
    if (!empty($rv[0]['guests']['id']))
      return true;
    else
      return false;
  }

  function getGuests($pid) {
    $rv = $this->query("SELECT users.id, users.name, users.email, guests.invited
                        FROM users
                        LEFT JOIN guests 
                          ON users.id = guests.uid
                        WHERE guests.pid = ".$pid);
    return $rv;
  }

  function rsvp($pid, $uid) {
    $party = $this->findById($pid);
    if (!empty($party['Party']['id']) && !$this->isGuest($pid, $uid)) {
      $this->query("INSERT INTO guests (id, pid, uid, invited)
                        VALUES (NULL, ".$party['Party']['id'].", ".$uid.", 0)"); 
    }
  }

  function unrsvp($pid, $uid) {
    $party = $this->findById($pid);
    if (!empty($party['Party']['id']) && $this->isGuest($pid, $uid)) {
      $this->query('DELETE FROM guests WHERE uid = '.$uid.' AND pid = '.$pid);
    }
  }

  function addGuest($uid, $icode) {
    $party = $this->findByInvitecode($icode);
    if (!empty($party['Party']['id'])) {
      $check = $this->query('SELECT uid FROM guests WHERE uid = '.$uid.' 
                              AND pid = '.$party['Party']['id']);
      if (empty($check[0]['guests']['uid']) && $uid != $party['Party']['owner']) {
        $this->query("INSERT INTO guests (id, pid, uid, invited)
                        VALUES (NULL, ".$party['Party']['id'].", ".$uid.", 1)");
      }
    }
  }
  
  function findByInvitecode($icode) {
    $rv = $this->query('SELECT * FROM parties AS Party WHERE invitecode = "'.$icode.'" LIMIT 1');
    return @$rv[0];
  }
}
?>
