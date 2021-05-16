#!/usr/bin/env python3

import sys
import os
import struct
import math

#Messages take the form <length prefix><Message id><paylod> where not every message has a payload
class Message:

    def __init__(self):
        self.r_type = ""
        self.bitfield = []
        self.piece_index = 0
        self.piece_offset = 0
        self.request_length = 0
        self.block = None
        #c is a counter for moving through the message
        self.c = 0

    def read_int(self, msg, length):
        answer = int.from_bytes(msg[self.c:self.c+length], 'big')
        self.c += length
        return answer

    def parse_message(self, msg):
        self.c = 0
        length = self.read_int(msg, 4)
        if length == 0:
            self.r_type = "keep-alive"
        else:
            r_id = self.read_int(msg, 1)
            if r_id == 0:
                self.r_type = "choke"
            elif r_id == 1:
                self.r_type = "unchoke"
            elif r_id == 2:
                self.r_type = "interested"
            elif r_id == 3:
                self.r_type = "not interested"
            elif r_id == 4:
                self.r_type = "have"
                self.piece_index = self.read_int(msg, length - 1)
            elif r_id == 5:
                # TODO: Handle error of wrong length bitfield
                self.r_type = "bitfield"
                for _ in range(length-1):
                    # Break up byte read into bits for bitfield
                    b = self.read_int(msg, 1)
                    for i in range(8):
                        if b & (1 << (7-i)):
                            self.bitfield.append(1)
                        else:
                            self.bitfield.append(0)
            elif r_id == 6:
                self.r_type = "request"
                (self.piece_index, self.piece_offset, self.request_length) = struct.unpack_from("!III", msg, self.c)
            elif r_id == 7:
                self.r_type = "piece"
                (self.piece_index, self.piece_offset) = struct.unpack_from("!II", msg, self.c)
                self.c += 8
                self.block = msg[self.c:self.c+length-9]
            elif r_id == 8:
                self.r_type = "cancel"
                (self.piece_index, self.piece_offset, self.request_length) = struct.unpack_from("!III", msg, self.c)
        #print(self.r_type, self.bitfield, self.piece_index, self.piece_offset, self.request_length, self.block, sep=" | ")

    #Has format: <len=0000>, no <message id> or <payload>
    def m_keep_alive(self):
        prefix = (0).to_bytes(4, 'big')
        message = prefix
        return message

    #Has format <len=0001><id=0>
    def m_choke(self):
        prefix = (1).to_bytes(4, 'big')
        m_id = (0).to_bytes(1, 'big')
        message = prefix + m_id
        return message

    #Has format <len=0001><id=1>
    def m_unchoke(self):
        prefix = (1).to_bytes(4, 'big')
        m_id = (1).to_bytes(1, 'big')
        message = prefix + m_id
        return message

    #Has format <len=0001><id=2>
    def m_interested(self):
        prefix = (1).to_bytes(4, 'big')
        m_id = (2).to_bytes(1, 'big')
        message = prefix + m_id
        return message

    #Has format <len=0001><id=3>
    def m_uninterested(self):
        prefix = (1).to_bytes(4, 'big')
        m_id = (3).to_bytes(1, 'big')
        message = prefix + m_id
        return message

    # Has format <len=0005><id=4><piece index>
    # @Param index
    # Should be a integer that denotes the piece we just received (and verified)
    def m_have(self,index):
        prefix = (5).to_bytes(4, 'big')
        m_id = (4).to_bytes(1, 'big')
        message = prefix + m_id + (index).to_bytes(4, 'big')
        return message

    # Has format <len=0001+X><id=5><bitfield>
    # @Param bitfield
    # Should be an array of 1's and 0's where 1's shows that we have that piece at that index of the file
    # i.e [0, 1, 0, 0, 0, 1] Shows we have piece [1] and piece [5] so other peers will know after we send them the
    # bitfield.
    def m_bitfield(self, bitfield):
        prefix = (1+math.ceil(len(bitfield)/8)).to_bytes(4, 'big')
        m_id = (5).to_bytes(1, 'big')
        message = prefix + m_id
        i = 0
        while i < len(bitfield):
            # Convert bits in bitfield array to bytes
            b = ''
            remaining = len(bitfield) - i
            if remaining < 8:
                for j in range(remaining):
                    b += str(bitfield[i+j])
                b += '0'*(8-remaining)
            else:
                for j in range(8):
                    b += str(bitfield[i+j])
            
            message += (int(b, 2)).to_bytes(1, 'big')
            i += 8
        return message

    # Has format <len=00013><id=6><index><start><length>
    # @Param index
    # Should be an integer specifying the zero-based piece index
    # @Param start
    # Should be an integer specifying the zero-based byte offset within the piece
    # @Param length
    # Should be an integer specifying the requested length.
    def m_request(self, index, start, length):
        prefix = (13).to_bytes(4, 'big')
        m_id = (6).to_bytes(1, 'big')
        message = prefix + m_id + (index).to_bytes(4, 'big') + (start).to_bytes(4, 'big') + (length).to_bytes(4, 'big')
        return message

    # Has format <len=0009+X><id=7><index><start><length>
    # @Param index
    # Should be an integer specifying the zero-based piece index
    # @Param start
    # Should be an integer specifying the zero-based byte offset within the piece
    # @Param block
    # Should be a block of data, which is a subset of the piece specified by index.
    def m_piece(self, index, start, block):
        #TODO Not sure what block will be? Maybe bytearray or bytestring, will need to update once determined
        #For now: implemented block as bytes object
        prefix = (9+len(block)).to_bytes(4, 'big')
        m_id = (7).to_bytes(1, 'big')
        message = prefix + m_id + (index).to_bytes(4, 'big') + (start).to_bytes(4, 'big') + block
        return message

    # Has format <len=00013><id=8><index><start><length>
    # @Param index
    # Should be an integer specifying the zero-based piece index
    # @Param start
    # Should be an integer specifying the zero-based byte offset within the piece
    # @Param length
    # Should be an integer specifying the requested length.
    def m_cancel(self, index, start, length):
        prefix = (13).to_bytes(4, 'big')
        m_id = (8).to_bytes(1, 'big')
        message = prefix + m_id + (index).to_bytes(4, 'big') + (start).to_bytes(4, 'big') + (length).to_bytes(4, 'big')
        return message

class QueuedMessage:
    def __init__(self, message, msg_type, peer):
        self.message = message
        self.msg_type = msg_type
        self.peer = peer

# generator = Message()
# reader = Message()
# reader.parse_message(generator.m_piece(4, 3, b'asdlkfjhaskldjfhlaskdjhf'))