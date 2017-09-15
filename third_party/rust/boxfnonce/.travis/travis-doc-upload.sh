#!/bin/sh

# License: CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/legalcode

set -e

echo "Building docs..."

cargo doc

if [ "${TRAVIS_RUST_VERSION}" != "stable" ]; then
	echo "Only uploading docs for stable, not ${TRAVIS_RUST_VERSION}"
	exit 0
fi

if [ "${TRAVIS_BRANCH}" != "master" ]; then
	echo "Only uploading docs for master branch, not ${TRAVIS_BRANCH}"
	exit 0
fi

if [ "${TRAVIS_PULL_REQUEST}" != "false" ]; then
	echo "Not uploading docs for pull requests"
	exit 0
fi

echo "Reading upload config..."

. .travis/travis-doc-upload.cfg

eval "key=\$encrypted_${SSH_KEY_TRAVIS_ID}_key"
eval "iv=\$encrypted_${SSH_KEY_TRAVIS_ID}_iv"

mkdir -p ~/.ssh
openssl aes-256-cbc -K "${key}" -iv "${iv}" -in .travis/id_rsa.enc -out ~/.ssh/id_rsa -d
chmod 600 ~/.ssh/id_rsa

git clone --branch gh-pages "git@github.com:${DOCS_REPO}" deploy_docs

cd deploy_docs
git config user.name "doc upload bot"
git config user.email "nobody@example.com"
rm -rf "${PROJECT_NAME}"
mv ../target/doc "${PROJECT_NAME}"
git add -A "${PROJECT_NAME}"
git commit -qm "doc upload for ${PROJECT_NAME} (${TRAVIS_REPO_SLUG})"
git push -q origin gh-pages
