yamma
=====

Yet another mma (naive) interpreter

General
-------

Yet another mma (naive) interpreter. 

Limitations
-----------

Pattern matcher under construction.
And many others.

Current Capability
--------------------

 * funs.m    implement some functions that are missing in kernel
 * j.m       Arthur Whitney's one-page interpreter ported to yamma (see http://www.jsoftware.com/jwiki/Essays/Incunabulum)
 * showpi.m  a naive try on printing PI digits
 * euler.m   solutions to some problems in Project Euler 
 * morse.m   morse code

Build/Run
---------

requirements

    * GCC
    * Gnu make

debug:      make
release:    make CFG=Release

place yamma.exe together with KernelMain.m, CP936.m, UnicodeCharacters.tr. Run yamma.exe.

Bonus
-----

thanks for reading this readme.

try yamma.exe -lang lisp

NEVER try "`*`". it will destory your computer.

History
--------

v0.1: make it open source under GPL, and say hello to GitHub. 



