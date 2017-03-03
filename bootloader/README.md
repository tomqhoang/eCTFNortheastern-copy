#Bootloader Description and Setup
Our implementation of the bootloader is mostly an extension of the original MITRE design - any interaction with host tools is done through bootloader functions, which are usually accessed through physically setting jumpers across the specific headers that are associated with their respective pins. See the quick access table below to see which functions you can access.

Function Enabled | Pin jumped
------------ | -------------
Load firmware | PB2
Readback | PB3
Store password, if not done before | PB4
Boot firmware | None


##readback

##load_firmware

##boot_firmware

##store_password
