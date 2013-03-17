# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

dependencies = []
targets = []

def makeQuote(filename):
    return filename.replace(' ', '\\ ')  # enjoy!

def writeMakeDependOutput(filename):
    print "Creating makedepend file", filename
    with open(filename, 'w') as f:
        if len(targets) > 0:
            f.write("%s:" % makeQuote(targets[0]))
            for filename in dependencies:
                f.write(' \\\n\t\t%s' % makeQuote(filename))
            f.write('\n\n')
            for filename in targets[1:]:
                f.write('%s: %s\n' % (makeQuote(filename), makeQuote(targets[0])))
            for filename in dependencies:
                f.write('%s:\n' % filename)

