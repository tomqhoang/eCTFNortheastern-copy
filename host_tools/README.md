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
that are written into the bootloader's memory space. In addition, the build tool also creates
`secret_build_output.txt`. This file creates the secret key, which is kept by the factory, and is
used later to encrypt the data to be sent to the bootloader. 

## Configure tool: bl_configure
"bl_configure serves to verify that the bootloader has been installed by the build tool properly. It
takes in the `secret_build_output.txt` file and checks to make sure that the factory and device have
the same secret key."

## Bundle and Protect: fw_protect
This script will encrypt the frames that represent the IP being protected. It makes use of the [Simon 
block cipher](https://github.com/inmcm/Simon_Speck_Ciphers/tree/master/Python) and [SHA256 hash algorithm](https://docs.python.org/2/library/hashlib.html).
