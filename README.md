XenNet - The XenServer Windows Network Device Driver
==========================================

XenNet.sys is an NIDS 6 network device driver.  It replaces the emulated
network device on a guest VM with a paravirtual network device which is
able to offer faster, lower latency networking on a guest VM.

There is an instance of the xennet device for each PV network device 
that has been made available to the guest VM

Quick Start
===========

Prerequisites to build
----------------------

*   Visual Studio 2012 or later 
*   Windows Driver Kit 8 or later
*   Python 3 or later 

Environment variables used in building driver
-----------------------------

MAJOR\_VERSION Major version number

MINOR\_VERSION Minor version number

MICRO\_VERSION Micro version number

BUILD\_NUMBER Build number

SYMBOL\_SERVER location of a writable symbol server directory

KIT location of the Windows driver kit

PROCESSOR\_ARCHITECTURE x86 or x64

VS location of visual studio

Commands to build
-----------------

    git clone http://github.com/xenserver/win-xennet
    cd win-xennet
    .\build.py [checked | free]

Device tree diagram
-------------------

    XenNet XenNet
        |    | 
        XenVif
           |
        XenBus
           |
        PCI Bus      
