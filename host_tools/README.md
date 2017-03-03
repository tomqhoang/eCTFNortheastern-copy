# NOT COMPLETE

# NOT COMPLETE

HELLO IT IS NOT COMPLETE





# Welcome to the host tools!
Here are the host tools that are needed to meet the functional requirements 
specified in the rules. We decided to follow the formatting set up in MITRE's
[insecure example](https://github.com/mitre-cyber-academy/2017-ectf-insecure-example).

The host tools do mostly what you would expect them to do, in that they follow the same
flow chart model provided to us in the [rule book (p. 10)](http://mitrecyberacademy.org/competitions/embedded/ectf_challenge17_v2.0.pdf).

Here are the tools we used, and a brief discription of their role in the security of 
the device.

## Build tool: bl_build
The main purpose of the build tool is to create the bootloader. It does so by creating hex files
that are written into the bootloader's memory space. This tool uses essentially all of the MITRE content, except for minor changes to .hex file handling which should not affect operation.  Also, the tool sets lock/fuse bits to more secure values. See the [ATMEL datasheet](http://www.atmel.com/Images/Atmel-42719-ATmega1284P_Datasheet.pdf) for a good table that describes the usage of each bit. at In addition, the build tool also creates `secret_build_output.txt` (which is in a JSON format). This file also stores the secret password (32 bytes) created by the tool which is used for readback permission.
Optional:
--clean (runs Make clean in bootloader)

## Configure tool: bl_configure
bl_configure generates the secret symmetric key (128-bits) used for SIMON encryption/decryption. It then provisions the bootloader board with this key and the password (which is done by consuming the "secret_build_output.txt), which it also integrity checks with hashing. Finally, the tool stores both of these secret values into a new text file called "secret_configure_output.txt" (also a JSON file). 
Required:
*--port (UART1 used for configuring the board)

## Bundle and Protect: fw_protect
This script will encrypt the fimrware that represent the IP being protected. It makes use of the [Simon 
block cipher, 64-bit block/128-bit word](https://github.com/inmcm/Simon_Speck_Ciphers/tree/master/Python) and [SHA256 hash algorithm](https://docs.python.org/2/library/hashlib.html). Our SIMON cipher requires workarounds to work properly with our microprocessor. There are also significant manual handling of firmware frame creation. Please see the code for detailed analysis of these procedures.

This function is the most changed from the MITRE code, mainly because the collaboration of the SIMON python and C libraries require significant porting in both the host tool and in the bootloader function. To be specific, this is mainly due to the unusual nature of how the python SIMON library handles data representation conversion between both its encrypt/decrypt function. Of course, encrypt/decrypt is consistent with the usage of the python library alone. However, when encryption and decryption are performed on different platforms, this internal consistency of python Simon data representations begins to break down and now requires a step-by-step consideration of how data types are manipulated. 

Frame handling also provides a significant challenge as well - there are many edge cases to consider. This challenge is also not completely agnostic of the data representation inconsistency - each frame (including edge cases) require some specific handling. The way that this python variant of SIMON handles this sort of data representation is obtuse at best - to fully understand the issue requires significant understanding of the cryptographic algorithm itself (something that we felt was out of the scope of the competition). Instead, the best way to understand our implementation is to step through the code of fw_update itself. 

It is difficult to structure porting code for such an obstuse system, but we decided that the best way was to simplify the flow of the code such that it can be read in a linear, top to bottom way. Code that are functionally the same are blocked in such a manner. Please read the comments to understand some of the more complex code, especially for code that require some thought into the type of data representation conversions that are being done. Again, we stress stepping through the code to ensure a working knowledge of the terminal-bootloader relationship. We suggest using [pdb](https://docs.python.org/2/library/pdb.html) and programming our build on a free ATMEGA chip and running gdb (you should remember to set the fuses to more debugging-friendly values). 

The fw_protect tool takes in the following parameters:
Required:
* --infile (hex file to be protected)
* --outfile (protected output file)
* --version (0 <= x <= 2^(16)-1)
* --message (Release message)

## Update Tool: fw_update
This publicly available tool has no security measures - everything related to cryptographic measures is handled in host tools executed before this tool and in the bootloader itself. This host tool essentially has no changes from the original MITRE code.
Required:
* --port (UART1, sends/receives data over)
* --firmware (secured firmware .hex file)
* --debug (prints debug messages)

## Readback Tool: readback
Tool used to extract sections of flash from the bootloader, provided that the readback tool delivers a correct password. A correct password will cause the bootloader to send the firmware in frames over UART1 in an encrypted, hashed form. The readback tool will be provisioned with the key/password from the secret_configure_output.txt in order to gain readback permission and be able to decrypt the firmware. This also implements the porting of the Simon python library mentioned in fw_protect.
Required:
* --port (UART1 for sending passwords/addresses/hash, receiving encrypted/firmare
* --address (start address of flash being read)
* --num-bytes (size of data being read, starting from applied address)
Optional:
* --datafile (file to write data to)
