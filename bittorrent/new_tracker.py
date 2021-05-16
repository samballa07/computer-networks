#!/usr/bin/env python3

import sys
import os
import hashlib
import secrets
import bencodepy
import math
from torrent import Torrent
import urllib.request
import urllib.parse

# Importing the bencoding library found at https://pypi.org/project/bencode.py/ *NOTE: FOR PYTHON3*
# This executeable will read the contents of a .torrent file and parse it correctly, creating a 
# message to send to the tracker, sending that message, the printing out the relevant results of 
# that message. i.e. seeders or other clients that may have pieces

# Here we assume (and require) that argv[1] hold the filename.
# We also assume that argv[2] holds the compact boolean value (We'll use yes/no)
class Peer:
    def __init__(self, ip, port, num_pieces):
        
        self.sock = None
        self.ip = ip
        self.port = port
        self.idx = 0

        self.am_choking = True
        self.am_interested = False
        self.peer_choking = True
        self.peer_interested = False
        
        self.bitfield = [0] * num_pieces

        self.send_bitfield_to_peer = True

        self.pending_request = False
    
    def update_status(self, message):
        # For all messages that return -1, consider handling it outside of the Peer class.
        if message.r_type == "keep-alive":
            return -1
        elif message.r_type == "choke":
            self.peer_choking = True
            return 0
        elif message.r_type == "unchoke":
            self.peer_choking = False
            return 0
        elif message.r_type == "interested":
            self.peer_interested = True
            return 0
        elif message.r_type == "not interested":
            self.peer_interested = False
            return 0
        elif message.r_type == "have":
            self.bitfield[message.piece_index] = 1
            return 0
        elif message.r_type == "bitfield":
            self.bitfield = message.bitfield
            return 0
        elif message.r_type == "request":
            return -1
        elif message.r_type == "piece":
            return -1
        elif message.r_type == "cancel":
            return -1
        else:
            return -1


    
# will parse torrent file and make request upon calling constructing
# public methods include printing the peers and return the peers in an array
class Tracker:

    def __init__(self, compact, torrent):

        if compact.lower() == 'yes':
            self.compact = 1
        else:
            self.compact = 0

        # Passing in .torrent file data to tracker.
        self.trackerURL = torrent.trackerURL
        self.infohash = torrent.infohash
        self.length = torrent.total_length

        self.peerID = secrets.token_bytes(20)
        self.num_pieces = math.ceil(float(torrent.total_length) / float(torrent.piece_length))
        # Do not use this array beyond self.__make_request()!
        self.peers = []

        self.__make_request()

        #print(trackerURL, infohash, peerID, length, sep="\n")


    # This section sets up the key&values that go into the GET request along with the /announce?
    def __make_request(self):
        data = {}
        data['info_hash'] = self.infohash
        data['peer_id'] = self.peerID
        data['port'] = 6888 #NOTE we MUST be listening on this port for responses choose 6881-6889
        data['uploaded'] = 0
        data['downloaded'] = 0
        data['left'] = self.length
        data['compact'] = self.compact
        if not self.compact:
            data['no_peer_id'] = 1
        data['event'] = 'started'
        url_values = urllib.parse.urlencode(data)
        url = self.trackerURL + '?' + url_values
        #print(url)

        # Sends 'url' and the response goes into 'response'
        # We parse it and read the values to see what peers we may connect to
        # Along with how many piece we have completed/incomplete and the interval at which to communicate with the tracker again
        ip = ""
        port = 0
        with urllib.request.urlopen(url) as response:
            parsed_response = bencodepy.decode(response.read())

            respKeys = list(parsed_response.keys())
            respValues = list(parsed_response.values())
            for i in range(0, len(respKeys)):
                if "peers" in respKeys[i].decode('utf-8'):
                    temp = respValues[i]
                    counter = 0
                    for x in range(0, len(temp)):
                        if counter < 4:
                            ip += str(temp[x])
                            ip += "."
                            counter += 1
                        elif counter == 4:
                            port += int.from_bytes(temp[x:x+2], "big")
                            counter += 2
                        else:
                            ip = ip[0:len(ip)-1]
                            self.peers.append(Peer(ip,port,self.num_pieces))
                            ip = ""
                            port = 0
                            counter = 0
                else:
                    # print(respKeys[i].decode('utf-8'), respValues[i],sep="=")
                    pass

    def get_peers(self):
        return self.peers

    def print_peers(self): #public method
        for y in self.peers:
            print(y.ip, y.port, sep="|")



if __name__ == "__main__":
    tracker = Tracker(sys.argv[2], sys.argv[1])
    tracker.print_peers()
