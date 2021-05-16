#!/usr/bin/env python3
import math
import os
from enum import Enum
from messages import Message, QueuedMessage

class Status(Enum):
    complete = 0
    pending = 1
    missing = 2


class Block:

    length = 16384

    def __init__(self, length, start_idx):
        
        ### Update these variables whenever we receive new data
        self.status = Status.missing
        ###
        self.data = None
        self.length = length
        # start_idx is the index of this block within PieceManager.data
        self.start_idx = start_idx


class Piece:

    def __init__(self, length, start_idx):

        ### Update these variables whenever we receive new data
        self.blocks = []
        self.status = Status.missing
        ###

        self.length = length
        # start_idx is the index of this piece within PieceManager.data
        self.start_idx = start_idx

        self.num_blocks = math.ceil(float(self.length) / float(Block.length))

        # Length of the last block if it is not equal to the normal block length.
        self.remainder_block_length = 0

        num_standard_blocks = math.floor(float(self.length) / float(Block.length))

        for i in range(num_standard_blocks):
            start_idx = self.start_idx + i * Block.length
            self.blocks.append(Block(Block.length, start_idx))
        
        if self.num_blocks > num_standard_blocks:
            start_idx = self.start_idx + num_standard_blocks * Block.length
            self.remainder_block_length = self.length % Block.length
            self.blocks.append(Block(self.remainder_block_length, start_idx))
    
    # Mark block as complete
    def process_incoming_block(self, block_index, data, start_idx):
        # Update block to be complete
        self.blocks[block_index].status = Status.complete
        self.blocks[block_index].data = data
        self.blocks[block_index].start_idx = start_idx
        # Update status of piece
        self.update_status()

        if self.status == Status.complete:
            return True   
        return False

    # Updates status of piece
    def update_status(self):
        self.status = Status.complete
        for block in self.blocks:
            if block.status != Status.complete:
                self.status = Status.pending
                break

    # Helper function for PieceManager.next_request
    def first_pending_and_missing_block(self):
        first_pending_block = None
        first_missing_block = None
        for block in self.blocks:
            if first_pending_block == None and block.status == Status.pending:
                first_pending_block = block
            elif first_missing_block == None and block.status == Status.missing:
                first_missing_block = block
            elif first_pending_block != None and first_missing_block != None:
                break
        return first_pending_block, first_missing_block


class PieceManager:

    def __init__(self, torrent):

        # Stores the final data.
        # Pieces will index into this array using:
        # self.data[piece.start_idx:piece.start_idx+piece.length]
        # Blocks will index into this array using:
        # self.data[block.start_idx:block.start_idx+block.length]
        ### Update these variables whenever we receive new data
        self.data = bytearray(torrent.total_length)
        self.pieces = []
        self.complete = False
        self.writing = FileWriter(torrent)
        
        ###

        # List of peers with outstanding request messages in the queue - will not send a new request to these peers
        self.outstanding_requests = []

        self.piece_length = torrent.piece_length
        self.total_length = torrent.total_length

        self.num_pieces = math.ceil(float(self.total_length) / float(self.piece_length))
        print("Num pieces: " + str(self.num_pieces))

        # Length of the last piece if it is not equal to the normal piece_length.
        self.remainder_piece_length = 0

        num_standard_pieces = math.floor(float(self.total_length) / float(self.piece_length))

        for i in range(num_standard_pieces):
            start_idx = i * self.piece_length
            self.pieces.append(Piece(self.piece_length, start_idx))

        if self.num_pieces > num_standard_pieces:
            start_idx = num_standard_pieces * self.piece_length
            self.remainder_piece_length = self.total_length % self.piece_length
            self.pieces.append(Piece(self.remainder_piece_length, start_idx))
    

    # Returns whether or not peer has something we are interested in.
    def check_if_interested(self, peer):
        for idx, piece in enumerate(self.pieces):
            if piece.status != Status.complete and peer.bitfield[idx] == 1:
                return True
        return False

    # index: integer specifying the zero-based piece index
    # begin: integer specifying the zero-based byte offset within the piece
    # block: incoming block of data. Should be an array of bytes.
    # TODO: If a piece goes from pending to complete, then:
    # Check hash of piece
    # Write piece to disk
    # Send a signal to all threads signifying that they should send a "HAVE" message.
    def process_incoming_piece(self, index, begin, block):
        start_idx = index * self.piece_length + begin
        # Update data
        self.data[start_idx:start_idx+len(block)] = block
        # Update completion status of pieces and blocks.
        block_idx = begin // Block.length
        piece_complete = self.pieces[index].process_incoming_block(block_idx, block, start_idx)

        # TODO: check hash of piece
        if piece_complete:
            self.writing.write_piece(self.pieces[index])

        self.update_status()
        return 0

    def update_status(self):
        self.complete = True
        for piece in self.pieces:
            if piece.status != Status.complete:
                self.complete = False
                break

    # Determine the next reqest for a peer.
    # Optional TODO: Implement rarest first strategy
    # Returns:
    # available: boolean specifying whether a piece we need is available by the peer.
    # index: integer specifying the zero-based piece index
    # begin: integer specifying the zero-based byte offset within the piece
    # length: integer specifying the requested length.
    def next_request(self, peer):
        # Priority should be:
        # Pending piece -> Missing block
        # Missing piece -> Missing block
        # Pending piece -> Pending block
        piece = None
        block = None

        # Pending piece -> Missing block
        block_and_piece_found = False
        if block_and_piece_found == False:
            for idx, piece in enumerate(self.pieces):
                if piece.status == Status.pending and peer.bitfield[idx] == 1:
                    _, block = piece.first_pending_and_missing_block()
                    if block != None:
                        block_and_piece_found = True
                        break

        # Missing piece -> Missing block
        if block_and_piece_found == False:
            for idx, piece in enumerate(self.pieces):
                if piece.status == Status.missing and peer.bitfield[idx] == 1:
                    _, block = piece.first_pending_and_missing_block()
                    if block != None:
                        block_and_piece_found = True
                        break

        # Pending piece -> Pending block
        if block_and_piece_found == False:
            for idx, piece in enumerate(self.pieces):
                if piece.status == Status.pending and peer.bitfield[idx] == 1:
                    block, _ = piece.first_pending_and_missing_block()
                    if block != None:
                        block_and_piece_found = True
                        break
        
        if block_and_piece_found == False:
            return False, 0, 0, 0

        # Update piece and block to pending status
        piece.status = Status.pending
        block.status = Status.pending
        # Create return variables
        available = True
        index = piece.start_idx // self.piece_length
        begin = block.start_idx - piece.start_idx
        length = block.length
        return available, index, begin, length

    # Generates the bitfield for the pieces we have.
    def generate_bitfield(self):
        bitfield = []
        for piece in self.pieces:
            if piece.status == Status.complete:
                bitfield.append(1)
            else:
                bitfield.append(0)
        return bitfield

    # To be used when a peer chokes the client, thus meaning a request cannot go through. Updates the block's status to
    # be requestable again and removes the peer from outstanding_requests.
    def cancel_request(self, queued_message):
        parsed_message = Message()
        parsed_message.parse_message(queued_message.message)
        piece = self.pieces[parsed_message.piece_index]
        block = piece.blocks[int(parsed_message.piece_offset/Block.length)]
        block.status = Status.missing
        piece.update_status()
        self.outstanding_requests.remove(queued_message.peer)

class FileWriter:

    def __init__(self, torrent):
        self.torrent = torrent
        self.bytes_written = 0
        self.curr_file_idx = 0
        self.curr_file = self.torrent.files[0]
        self.output_stream = open(self.curr_file.path, 'wb')

    def write_piece(self, piece):
        for block in piece.blocks:
            start_pos = block.start_idx

            # handles block that overlaps two files
            if self.bytes_written + block.length > self.curr_file.length:
                print("FILE LENGTH:", self.curr_file.length)
                print("BLOCKLEN:", len(block.data))
                bytes_to_write = self.curr_file.length - self.bytes_written
                first_block = block.data[:bytes_to_write]
                self.output_stream.seek(start_pos)
                self.output_stream.write(first_block)

                self.curr_file_idx += 1
                try:
                    self.curr_file = self.torrent.files[self.curr_file_idx]
                except:
                    print("Saving is complete", flush=True)
                self.output_stream = open(self.curr_file.path, 'wb')
                second_block = block.data[bytes_to_write:]
                self.output_stream.seek(0)
                self.output_stream.write(second_block)

                self.bytes_written = block.length - bytes_to_write
            else:
                self.output_stream.seek(start_pos)
                self.output_stream.write(block.data)
                self.bytes_written += block.length
        print("BYTES WRITTEN:", self.bytes_written)