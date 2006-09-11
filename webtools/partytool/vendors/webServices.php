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
class webServices {
  
  var $userid;
  var $host;
  
  function webServices($data) {
    switch ($data['type']) {
      case "flickr":
        $this->host = "api.flickr.com";
        
        if (array_key_exists('userid', $data)) {
          $this->userid = $data['userid'];
        }
    
        if (array_key_exists('username', $data)) {
          $head  = "GET /services/rest/?method=flickr.people.findByUsername&api_key=".FLICKR_API_KEY."&username=".$data['username']." HTTP/1.1\r\n";
          $head .= "Host: ".$this->host."\r\n";
          $head .= "Connection: Close\r\n\r\n";
          
          if ($results = $this->fetchResults($head)) {
            preg_match('/nsid=\"(.*)\"/', $results, $matches);
            if ($matches[1]) {
              $this->userid = $matches[1];
            }
            else
              return 0;
          }
        }
        break;
        
      case "gsuggest":
        $this->host = "api.google.com";
        break;
        
      case "geocode":
        $this->host = "maps.google.com";
        break;
    }
  }
  
  function fetchPhotos($tags, $num_results) {
    $head  = "GET /services/rest/?method=flickr.photos.search&api_key=".FLICKR_API_KEY."&user_id=".$this->userid."&tags=".$tags."&per_page=".$num_results." HTTP/1.1\r\n";
    $head .= "Host: ".$this->host."\r\n";
    $head .= "Connection: Close\r\n\r\n";
    
    if ($results = $this->fetchResults($head)) {
      preg_match_all('/id="(.*)" own/', $results, $ids, PREG_SET_ORDER);
      preg_match_all('/owner="(.*)" sec/', $results, $owners, PREG_SET_ORDER);
      preg_match_all('/secret="(.*)" ser/', $results, $secrets, PREG_SET_ORDER);
      preg_match_all('/server="(.*)" tit/', $results, $servers, PREG_SET_ORDER);
      preg_match_all('/title="(.*)" isp/', $results, $titles, PREG_SET_ORDER);
      
      $arr = array();
      for ($i = 0; $i < count($ids); $i++) {
        $arr[$i] = array('id'    => $ids[$i][1],
                         'owner' => $owners[$i][1],
                         'secret' => $secrets[$i][1],
                         'server' => $servers[$i][1],
                         'title'  => $titles[$i][1]);
      }
      
      return $arr;
    }
    return 0;
  }
  
  function GSuggest($phrase) {
    $soapy = '<?xml version=\'1.0\' encoding=\'UTF-8\'?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
                   xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"
                   xmlns:xsd="http://www.w3.org/1999/XMLSchema">
  <SOAP-ENV:Body>
    <doSpellingSuggestion xmlns="urn:GoogleSearch">
      <key xsi:type="xsd:string">'.GSEARCH_API_KEY.'</key>
      <phrase xsi:type="xsd:string">'.$phrase.'</phrase>
    </doSpellingSuggestion>
  </SOAP-ENV:Body>
</SOAP-ENV:Envelope>';
    
    $head  = "POST /search/beta2 HTTP/1.1\r\n";
    $head .= "Host: api.google.com\r\n";
    $head .= "MessageType: CALL\r\n";
    $head .= "Content-type: text/xml\r\n";
    $head .= "Content-length: ".strlen($soapy)."\r\n";
    $head .= "Connection: Close\r\n\r\n";
    $head .= $soapy;
    
    if ($results = $this->fetchResults($head)) {
      if (preg_match('/return xsi:type="xsd:string">(.*)</', $results, $matches))
        return $matches[1];
      else
        return 0;
    }
    return 0;
  }
  
  function geocode($query) {
    $head = "GET /maps/geo?q=".urlencode($query)."&output=xml&key=".GMAP_API_KEY." HTTP/1.1\r\n";
    $head .= "Host: maps.google.com\r\n";
    $head .= "Connection: Close\r\n\r\n";
    
    if ($results = $this->fetchResults($head)) {
      if (stristr($results, '<code>200</code>')) {
        preg_match('/coordinates>(.*)</', $results, $matches);
        $ll = explode(',', $matches[1]);          
        $rv = array('lat' => $ll[0], 'lng' => $ll[1]);
        return $rv;
      }
    }
    return 0;
  }
  
  function fetchResults($headers) {
    $fs = fsockopen($this->host, 80, $errno, $errstr, 30);
    if (!$fs)
      return 0;
    
    else {
      fwrite($fs, $headers);
      
      $buffer = null;
      while (!feof($fs))
        $buffer .= fgets($fs, 128);  
  
      fclose($fs);
      
      return $buffer;
    }
  }
}
?>