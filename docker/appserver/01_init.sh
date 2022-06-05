#!/bin/bash

datafolder="/app/data"

if [ -z "$(ls -A $datafolder)" ]; then
  SETUP=1 cmlystd --http-socket :3000 --ini /usr/local/src/cmlyst/config.ini
else
  cmlystd --http-socket :3000 --ini /usr/local/src/cmlyst/config.ini
fi

upt=$(awk -F. '{print $1}' /proc/uptime)
if [ $upt -lt 60 ]; then
  echo "Sleeping for debug"
  sleep infinity
fi

