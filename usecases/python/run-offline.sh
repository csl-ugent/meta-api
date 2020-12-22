#!/usr/bin/env bash
set -x
find $HOME/python -name *.pyc -exec rm -f {} \; 
 mkdir -p prefix 
 rm -r prefix/* 
 /home/ubuntu/python/bin/python3.7 ./get-pip-offline.py --prefix=${PWD}/prefix --no-cache-dir > pip.stdout 2> pip.stderr 
 /home/ubuntu/python/bin/python3.7 -m pip install --prefix=${PWD}/prefix --no-cache-dir speedtest-cli --no-index --find-links=file:///home/ubuntu/pip-repository > speedtest-install.stdout 2> speedtest-install.stderr 
 /home/ubuntu/python/bin/python3.7 -m pip install --prefix=${PWD}/prefix --no-cache-dir homer-text --no-index --find-links=file:///home/ubuntu/pip-repository > homer-install.stdout 2> homer-install.stderr 
 /home/ubuntu/python/bin/python3.7 -m pip install --prefix=${PWD}/prefix --no-cache-dir steganographer --no-index --find-links=file:///home/ubuntu/pip-repository > steganographer-install.stdout 2> steganographer-install.stderr 
# /home/ubuntu/python/bin/python3.7 -m speedtest > speedtest.stdout 2> speedtest.stderr 
 /home/ubuntu/python/bin/python3.7 ./homer-prepare.py > homer-prepare.stdout 2> homer-prepare.stderr 
 /home/ubuntu/python/bin/python3.7 -m homer.homer_cmd --name moby_dick --author hermal_melville --file_path ./mobydick.txt > homer.stdout 2> homer.stderr 
 /home/ubuntu/python/bin/python3.7 -m steganographer -o ./trollface-stego.jpg -m it_works ./trollface.jpg > steganographer.stdout 2> steganographer.stderr 
