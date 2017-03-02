# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Create a separate commit for every locale.

parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$parent_path/../../l10n-repo"

git add locales/templates/app.pot
git commit -m "template update: app.pot"

cd locales

locale_list=$(find . -mindepth 1 -maxdepth 1 -type d  \( ! -iname ".*" \) | sed 's|^\./||g' | sort)
for locale in ${locale_list};
do
    # Exclude templates
    if [ "${locale}" != "templates" ]
    then
        git add ${locale}/app.po
        git commit -m "${locale}: Update app.po"
    fi
done

