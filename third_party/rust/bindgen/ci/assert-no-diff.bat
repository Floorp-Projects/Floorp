@echo off

cd "%~dp0.."

git add -u
git diff @
git diff-index --quiet HEAD
