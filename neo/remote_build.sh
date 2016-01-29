#!/bin/bash

if [ "${RPI_ADDRESS}" == "" ]; then
  echo "error: RPI_ADDRESS not defined"
  exit 1
fi

echo "synchronising..."
rsync -av --delete --exclude=build * ${RPI_ADDRESS}:/home/pi/doom3_build/

echo "building..."
ssh ${RPI_ADDRESS} "cd /home/pi/doom3_build; ./raspberry_pi.sh"

