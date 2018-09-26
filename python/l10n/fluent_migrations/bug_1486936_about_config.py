# coding=utf8

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import
import fluent.syntax.ast as FTL
from fluent.migrate.helpers import transforms_from, VARIABLE_REFERENCE
from fluent.migrate import REPLACE, COPY

def migrate(ctx):
	"""Bug 1486936 - Convert about:config to using Fluent for localization, part {index}."""

	ctx.add_transforms(
	"toolkit/toolkit/about/aboutConfig.ftl",
	"toolkit/toolkit/about/aboutConfig.ftl",
	transforms_from(
"""
config-window =
  .title = { COPY(from_path, "window.title") }
config-about-warning-title =
  .value = { COPY(from_path, "aboutWarningTitle.label") }
config-about-warning-text = { COPY(from_path, "aboutWarningText.label") }
config-about-warning-button = 
  .label = { COPY(from_path, "aboutWarningButton2.label") }
config-about-warning-checkbox =
  .label = { COPY(from_path, "aboutWarningCheckbox.label") }
config-search-prefs =
  .value = { COPY(from_path, "searchPrefs.label") }
  .accesskey = { COPY(from_path, "searchPrefs.accesskey") }
config-focus-search =
  .key = { COPY(from_path, "focusSearch.key") }
config-focus-search-2 =
  .key = { COPY(from_path, "focusSearch2.key") }
config-pref-column =
  .label = { COPY(from_path, "prefColumn.label") }
config-lock-column =
  .label = { COPY(from_path, "lockColumn.label") }
config-type-column =
  .label = { COPY(from_path, "typeColumn.label") }
config-value-column =
  .label = { COPY(from_path, "valueColumn.label") }
config-pref-column-header =
  .tooltip = { COPY(from_path, "prefColumnHeader.tooltip") }
config-column-chooser =
  .tooltip = { COPY(from_path, "columnChooser.tooltip") }
config-copy-pref =
  .key = { COPY(from_path, "copyPref.key") }
  .label = { COPY(from_path, "copyPref.label") }
  .accesskey = { COPY(from_path, "copyPref.accesskey") }
config-copy-name =
  .label = { COPY(from_path, "copyName.label") }
  .accesskey = { COPY(from_path, "copyName.accesskey") }
config-copy-value =
  .label = { COPY(from_path, "copyValue.label") }
  .accesskey = { COPY(from_path, "copyValue.accesskey") }
config-modify =
  .label = { COPY(from_path, "modify.label") }
  .accesskey = { COPY(from_path, "modify.accesskey") }
config-toggle =
  .label = { COPY(from_path, "toggle.label") }
  .accesskey = { COPY(from_path, "toggle.accesskey") }
config-reset =
  .label = { COPY(from_path, "reset.label") }
  .accesskey = { COPY(from_path, "reset.accesskey") }
config-new =
  .label = { COPY(from_path, "new.label") }
  .accesskey = { COPY(from_path, "new.accesskey") }
config-string =
  .label = { COPY(from_path, "string.label") }
  .accesskey = { COPY(from_path, "string.accesskey") }
config-integer =
  .label = { COPY(from_path, "integer.label") }
  .accesskey = { COPY(from_path, "integer.accesskey") }
config-boolean =
  .label = { COPY(from_path, "boolean.label") }
  .accesskey = { COPY(from_path, "boolean.accesskey") }
""", from_path="toolkit/chrome/global/config.dtd"))

	ctx.add_transforms(
	"toolkit/toolkit/about/aboutConfig.ftl",
	"toolkit/toolkit/about/aboutConfig.ftl",
		transforms_from(
"""
config-default = { COPY(from_path, "default") }
config-modified = { COPY(from_path, "modified") }
config-locked = { COPY(from_path, "locked") }
config-property-string = { COPY(from_path, "string") }
config-property-int = { COPY(from_path, "int") }
config-property-bool = { COPY(from_path, "bool") }
config-new-prompt = { COPY(from_path, "new_prompt") }
config-nan-title = { COPY(from_path, "nan_title") }
config-nan-text = { COPY(from_path, "nan_text") }
""", from_path="toolkit/chrome/global/config.properties"))

	ctx.add_transforms(
	"toolkit/toolkit/about/aboutConfig.ftl",
	"toolkit/toolkit/about/aboutConfig.ftl",
	[
		FTL.Message(
			id=FTL.Identifier("config-new-title"),
			value=REPLACE(
				"toolkit/chrome/global/config.properties",
				"new_title",
				{
					"%S": VARIABLE_REFERENCE(
						"type"
					),
				}
			)
		),
		FTL.Message(
			id=FTL.Identifier("config-modify-title"),
			value=REPLACE(
				"toolkit/chrome/global/config.properties",
				"modify_title",
				{
					"%S": VARIABLE_REFERENCE(
						"type"
					),
				}
			)
		),
	]
	)
