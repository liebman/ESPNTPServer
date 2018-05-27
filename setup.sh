#!/bin/sh

git submodule init
git submodule update
cd hardware/esp8266com/esp8266
git submodule init
git submodule update
cd tools
./get.py

