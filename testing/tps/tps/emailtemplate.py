# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime

def GenerateEmailBody(data, numpassed, numfailed, serverUrl, buildUrl):

  now = datetime.datetime.now()
  builddate = datetime.datetime.strptime(data['productversion']['buildid'],
                                         '%Y%m%d%H%M%S')
  tree = data['productversion']['repository']

  row = """
<tr>
  <td><a href="http://hg.mozilla.org/services/services-central/file/default/services/sync/tests/tps/{name}">{name}</a></td>
  <td>{state}</td>
  <td>{message}</td>
</tr>
"""

  rowWithLog = """
<tr>
  <td><a href="http://hg.mozilla.org/services/services-central/file/default/services/sync/tests/tps/{name}">{name}</a></td>
  <td>{state}</td>
  <td>{message} [<a href="{logurl}">view log</a>]</td>
</tr>
"""

  rows = ""
  for test in data['tests']:
    if test.get('logurl'):
      rows += rowWithLog.format(name=test['name'],
                         state=test['state'],
                         message=test['message'] if test['message'] else 'None',
                         logurl=test['logurl'])
    else:
      rows += row.format(name=test['name'],
                         state=test['state'],
                         message=test['message'] if test['message'] else 'None')

  firefox_version = data['productversion']['version']
  if buildUrl is not None:
    firefox_version = "<a href='%s'>%s</a>" % (buildUrl, firefox_version)
  body = """
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
    <title>TPS</title>
    <style type="text/css">
#headertable {{ border: solid 1px black; margin-bottom: 2em; border-collapse: collapse; font-size: 0.8em; }}
#headertable th {{ border: solid 1px black; background-color: lightgray; padding: 4px; }}
#headertable td {{ border: solid 1px black; padding: 4px; }}
.light {{ color: gray; }}
.pass, a.pass:link, a.pass:visited {{ color: green; font-weight: bold; }}
.fail, a.fail:link, a.fail:visited {{ color: red; font-weight: bold; }}
.rightgray {{ text-align: right; background-color: lightgray; }}
#summarytable {{ border: solid 1px black; margin-bottom: 2em; border-collapse: collapse; font-size: 0.8em; }}
#summarytable th {{ border: solid 1px black; background-color: lightgray; padding: 4px; }}
#summarytable td {{ border: solid 1px black; padding: 4px; }}
</style>
</head>

<body>
    <div id="content">
        
<h2>TPS Testrun Details</h2>

<table id="headertable">

<tr>
  <td class="rightgray">Testrun Date</td>
  <td>{date}</td>

</tr>
<tr>
  <td class="rightgray">Firefox Version</td>
  <td>{firefox_version}</td>
</tr>
<tr>
  <td class="rightgray">Firefox Build Date</td>
  <td>{firefox_date}</td>
</tr>

<tr>
  <td class="rightgray">Firefox Sync Version / Type</td>
  <td>{sync_version} / {sync_type}
  </td>
</tr>
<tr>
  <td class="rightgray">Firefox Sync Changeset</td>
  <td>

    <a href="{repository}/rev/{changeset}">

      {changeset}</a> / {sync_tree}

  </td>
</tr>
<tr>
  <td class="rightgray">Sync Server</td>
  <td>{server}</td>
</tr>
<tr>
  <td class="rightgray">OS</td>
  <td>{os}</td>
</tr>
<tr>
  <td class="rightgray">Passed Tests</td>

  <td>
  <span class="{passclass}">{numpassed}</span>
  </td>
</tr>
<tr>
  <td class="rightgray">Failed Tests</td>
  <td>

  <span class="{failclass}">{numfailed}</span>
  </td>
</tr>
</table>


<table id="summarytable">
<thead>
<tr>
  <th>Testcase</th>
  <th>Result</th>
  <th>Message</th>
</tr>
</thead>

{rows}

</table>

    </div>
</body>
</html>

""".format(date=now.ctime(),
           firefox_version=firefox_version,
           firefox_date=builddate.ctime(),
           sync_version=data['addonversion']['version'],
           sync_type=data['synctype'],
           sync_tree=tree[tree.rfind("/") + 1:],
           repository=data['productversion']['repository'],
           changeset=data['productversion']['changeset'],
           os=data['os'],
           rows=rows,
           numpassed=numpassed,
           numfailed=numfailed,
           passclass="pass" if numpassed > 0 else "light",
           failclass="fail" if numfailed > 0 else "light",
           server=serverUrl if serverUrl != "" else "default"
          )

  return body
