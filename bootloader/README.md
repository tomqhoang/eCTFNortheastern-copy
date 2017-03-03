#Bootloader Description and Setup
Our implementation of the bootloader is mostly an extension of the original MITRE design - any interaction with host tools is done through bootloader functions, which are usually accessed through physically setting jumpers across the specific headers that are associated with their respective pins. Special consideration has been given to restricting access to the functions through flagging and redundant checks. See the quick access table below to see which functions you can access. 

Function Enabled | Pin jumped
------------ | -------------
Load firmware | PB2
Readback | PB3
Store password, if not done before | PB4
Boot firmware | None


##readback
Functionally the same as MITRE example code, but with secure implementation in mind. In this case, the readback tools receives frames with the password (a set length of 32 bytes), the start address and size of data read (4 bytes raw bytes each, but are effectively 8 byte chunks due to SIMON implementing 64-bit blocks), and the hash of the previous data (32 bytes). Each of these fields are encrypted by SIMON separately and then the hash of that encrypted block is appended. See the readback host tool for more information. This information is then appropriately authenticated by the bootloader (decryption necessary to check password). 

The given start address and size determines the location of the firmware being processed (implemented in the same way as in the original MITRE code) except that the data is hashed and encrypted, in that order. This follows a similar structure to the load_firmware function, except one function is encrypting pages and the other is encrypting.

##load_firmware
This function is the most changed from the MITRE code, mainly because the collaboration of the SIMON python and C libraries require significant porting in both the host tool and in the bootloader function. To be specific, this is mainly due to the unusual nature of how the python SIMON library handles data representation conversion between both its encrypt/decrypt function. Of course, encrypt/decrypt is consistent with the usage of the python library alone. However, when encryption and decryption are performed on different platforms, this internal consistency of python Simon data representations begins to break down and now requires a step-by-step consideration of how data types are manipulated. 

Frame handling also provides a significant challenge as well - there are many edge cases to consider. This challenge is also not completely agnostic of the data representation inconsistency - each frame (including edge cases) require some specific handling. The way that this python variant of SIMON handles this sort of data representation is obtuse at best - to fully understand the issue requires significant understanding of the cryptographic algorithm itself (something that we felt was out of the scope of the competition). Instead, the best way to understand our implementation is to step through the code of fw_update itself. 

It is difficult to structure porting code for such an obstuse system, but we decided that the best way was to simplify the flow of the code such that it can be read in a linear, top to bottom way. Code that are functionally the same are blocked in such a manner. Please read the comments to understand some of the more complex code, especially for code that require some thought into the type of data representation conversions that are being done. Again, we stress stepping through the code to ensure a working knowledge of the terminal-bootloader relationship. We suggest using [pdb](https://docs.python.org/2/library/pdb.html) and programming our build on a free ATMEGA chip and running gdb (you should remember to set the fuses to more debugging-friendly values). 

##boot_firmware
This function is fundamentally the same as the MITRE edition. Should boot up to the first address in the provisioned firmware.

##store_password
This stores password and key (sent from bl_configure factory side) into flash. This is only done once per bootloader flash.
