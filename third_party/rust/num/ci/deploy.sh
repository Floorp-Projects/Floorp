#!/bin/sh

set -ex

cp doc/* target/doc/
pip install ghp-import --user
$HOME/.local/bin/ghp-import -n target/doc

openssl aes-256-cbc -K $encrypted_9e86330b283d_key -iv $encrypted_9e86330b283d_iv -in ./ci/deploy.enc -out ./ci/deploy -d
chmod 600 ./ci/deploy
ssh-add ./ci/deploy
git push -qf ssh://git@github.com/${TRAVIS_REPO_SLUG}.git gh-pages
