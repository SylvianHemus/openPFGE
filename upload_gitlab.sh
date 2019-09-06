#!/bin/bash

echo Commit text
read commit_text

git add .
git commit -m "$commit_text"
git push origin master
