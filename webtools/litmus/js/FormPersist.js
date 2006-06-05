/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * FormPersist.js derived from CFormData.
 * See <http://devedge-temp.mozilla.org/toolbox/examples/2003/CFormData/>
 */

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
 * The Original Code is Netscape code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bob Clary <bclary@netscape.com>
 *                 Bob Clary <http://bclary.com/>
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

function FormInit(/*HTMLFormElement */ aForm, /* String */ aQueryString)
{
  var type;
  var options;

  if (!aForm)
  {
    return;
  }

  if (!aQueryString)
  {
    return;
  }

  // empty out the form's values.
  var elements = aForm.elements;

  for (var iElm = 0; iElm < elements.length; iElm++)
  {
    var element  = elements[iElm];
    var nodeName = element.nodeName.toLowerCase();
      
    switch(nodeName)
    {
    case 'input':
      type = element.type.toLowerCase();
      switch(type)
      {
      case 'text':
        element.value = '';
        break;
      case 'radio':
      case 'checkbox':
        element.checked = false;
        break;
      }
      break;
    case 'textarea':
      element.value = '';
      break;
    case 'select':
      options = element.options;
      for (iOpt = 0; iOpt < options.length; iOpt++)
      {
        options[iOpt].selected = false;
      }
      break;
    }
  }

  // fill in form from the query string

  if (aQueryString.indexOf('?') == 0);
  {
    aQueryString = aQueryString.substring(1);
  }

  aQueryString = decodeURIComponent(aQueryString);
  aQueryString = aQueryString.replace(/[\&]amp;/g, '&');
  var parmList = aQueryString.split('&');

  for (var iParm = 0; iParm < parmList.length; iParm++)
  {
    var parms = parmList[iParm].split('=');
    var name  = parms[0];
    var value;

    if (parms.length == 1)
    {
      value = true;
    }
    else
    {
      var tempValue = parms[1];
      value = tempValue.replace(/\+/g,' ');
    }

    elements = aForm[name];

    if (!elements) {
      return;
    }

    if (typeof elements.nodeName != 'undefined')
    {
      elements = [elements];
    }

    for (iElm = 0; iElm < elements.length; iElm++)
    {
      element = elements[iElm];
      nodeName = element.nodeName.toLowerCase();
      
      switch(nodeName)
      {
      case 'input':
        type = element.type.toLowerCase();
        switch(type)
        {
        case 'text':
          element.value = value;
          break;
        case 'radio':
        case 'checkbox':
          if (element.value == value)
          {
            element.checked = true;
          }
        }
        break;
      case 'textarea':
        element.value = value;
        break;
      case 'select':
        options = element.options;
        for (iOpt = 0; iOpt < options.length; iOpt++)
        {
          if (options[iOpt].value == value)
          {
            options[iOpt].selected = true;;
          }
        }
        break;
      }
    }
  }
}

function FormDump(aForm)
{
  var type;
  var s = '';
  var elements = aForm.elements;

  for (var iElm = 0; iElm < elements.length; iElm++)
  {
    var element  = elements[iElm];
    var nodeName = element.nodeName.toLowerCase();
      
    switch(nodeName)
    {
    case 'input':
      type = element.type.toLowerCase();
      switch(type)
      {
      case 'text':
        if (element.value)
        {
          s += '&' + element.name + '=' + element.value;
        }
        break;
      case 'radio':
      case 'checkbox':
        if (element.checked)
        {
          s += '&' + element.name + '=' + element.value;
        }
        break;
      }
      break;
    case 'textarea':
      if (element.value)
      {
        s += '&' + element.name + '=' + element.value;
      }
      break;
    case 'select':
      var options = element.options;
      for (iOpt = 0; iOpt < options.length; iOpt++)
      {
        if (options[iOpt].selected)
        {
          s += '&' + element.name + '=' + options[iOpt].value;
        }
      }
      break;
    }
  }

  s = '?' + encodeURIComponent(s.slice(1));

  return s;
};

