#!/usr/bin/env python
"""
Firmware Bundle-and-Protect Tool
"""
import argparse
import shutil
import struct
import json
import zlib
import pdb
from hashlib import sha256
from cStringIO import StringIO
from intelhex import IntelHex
import random, os, struct
#from Crypto.Cipher import AES
from simon import SimonCipher

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Firmware Update Tool')

    parser.add_argument("--infile",
                        help="Path to the firmware image to protect.",
                        required=True)
    parser.add_argument("--outfile", help="Filename for the output firmware.",
                        required=True)
    parser.add_argument("--version", help="Version number of this firmware.",
                        required=True)
    parser.add_argument("--message", help="Release message for this firmware.",
                        required=True)
    args = parser.parse_args()

    # Parse Intel hex file.
    firmware = IntelHex(args.infile)


    # Get version and size.
    firmware_size = firmware.maxaddr() + 1
    version = int(args.version)

    # Add release message to end of hex (null-terminated).
    sio = StringIO()
    firmware.putsz(firmware_size, (args.message + '\0'))
    firmware.write_hex_file(sio)
    hex_data = sio.getvalue()

    # Hex data is in IntelHex format, see wikipedia article on formatting


    #create simon cipher, key goes in here in 0xhex, or int('hex-string-here',16)
    my_simon = SimonCipher(0xABBAABBAABBAABBAABBAABBAABBAABBA)

    # split each line
    byline = hex_data.splitlines()

    # remove first two lines from encryption because there is no data there
    encrypted_byline = byline[0:2]

    # only use data lines
    byline_data = byline[2:-1]

    # for hash output
    hash_input_temp = ''
    hash_input = []
    i = 0
    # for each line with data
    for line in byline_data:
        # read numBytes in line
        numBytes = int(line[1:3],16);
        # get data hex values
        data = line[9:9+numBytes*2]
        #pdb.set_trace()
        # encrypt the 16B using simon
        encryptedData = my_simon.encrypt(int(data,16))
        # create new line with encrypted data, convert to hex string
        encryptedline = line[0:9] + hex(encryptedData)[2:-1] + line[-2:]
        if(i == 16):
            i = 0;
            hash_input.append((hash_input_temp))
        else:
            i = i+1
            hash_input_temp = hash_input_temp + hex(encryptedData)[2:-1]

        encrypted_byline.append(encryptedline)


    # add the last line back
    encrypted_byline.append(byline[-1])
    # go back to intel_hex SIO() format for hex_data
    encrypted_hex_data = "\n".join(encrypted_byline)

    #HASH for pages
    tags = []
    for input_data in hash_input:
        hash_local = sha256(input_data).hexdigest()
        first_half = hex(my_simon.encrypt(int(hash_local[0:32],16)))[2:-1]
        second_half = hex(my_simon.encrypt(int(hash_local[32:],16)))[2:-1]
        tags.append(first_half+second_half)

    # Sign Result
    # Save as Version-Bytes
    pdb.set_trace()

    # 16-bit 2B integer representation
    version_bytes = struct.pack('<H',version)
    # HASH
    version_hash = sha256(version_bytes).hexdigest()
    # SIMON in 16B chunks
    version_sig = hex(my_simon.encrypt(int(version_hash[0:32],16)))[2:-1]
    # Combine
    version_sig = version_sig + hex(my_simon.encrypt(int(version_hash[32:],16)))[2:-1]
   
    # Encode the data as json and write to outfile.
    data = {
        'firmware_size' : firmware_size,
        'version_bytes'  : version_bytes,
        'version_sig' : version_sig,
        'hex_data' : encrypted_hex_data,
        'tags' : tags
    }

    with open(args.outfile, 'wb+') as outfile:
        data = json.dumps(data)
        data = zlib.compress(data)
        outfile.write(data)