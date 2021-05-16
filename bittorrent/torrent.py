#!/usr/bin/env python3

import bencodepy
import hashlib

class File:

    def __init__(self, length, path):
        self.length = length
        self.path = path


class Torrent:

    def __init__(self, torrent_file):
        self.trackerURL = None
        self.infohash = None
        self.decoded = None

        # Info dictionary
        self.piece_length = 0
        self.pieces_hash = []

        # Boolean that tells us whether there's a single or multiple files.
        self.single_file = True

        self.name = None
        self.total_length = 0
        
        # TODO: Multiple files
        self.files = []


        with open(torrent_file, 'rb') as f:
            self.decoded = bencodepy.decode(f.read())
        keys = list(self.decoded.keys())
        values = list(self.decoded.values())
        for i in range(0,len(keys)):

            #print(keys[i].decode('utf-8'), values[i], sep="=")

            if keys[i].decode('utf-8') == "announce":

                self.trackerURL = values[i].decode('utf-8')

            elif keys[i].decode('utf-8') == "info":

                newDict = values[i]
                infoKeys = list(newDict.keys())
                infoValues = list(newDict.values())
                #print(newDict, infoKeys, infoValues, sep="\n")
                self.infohash = hashlib.sha1( bencodepy.encode(values[i]) ).digest()
                #bytes.fromhex(infohash)
                for j in range(0,len(infoKeys)):
                    #Means that this is a multi-file torrent
                    if infoKeys[j].decode('utf-8') == "files":
                        self.single_file = False
                        files = (infoValues[j])
                        temp_len = 0
                        temp_path = ""
                        temp_path_str = ""
                        for x in files:
                            filesKeys = list(x.keys())
                            filesValues = list(x.values())
                            # TODO: Parse length and path for each file
                            for k in range(0,len(filesKeys)):

                                if "length" in filesKeys[k].decode('utf-8'):
                                    self.total_length += filesValues[k]
                                    temp_len = filesValues[k]
                                if "path" in filesKeys[k].decode('utf-8'):
                                    temp_path  = filesValues[k]
                                    temp_path_str = ""
                                    for l in range(len(temp_path)):
                                        path_part = temp_path[l].decode('utf-8')
                                        if l == len(temp_path) - 1:
                                            temp_path_str += path_part
                                        else:
                                            temp_path_str += path_part + "/"
                            #Create File objects and store them so we know what files to create/store data in
                            if not self.files:
                                self.files.append(File(temp_len, temp_path_str))
                            else: 
                                next_file = File(temp_len, temp_path_str)
                                self.files.append(next_file)

                       
                    #Means that this is a single-file torrent
                    #Last file in self.files is the directory for the files if it's multi torrent i believe
                    elif infoKeys[j].decode('utf-8') == "name":
                        if not self.files:
                            self.files = [File(self.total_length, infoValues[j].decode('utf-8'))]
                        else: 
                            next_file = File(self.total_length, infoValues[j].decode('utf-8'))
                            self.files.append(next_file)
                    elif infoKeys[j].decode('utf-8') == "length":
                        self.total_length += infoValues[j]
                    
                    elif infoKeys[j].decode('utf-8') == "piece length":
                        self.piece_length = infoValues[j]
                        #print("Piece length: " + str(self.piece_length))

                    elif infoKeys[j].decode('utf-8') == "pieces":
                        counter = 0
                        while counter < len(infoValues[j]):
                            self.pieces_hash.append(infoValues[j][counter:counter+20])
                            counter += 20
                        


            