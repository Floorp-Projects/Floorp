# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code are the Bugzilla tests.
#
# The Initial Developer of the Original Code is Jacob Steenhagen.
# Portions created by Jacob Steenhagen are
# Copyright (C) 2001 Jacob Steenhagen. All
# Rights Reserved.
#
# Contributor(s): Gervase Markham <gerv@gerv.net>

# Important! The following classes of directives are excluded in the test,
# and so do not need to be added here. Doing so will cause warnings.
# See 008filter.t for more details.
#
# Comments                        - [%#...
# Directives                      - [% IF|ELSE|UNLESS|FOREACH...
# Assignments                     - [% foo = ...
# Simple literals                 - [% " selected" ...
# Values always used for numbers  - [% (i|j|k|n|count) %]
# Params                          - [% Param(...
# Safe functions                  - [% (time2str|GetBugLink)...
# Safe vmethods                   - [% foo.size %]
# TT loop variables               - [% loop.count %]
# Already-filtered stuff          - [% wibble FILTER html %]
#   where the filter is one of html|csv|js|url_quote|quoteUrls|time|uri|xml

# Key:
#
# "#": directive should be filtered, but not doing so is not a security hole
# The plan is to come back and add filtering for all those marked "#" after
# the security release.
#
# "# Email": as above; but noting that it's an email address.
# Other sorts of comments denote cleanups noticed while doing this work;
# they should be fixed in the very short term.

%::safe = (

'sidebar.xul.tmpl' => [
  'template_version', 
],

'flag/list.html.tmpl' => [
  'flag.id', 
  'flag.status', 
  'type.id', 
],

'search/boolean-charts.html.tmpl' => [
  '"field${chartnum}-${rownum}-${colnum}"', 
  '"value${chartnum}-${rownum}-${colnum}"', 
  '"type${chartnum}-${rownum}-${colnum}"', 
  'field.name', 
  'field.description', 
  'type.name', 
  'type.description', 
  '"${chartnum}-${rownum}-${newor}"', 
  '"${chartnum}-${newand}-0"', 
  'newchart',
  'jsmagic',
],

'search/form.html.tmpl' => [
  'qv.value',
  'qv.name',
  'qv.description',
  'field.name',
  'field.description',
  'field.accesskey',
  'sel.name',
  'sel.accesskey',
  'button_name', #
],

'search/knob.html.tmpl' => [
  'button_name', #
],

'search/search-report-graph.html.tmpl' => [
  'button_name', #
],

'search/search-report-table.html.tmpl' => [
  'button_name', #
],

'request/queue.html.tmpl' => [
  'column_headers.$group_field', 
  'column_headers.$column', 
  'request.status', 
  'request.bug_id', 
  'request.attach_id', 
],

'reports/components.html.tmpl' => [
  'numcols',
  'numcols - 1',
  'comp.description', 
  'comp.initialowner', # email address
  'comp.initialqacontact', # email address
],

'reports/duplicates-simple.html.tmpl' => [
  'title', #
],

'reports/duplicates-table.html.tmpl' => [
  '"&maxrows=$maxrows" IF maxrows',
  '"&changedsince=$changedsince" IF changedsince',
  '"&product=$product" IF product', #
  '"&format=$format" IF format', #
  '"&bug_id=$bug_ids_string&sortvisible=1" IF sortvisible',
  'column.name', 
  'column.description',
  'vis_bug_ids.push(bug.id)', 
  'bug.id', 
  'bug.count', 
  'bug.delta', 
  'bug.component', #
  'bug.bug_severity', #
  'bug.op_sys', #
  'bug.target_milestone', #
],

'reports/duplicates.html.tmpl' => [
  'bug_ids_string', 
  'maxrows',
  'changedsince',
  'reverse',
],

'reports/keywords.html.tmpl' => [
  'keyword.description', 
  'keyword.bugcount', 
],

'reports/report-table.csv.tmpl' => [
  '"$tbl_field_disp: $tbl\n" IF tbl_field', #
  'row_field_disp IF row_field', #
  'col_field_disp', #
  'num_bugs',
  'data.$tbl.$col.$row',
  '', # This is not a bug in the filter exceptions - this template has an 
      # empty directive which is necessary for it to work properly.
],

'reports/report-table.html.tmpl' => [
  'buglistbase', 
  '"&amp;$tbl_vals" IF tbl_vals', 
  '"&amp;$col_vals" IF col_vals', 
  '"&amp;$row_vals" IF row_vals', 
  'tbl_disp', # 
  'classes.$row_idx.$col_idx', 
  'urlbase', 
  'data.$tbl.$col.$row', 
  'row_total',
  'col_totals.$col',
  'grand_total', 
],

'reports/report.html.tmpl' => [
  'tbl_field_disp IF tbl_field', #
  'row_field_disp IF row_field', #
  'col_field_disp', #
  'imagebase', 
  'width', 
  'height', 
  'imageurl', 
  'formaturl', 
  'other_format.name', 
  'other_format.description', #
  'sizeurl', 
  'height + 100', 
  'height - 100', 
  'width + 100', 
  'width - 100', 
  'switchbase',
  'format',
  'cumulate',
],

'reports/duplicates.rdf.tmpl' => [
  'template_version', 
  'bug.id', 
  'bug.count', 
  'bug.delta', 
],

'reports/chart.html.tmpl' => [
  'width', 
  'height', 
  'imageurl', 
  'sizeurl', 
  'height + 100', 
  'height - 100', 
  'width + 100', 
  'width - 100', 
],

'reports/series-common.html.tmpl' => [
  'sel.name', 
  'sel.accesskey', 
  '"onchange=\'$sel.onchange\'" IF sel.onchange', 
],

'reports/chart.csv.tmpl' => [
  'data.$j.$i', 
],

'reports/create-chart.html.tmpl' => [
  'series.series_id', 
  'newidx',
],

'reports/edit-series.html.tmpl' => [
  'default.series_id', 
],

'list/change-columns.html.tmpl' => [
  'column', 
  'field_descs.${column} || column', #
],

'list/edit-multiple.html.tmpl' => [
  'group.id', 
  'group.description',
  'group.description FILTER strike', 
  'knum', 
  'menuname', 
],

'list/list-simple.html.tmpl' => [
  'title', 
],

'list/list.html.tmpl' => [
  'buglist', 
  'bugowners', # email address
],

'list/list.rdf.tmpl' => [
  'template_version', 
  'bug.bug_id', 
  'column', 
],

'list/table.html.tmpl' => [
  'id', 
  'splitheader ? 2 : 1', 
  'abbrev.$id.title || field_descs.$id || column.title', #
  'tableheader',
  'bug.bug_severity', #
  'bug.priority', #
  'bug.bug_id', 
],

'list/list.csv.tmpl' => [
  'bug.bug_id', 
],

'list/list.js.tmpl' => [
  'bug.bug_id', 
],

'global/help.html.tmpl' => [
  'h.id', 
  'h.html', 
],

'global/banner.html.tmpl' => [
  'VERSION', 
],

'global/choose-product.html.tmpl' => [
  'target',
  'proddesc.$p', 
],

'global/code-error.html.tmpl' => [
  'parameters', 
  'bug.bug_id',
  'field', 
  'argument', #
  'function', #
  'bug_id', # Need to remove unused error no_bug_data
  'variables.id', 
  'template_error_msg', # Should move filtering from CGI.pl to template
  'error', 
  'error_message', 
],

'global/header.html.tmpl' => [
  'javascript', 
  'style', 
  'style_url', 
  'bgcolor', 
  'onload', 
  'h1',
  'h2',
  'h3', 
  'message', 
],
'global/hidden-fields.html.tmpl' => [
  'mvalue | html | html_linebreak', # Need to eliminate | usage
  'field.value | html | html_linebreak',
],

'global/messages.html.tmpl' => [
  'parameters',
  '# ---', # Work out what this is
  'namedcmd', #
  'old_email', # email address
  'new_email', # email address
  'message_tag', 
  'series.frequency * 2',
],

'global/select-menu.html.tmpl' => [
  'options', 
  'onchange', # Again, need to be certain where we are filtering
  'size', 
],

'global/useful-links.html.tmpl' => [
  'email', 
  'user.login', # Email address
],

# Need to change this and code-error to use a no-op filter, for safety
'global/user-error.html.tmpl' => [
  'disabled_reason', 
  'bug_link', 
  'action', # 
  'bug_id', 
  'both', 
  'filesize', 
  'attach_id', 
  'field', 
  'field_descs.$field', 
  'today', 
  'product', #
  'max', 
  'votes', 
  'error_message', 
],

'global/confirm-user-match.html.tmpl' => [
  '# use the global field descs', # Need to fix commenting style here
  'script',
  '# this is messy to allow later expansion',
  '# ELSIF for things that don\'t belong in the field_descs hash here',
  'fields.${field_name}.flag_type.name',
],

'global/site-navigation.html.tmpl' => [
  'bug_list.first', 
  'bug_list.$prev_bug', 
  'bug_list.$next_bug', 
  'bug_list.last', 
  'bug.bug_id', 
  'bug.votes', 
],

'bug/comments.html.tmpl' => [
  'comment.isprivate', 
  'comment.when', 
],

'bug/dependency-graph.html.tmpl' => [
  'image_map', # We need to continue to make sure this is safe in the CGI
  'image_url', 
  'map_url', 
  'bug_id', 
],

'bug/dependency-tree.html.tmpl' => [
  'hide_resolved ? "Open b" : "B"', 
  'bugid', 
  'maxdepth', 
  'dependson_ids.join(",")', 
  'blocked_ids.join(",")', 
  'dep_id', 
  'hide_resolved ? 0 : 1', 
  'hide_resolved ? "Show" : "Hide"', 
  'realdepth < 2 || maxdepth == 1 ? "disabled" : ""', 
  'hide_resolved', 
  'realdepth < 2 ? "disabled" : ""', 
  'maxdepth + 1', 
  'maxdepth == 0 || maxdepth == realdepth ? "disabled" : ""', 
  'realdepth < 2 || ( maxdepth && maxdepth < 2 ) ? "disabled" : ""',
  'maxdepth > 0 && maxdepth <= realdepth ? maxdepth : ""',
  'maxdepth == 1 ? 1 
                       : ( maxdepth ? maxdepth - 1 : realdepth - 1 )',
  'realdepth < 2 || ! maxdepth || maxdepth >= realdepth ?
            "disabled" : ""',
],

'bug/edit.html.tmpl' => [
  'bug.remaining_time', 
  'bug.delta_ts', 
  'bug.bug_id', 
  'bug.votes', 
  'group.bit', 
  'group.description', 
  'knum', 
  'dep.title', 
  'dep.fieldname', 
  'accesskey', 
  'bug.${dep.fieldname}.join(\', \')', 
  'selname',
  'depbug FILTER bug_link(depbug)',
  '"bug ${bug.dup_id}" FILTER bug_link(bug.dup_id)',
],

'bug/navigate.html.tmpl' => [
  'this_bug_idx + 1', 
  'bug_list.first', 
  'bug_list.last', 
  'bug_list.$prev_bug', 
  'bug_list.$next_bug', 
],

'bug/show-multiple.html.tmpl' => [
  'bug.bug_id', 
  'bug.component', #
  'attr.description', #
],

'bug/show.xml.tmpl' => [
  'VERSION', 
  'a.attachid', 
  'field', 
],

'bug/time.html.tmpl' => [
  'time_unit FILTER format(\'%.1f\')', 
  'time_unit FILTER format(\'%.2f\')', 
  '(act / (act + rem)) * 100 
       FILTER format("%d")', 
],

'bug/votes/list-for-bug.html.tmpl' => [
  'voter.count', 
  'total', 
],

'bug/votes/list-for-user.html.tmpl' => [
  'product.maxperbug', 
  'bug.id', 
  'bug.count', 
  'product.total', 
  'product.maxvotes', 
],
# h2 = voting_user.name # Email

'bug/process/confirm-duplicate.html.tmpl' => [
  'original_bug_id', 
  'duplicate_bug_id', 
],

'bug/process/midair.html.tmpl' => [
  'bug_id', 
],

'bug/process/next.html.tmpl' => [
  'bug.bug_id', 
],

'bug/process/results.html.tmpl' => [
  'title.$type', 
  'id', 
],

'bug/process/verify-new-product.html.tmpl' => [
  'form.product', # 
],

'bug/process/bugmail.html.tmpl' => [
  'description', 
  'name', # Email 
],

'bug/create/comment.txt.tmpl' => [
  'form.comment', 
],

'bug/create/create.html.tmpl' => [
  'default.bug_status', #
  'g.bit',
  'g.description',
  'sel.name',
  'sel.description',
],

'bug/create/create-guided.html.tmpl' => [
  'matches.0', 
  'tablecolour',
  'product', #
  'buildid',
  'sel', 
],

'bug/activity/show.html.tmpl' => [
  'bug_id', 
],

'bug/activity/table.html.tmpl' => [
  'operation.who', # Email
  'change.attachid', 
  'change.field', 
],

'attachment/create.html.tmpl' => [
  'bugid', 
  'attachment.id', 
],

'attachment/created.html.tmpl' => [
  'attachid', 
  'bugid', 
  'contenttype', 
],

'attachment/edit.html.tmpl' => [
  'attachid', 
  'bugid', 
  'a', 
],

'attachment/list.html.tmpl' => [
  'attachment.attachid', 
  'FOR flag = attachment.flags', # Bug? No FOR directive
  'flag.type.name', 
  'flag.status',
  'flag.requestee.nick', # Email
  'show_attachment_flags ? 4 : 3',
  'bugid',
],

'attachment/show-multiple.html.tmpl' => [
  'a.attachid', 
],

'attachment/updated.html.tmpl' => [
  'attachid', 
  'bugid', 
],

'admin/products/groupcontrol/confirm-edit.html.tmpl' => [
  'group.count', 
],

'admin/products/groupcontrol/edit.html.tmpl' => [
  'filt_product', 
  'group.bugcount', 
  'group.id', 
  'const.CONTROLMAPNA', 
  'const.CONTROLMAPSHOWN', 
  'const.CONTROLMAPDEFAULT', 
  'const.CONTROLMAPMANDATORY', 
],

'admin/flag-type/confirm-delete.html.tmpl' => [
  'flag_count', 
  'name', #
  'flag_type.id', 
],

'admin/flag-type/edit.html.tmpl' => [
  'action', 
  'type.id', 
  'type.target_type', 
  'category', #
  'item', #
  'type.sortkey || 1',
  '(last_action == "enter" || last_action == "copy") ? "Create" : "Save Changes"',
],

'admin/flag-type/list.html.tmpl' => [
  'type.is_active ? "active" : "inactive"', 
  'type.id', 
  'type.flag_count', 
],

'account/login.html.tmpl' => [
  'target', 
],

'account/prefs/account.html.tmpl' => [
  'login_change_date', #
],

'account/prefs/email.html.tmpl' => [
  'watchedusers', # Email
  'useqacontact ? \'5\' : \'4\'', 
  'role', 
  'reason.name', 
  'reason.description',
],

'account/prefs/permissions.html.tmpl' => [
  'bit_description.name', 
  'bit_description.desc', 
],

'account/prefs/prefs.html.tmpl' => [
  'tab.name', 
  'tab.description', 
  'current_tab.name', 
  'current_tab.description', 
  'current_tab.description FILTER lower',
],

);

# Should filter reports/report.html.tmpl:130 $format
