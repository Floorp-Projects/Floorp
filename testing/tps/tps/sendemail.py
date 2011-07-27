# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is TPS.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jonathan Griffin <jgriffin@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

import smtplib
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText

def SendEmail(From=None, To=None, Subject='No Subject', 
              TextData=None, HtmlData=None,
              Server='mail.mozilla.com', Port=465,
              Username=None, Password=None):
  """Sends an e-mail.
  
     From is an e-mail address, To is a list of e-mail adresses.
     
     TextData and HtmlData are both strings.  You can specify one or both.
     If you specify both, the e-mail will be sent as a MIME multipart
     alternative; i.e., the recipient will see the HTML content if his
     viewer supports it, otherwise he'll see the text content.
  """

  if From is None or To is None:
    raise Exception("Both From and To must be specified")
  if TextData is None and HtmlData is None:
    raise Exception("Must specify either TextData or HtmlData")

  server = smtplib.SMTP_SSL(Server, Port)

  if Username is not None and Password is not None:
    server.login(Username, Password)

  if HtmlData is None:
    msg = MIMEText(TextData)
  elif TextData is None:
    msg = MIMEMultipart()
    msg.preamble = Subject
    msg.attach(MIMEText(HtmlData, 'html'))
  else:
    msg = MIMEMultipart('alternative')
    msg.attach(MIMEText(TextData, 'plain'))
    msg.attach(MIMEText(HtmlData, 'html'))

  msg['Subject'] = Subject
  msg['From'] = From
  msg['To'] = ', '.join(To)

  server.sendmail(From, To, msg.as_string())

  server.quit()
