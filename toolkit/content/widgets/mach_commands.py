# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re

from mach.decorators import Command, CommandArgument

FIXME_COMMENT = "// FIXME: replace with path to your reusable widget\n"
LICENSE_HEADER = """/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"""

JS_HEADER = """{license}
import {{ html }} from "../vendor/lit.all.mjs";
import {{ MozLitElement }} from "../lit-utils.mjs";

/**
 * Component description goes here.
 *
 * @tagname {element_name}
 * @property {{string}} variant - Property description goes here
 */
export default class {class_name} extends MozLitElement {{
  static properties = {{
    variant: {{ type: String }},
  }};

  constructor() {{
    super();
    this.variant = "default";
  }}

  render() {{
    return html`
      <link rel="stylesheet" href="chrome://global/content/elements/{element_name}.css" />
      <div>Variant type: ${{this.variant}}</div>
    `;
  }}
}}
customElements.define("{element_name}", {class_name});
"""

STORY_HEADER = """{license}
{html_lit_import}
// eslint-disable-next-line import/no-unassigned-import
{fixme_comment}import "{element_path}";

export default {{
  title: "{story_prefix}/{story_name}",
  component: "{element_name}",
  argTypes: {{
    variant: {{
      options: ["default", "other"],
      control: {{ type: "select" }},
    }},
  }},
}};

const Template = ({{ variant }}) => html`
  <{element_name} .variant=${{variant}}></{element_name}>
`;

export const Default = Template.bind({{}});
Default.args = {{
  variant: "default",
}};
"""


def run_mach(command_context, cmd, **kwargs):
    return command_context._mach_context.commands.dispatch(
        cmd, command_context._mach_context, **kwargs
    )


def run_npm(command_context, args):
    return run_mach(
        command_context, "npm", args=[*args, "--prefix=browser/components/storybook"]
    )


@Command(
    "addwidget",
    category="misc",
    description="Scaffold a front-end component.",
)
@CommandArgument(
    "names",
    nargs="+",
    help="Component names to create in kebab-case, eg. my-card.",
)
def addwidget(command_context, names):
    story_prefix = "UI Widgets"
    html_lit_import = 'import { html } from "../vendor/lit.all.mjs";'
    for name in names:
        component_dir = "toolkit/content/widgets/{0}".format(name)

        try:
            os.mkdir(component_dir)
        except FileExistsError:
            pass

        with open("{0}/{1}.mjs".format(component_dir, name), "w", newline="\n") as f:
            class_name = "".join(p.capitalize() for p in name.split("-"))
            f.write(
                JS_HEADER.format(
                    license=LICENSE_HEADER,
                    element_name=name,
                    class_name=class_name,
                )
            )

        with open("{0}/{1}.css".format(component_dir, name), "w", newline="\n") as f:
            f.write(LICENSE_HEADER)

        test_name = name.replace("-", "_")
        test_path = "toolkit/content/tests/widgets/test_{0}.html".format(test_name)
        jar_path = "toolkit/content/jar.mn"
        jar_lines = None
        with open(jar_path, "r") as f:
            jar_lines = f.readlines()
        elements_startswith = "   content/global/elements/"
        new_css_line = "{0}{1}.css    (widgets/{1}/{1}.css)\n".format(
            elements_startswith, name
        )
        new_js_line = "{0}{1}.mjs    (widgets/{1}/{1}.mjs)\n".format(
            elements_startswith, name
        )
        new_jar_lines = []
        found_elements_section = False
        added_widget = False
        for line in jar_lines:
            if line.startswith(elements_startswith):
                found_elements_section = True
            if found_elements_section and not added_widget and line > new_css_line:
                added_widget = True
                new_jar_lines.append(new_css_line)
                new_jar_lines.append(new_js_line)
            new_jar_lines.append(line)

        with open(jar_path, "w", newline="\n") as f:
            f.write("".join(new_jar_lines))

        story_path = "{0}/{1}.stories.mjs".format(component_dir, name)
        element_path = "./{0}.mjs".format(name)
        with open(story_path, "w", newline="\n") as f:
            story_name = " ".join(
                name for name in re.findall(r"[A-Z][a-z]+", class_name) if name != "Moz"
            )
            f.write(
                STORY_HEADER.format(
                    license=LICENSE_HEADER,
                    element_name=name,
                    story_name=story_name,
                    story_prefix=story_prefix,
                    fixme_comment="",
                    element_path=element_path,
                    html_lit_import=html_lit_import,
                )
            )

        run_mach(
            command_context, "addtest", argv=[test_path, "--suite", "mochitest-chrome"]
        )


@Command(
    "addstory",
    category="misc",
    description="Scaffold a front-end Storybook story.",
)
@CommandArgument(
    "name",
    help="Story to create in kebab-case, eg. my-card.",
)
@CommandArgument(
    "project_name",
    type=str,
    help='Name of the project or team for the new component to keep stories organized. Eg. "Credential Management"',
)
@CommandArgument(
    "--path",
    help="Path to the widget source, eg. /browser/components/my-module.mjs or chrome://browser/content/my-module.mjs",
)
def addstory(command_context, name, project_name, path):
    html_lit_import = 'import { html } from "lit.all.mjs";'
    story_path = "browser/components/storybook/stories/{0}.stories.mjs".format(name)
    project_name = project_name.split()
    project_name = " ".join(p.capitalize() for p in project_name)
    story_prefix = "Domain-specific UI Widgets/{0}".format(project_name)
    with open(story_path, "w", newline="\n") as f:
        print(f"Creating new story {name} in {story_path}")
        story_name = " ".join(p.capitalize() for p in name.split("-"))
        f.write(
            STORY_HEADER.format(
                license=LICENSE_HEADER,
                element_name=name,
                story_name=story_name,
                element_path=path,
                fixme_comment="" if path else FIXME_COMMENT,
                project_name=project_name,
                story_prefix=story_prefix,
                html_lit_import=html_lit_import,
            )
        )
