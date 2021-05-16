#!/usr/bin/env python3

import sys
import os
import subprocess
import time
import math
# This script utilizes transmissionbt, make sure you have it installed.
# Reference to transmissionbt can be found at https://help.ubuntu.com/community/TransmissionHowTo
# I am using this on Ubuntu running on WSL


#Starts up the transmission server used for torrenting
print("Starting transmission-daemon")
if subprocess.run(["transmission-daemon"]).returncode != 0:
    print("Error starting transmission-daemon")
    sys.exit()
time.sleep(2)

#Adds the torrent file to the server NOTE the file it hardcoded, which is fine for now and maybe for the purposes for this project
print("Adding torrent to transmissonbt")
if subprocess.run(["transmission-remote", "-a", "Torrents/ubuntu-20.04.2.0-desktop-amd64.iso.torrent"], stdout=subprocess.DEVNULL).returncode != 0:
    print("Error adding torrent to transmission-remote")
    sys.exit()
print("Added file successfully!")

#Starts the timer for downloading
start = time.time()
test = subprocess.run(["transmission-remote", "-l"], stdout=subprocess.PIPE, text=True)
output = list(filter(('').__ne__, test.stdout.split(" ")))
last = ''

#Periodically runs the command that shows the progress of the torrent
#Only prints out updates if the % has changed, looping until 100%
while output[10] != '100%':
    if output[10] != last:
        print(output[10])
    last = output[10]
    time.sleep(1)
    test = subprocess.run(["transmission-remote", "-l"], stdout=subprocess.PIPE, text=True)
    output = list(filter(('').__ne__, test.stdout.split(" ")))
#print(list(filter(('').__ne__, output)))
#Download complete, get the time again then I do some time manipulations
# to make the output appear 'prettier'
end = time.time()
timer = end-start
minutes = math.floor(timer/60)
seconds = math.floor(timer % 60)
#Print the results, necessary
print(f"Transmissionbt took time:\n{timer} total seconds OR\n{minutes} minutes {seconds} seconds to download the torrent")

#Prevent seeding and letting the server stay on so we remove the torrent (and its data) then shut it down
print("Shutting down transmissionbt")
if subprocess.run(["transmission-remote", "-t", "all", "-rad"], stdout=subprocess.DEVNULL).returncode != 0:
    print("Error shutting down transmissionbt")
if subprocess.run(["transmission-remote", "--exit"], stdout=subprocess.DEVNULL).returncode != 0:
    print("Error shutting down transmissionbt")

print("Success")

#TODO Extend this sort of implementation with our own working client to prove comparable bt client download times