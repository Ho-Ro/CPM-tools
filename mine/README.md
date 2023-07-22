# mine &mdash; get rid of those nasty mines!
## What is this?
`mine` is a simple text-based implementation of the ubiquitous
`minesweeper` game. It is included in `tnylpo` as an unencumbered
demonstration program (`mine` is subject to the same license as
`tnylpo`, see the comment at the beginning of the source code).
## Why is there a binary included?
Since `mine` has the purpose to serve as a handy test program for
`tnylpo`, its source code (`mine.c`) is accompanied by a precompiled
CP/M binary (`mine.com`) to allow immediate use.
## Never trust any binary you haven't compiled yourself!
A worthy standpoint. Go ahead, review the source code and compile it
yourself!

I used the HI-TECH Z80 CP/M C compiler V3.09 (available e. g.
from [http://www.retroarchive.org/cpm/cdrom/SIMTEL/HITECH-C/](http://www.retroarchive.org/cpm/cdrom/SIMTEL/HITECH-C/)), which
"is provided free of charge for any use, private or commercial, strictly
as-is" (see the accompanying documentation) to develop `mine`
and to create `mine.com`.

After downloading the compiler and its companion programs (and
after converting all filenames to lower case), you can recreate `mine.com`
by the command
```sh
tnylpo -b c -o mine.c
```
By the way, are you sure you trust this compiler? (kudos to [Ken Thompson,
Reflections on trusting trust, Communications of the ACM Volume 27 Issue 8,
Aug 1984, 761-763.](https://dl.acm.org/citation.cfm?id=358210))
## How do I run it?
Assuming you already compiled and installed `tnylpo`, start `mine` by
the command
```sh
tnylpo -f mine.conf mine
```
`mine.conf` is an included `tnylpo` configuration file selecting the
full screen console emulation and `tnylpo`'s proprietary eight-bit
character set. On an ASCII-only terminal, use the command
```sh
tnylpo -f mine_ascii.conf mine
```
instead; this will cause `tnylpo` to approximate the box drawing
characters with ASCII symbols. 
## Can I use this program anywhere else?
Sure, but you will have to modify it. Even though `mine` is a
fairly standard CP/M-80 program, it uses features of the full screen
emulation provided by `tnylpo` (reverse and bold video, VT100 box
drawing characters, characters from the ISO-8859-1 character set,
and VT52 function and cursor key sequences); porting to non-CP/M
platforms is even more work, since the code has been optimized for
size, not for portability, and contains direct BIOS calls (instead of
calls to the standard library) and compiler dependencies (e. g., `char`
is assumed to be signed, since the "almost-ANSI" HI-TECH C compiler
doesn't support the `signed` keyword).
## How do I play this game?
More or less like graphical versions of `minesweeper`, but you'll have
to use the cursor keys (or `^E`, `^S`, `^X`, and `^D`) to move from
square to square. Open a square by pressing `F1` (or the space bar),
set or reset a
flag by pressing `F3` (or the return key), and open all unflagged neighbouring
squares of an already opened square by pressing `F2` (or the tabulator key).
The five levels of the game differ only in the size of the board.
## I want to quit this sh...y game!
Press `F10`, `tnylpo`'s "kill-this-sh...y-program" key, or activate the
nearest mine.
## Who wrote this piece of s...oftware?
Georg Brein, `tnylpo@gmx.at`
