#!/usr/bin/env bash

if [ $# == 0 ];then
    echo "Usage: ./gitPush.sh message"
    exit 1
fi

git add -A
git commit -m "$*"
git push
