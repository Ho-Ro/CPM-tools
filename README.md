# CPM-tools
Collection of useful tools for your (or at least my) every-day work with CP/M
that are either difficult to find or customised by me or are completely new.
Some software was created during [hacking the BIOS of my Z80-MBC2](https://github.com/Ho-Ro/Z80-MBC2)
(e.g. to add a high speed double SIO), other was written just for fun.

Most tools use either [HiTech C (version 3.09-19) from agn453](https://github.com/agn453/HI-TECH-Z80-C/tree/master/dist) (provided here for completeness) or the Z80 assembler/linker ZSM4/LINK or ZMAC/ZML.
The Pascal tools need an unmodified Turbo Pascal 3.

## diskedit

Disk editor for CP/M 3, written in Turbo Pascal by
[Gerhard Strube](https://mark-ogden.uk/mirrors/www.cirsovius.de/CPM/Projekte/Artikel/TP/DirDisk/DirDisk.html).
I added track formatting and timestamp preparation for the directory track(s) and improved the user interface.

## make

This program is a slightly simplified clone of the UNIX utility of the same name.
Dependancy information is extracted from a makefile and acted upon.
This version requires CP/M-80 version 3.0 ("CP/M PLUS") with time-of-day-clock implemented.
Also the file timestamps must be "turned on". Current time-of-day call to CP/M is also used.

## tar

I wrote this program simply because there was no CP/M tar implementation. It was written
as a cleanroom approach using only [RFC1951](https://www.rfc-editor.org/rfc/rfc1951) and
[RFC1952](https://www.rfc-editor.org/rfc/rfc1952) because I wanted to see how easy or
difficult it was - the former was the case.

```
 * tar.c - A minimal ANSI C89-compatible TAR archive utility.
 *
 * This program implements a simplified version of the UNIX tar utility,
 * supporting the creation, listing, extraction, and appending of files
 * to a standard POSIX/GNU-compatible TAR archive (USTAR format).
 *
 * Supported Modes (UNIX-compatible syntax):
 *   -cf archive.tar file1 [file2 ...]  # Create a new archive from files
 *   -rf archive.tar file1 [file2 ...]  # Append files to an existing archive
 *   -xf archive.tar                    # Extract all files from an archive
 *   -tf archive.tar                    # List contents of an archive
 *   archive.tar                        # Same as -tf archive.tar
 *
 * Features:
 *   - Fully ANSI C89 compatible (no POSIX-specific functions used).
 *   - Uses only the standard C library (stdio.h, string.h, etc.).
 *   - Validates USTAR magic to ensure archive format correctness.
 *   - Appending safely handles existing TAR structure and trailing blocks.
 *   - Automatically overwrites and rewrites final zero blocks.
 *   - Stores file modification time (or current time if mtime is missing).
 *   - Creates smaller files than (uncompressed) UNIX tar because it puts
 *     only 2 empty blocks at the archive end (as defined in the standard)
 *     instead of padding the archive to a multiple of 10 KiBi.
 *
 * Limitations:
 *   - Flat archives only (no directory tree structure).
 *   - No support for special files (symlinks, devices, etc.).
 *   - No built-in compression (to maintain POSIX/GNU tar compatibility).
 *
 * Target:
 *   The program is mainly intended for small 8-bit systems like CP/M 3.0
 *   that do provide neither directories nor specials files.
 *   It supports the grouping of multiple files and the transfer between
 *   CP/M and Linux with tools like KERMIT or [XYZ]MODEM.
 *
 * Building on Linux:
 *   gcc -Wall -Wextra -Wpedantic -std=c89 -o tinytar tar.c
 *   Keep the name "tinytar" to distinguish it from real tar.
 *
 * Cross-building for CP/M with Z88DK:
 *   zcc --opt-code-speed +cpm -o tar.com tar.c
 *   This creates the file TAR.COM that resembles the std. tar commands.
 *
 * Building on CP/M with HiTech C compiler (needs version 3.09-19 or later):
 *   C309-19 -V -O TAR.C
 *   This creates the file TAR.COM that resembles the std. tar commands.
 *
 *
```

## gunzip

Based on a simple Unix [zcat](https://github.com/pts/pts-zcat) I hacked a CP/M version that
can be compiled with HiTech C. This compiler is almost ANSI-C standard.

```
 * muzcat_simple.c -- decompression filter in simple, portable C
 * by pts@fazekas.hu at Tue Jun 16 16:50:24 CEST 2020
 * The implementation was inspired by https://www.ioccc.org/1996/rcm/index.html
 *
 * This tool is a slow drop-in replacmeent of zcat (gzip -cd), without error
 * handling. It reads compressed data (can be gzip, flate or zip format) on
 * stdin, and it writes uncompressed data to stdout. There is no error
 * handling: if the input is invalid, the tool may do anything.
```

Modifications and supported features

```
 * * usage:
 *     gunzip <infile> [-o | <outfile>]
 *     gunzip <infile>            - list archive status
 *     gunzip <infile> <outfile>  - unzip <infile> producing <outfile>
 *     gunzio <infile> -o         - unzip <infile> producing the original file
 * * Writing output to a file instead of stdout allows to uncompress original binary formats.
 * * Error handling: check magic and compression mode before processing,
 *   calculate CRC32 during expanding and check against CRC32 of archive.
 * * Compile for Linux: `gcc -Wall -Wextra -Wpedantic -std=c89 -o gunzip gunzip.c`
 *   Supports display of mtime stored in archive.
 * * Compiles for CP/M using HI-TECH C: 'cc -v -n gunzip.c'.
 *   Do not optimise '-o', OPTIM.COM stops due to 'Out of memory'.
 * * CP/M needs a hack to get the real file size (w/o trailing ^Z)
 *   and convert ^Z at end of file into C EOF (-1).
 * * CP/M Program becomes too big when using time and date functions.
```

## grep

Based on [grep.as](https://github.com/Laci1953/RC2014-CPM/blob/main/System/grep/grep.as) with minor modifications for easy build on CP/M using [z80as](https://github.com/Laci1953/HiTech-C-compiler-enhanced/tree/main/Z80AS) and `linq` from HiTech C.

```
z80as grep.as
linq -z -ptext=100h,data,bss -c100h -ogrep.com grep.obj
```

```
Use: grep filename.ext
please enter the string to be searched, followed by a <CR>:

Lists the files containing the specified string
( ambiguous file references may be used, e.g. *.c or test?.asm )
( up to 512 files can be searched )
```

## be

Binary editor, written by [Lars Lindehaven](https://github.com/lindehaven/CP-M/tree/master/be) for CP/M 3 and adapted
by [Ladislau Szilagyi](https://github.com/Laci1953/RC2014-CPM/tree/main/System/BinaryEditor), who fixed it for CP/M 2.2 and adapted it for the HiTech C compiler.
BE enables hexadecimal and ASCII editing of binary files up to 37.5 KB (0x9600) in size.

## unzip

Slightly modification of [UNZIP187.Z80](https://github.com/agn453/UNZIP-CPM-Z80/blob/master/unzip/UNZIP187.Z80).

This version of `UNZIP` now can extract all files from MS-DOS and Linux ZIP files.
The addition of the UnDeflate decompression method make this possible since
previous versions could not handle Deflated files. Additionally it sets
the date stamp of the extracted file is the OS supports date stamping.

Usage Syntax:

    UNZIP {d:}zipfile {d:}{afn}

If no destination drive is given, member files are checked and listed that match `afn`.
If a destination drive is given, member files are extracted that match `afn` if given,
otherwise all member files are extracted.
This follows the syntax used by for example UNARC.


