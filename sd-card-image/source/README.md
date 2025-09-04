# Source files for Pico-6502 binaries

Included are the source files for some of the binaries, included with the sd-card image.

You can compile them using vasm. I found the best way is to use the vasm.tar.gz file,
since the github version seems to be broken:

http://sun.hasenbraten.de/vasm/index.php?view=relsrc

I compiled them using the vasm_oldstyle assembler.

An example of the command line, to assemble source is as follows:

vasm6502_oldstyle -Fbin -dotdir -o echo.bin -c02 echo.s
