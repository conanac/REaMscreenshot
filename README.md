REaM screenshot
===============

Reverse Engineered and Modified screenshot plugin for 
NTR CFW 1.0 and 2.x

Since screenshot plugin source is not available publicly, 
I use the binary version then deassembly and decompile it.
Hopefully open sourcing could bring more 3DS *applications* 
with built-in screenshot capabilities (and making the 
improvement or fixes to the existing problems).

Currently ntr.bin still has a problem for a game like 
animal crossing when closing NTR CFW menu (i.e. bottom screen
replicates the top screen). Without any fix to that problem, 
in order to be able to get the bottom screenshot, the bottom 
screen is saved before the top one (notes: it may not work all 
the time) to quickly copy the data before changing.

Thanks to the following people for providing me with learning 
opportunity:

* cell9 for NTR CFW and all plugins
* patois for NTR CFW Disasm
* enler for NTR CFW DEX
* smealum for libctru
* Others who helped with testing and suggestions
