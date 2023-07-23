# tcpm &mdash; runs CP/M 2 under tnylpo

## What is this?
`tcpm` is  a CP/M program (well, sort of &mdash; it needs to be run under
`tnylpo`) which implements a full CP/M 2 (C)BIOS and thereby allows the actual
DRI CP/M 2 operating system (CCP and BDOS) to execute under `tnylpo`.

While character I/O (console, printer, paper tape punch, and paper
tape reader) calls are simply passed through to `tnylpo`, the disk specific
BIOS calls (which are just dummies under `tnylpo`) are implemented using disk
image files, which are accessed via the BDOS implementation of `tnylpo`.
In this way, `tcpm` supports two disk drives with a capacity of 1 megabyte
each.

`tcpm` is accompanied by two companion programs, `tcpm_disk` and `sysutil`.
`tcpm_disk` is a program running under the host operating system of `tynlpo`
and allows the creation of the disk image files used by `tcpm`, while `sysutil`
is a CP/M executable which allows file transfers into and out of the
`tcpm` disk images, dynamic swapping of disk images (i.e., the equivalent
of swapping floppy disks in a real CP/M system), and exiting the `tcpm`
environment.

## How do I get `tcpm` resp. CP/M running?
Unfortunately, this is not completely straightforward, since some
components needed (above all, CP/M itself) are not free software, so I
cannot provide you with a turnkey solution.

You'll need
* a working installation of `tnylpo`
* a C compiler on your host operating system (to compile `tcpm_disk`)
* a system image of CP/M 2 (i.e., a file containing the CCP and BDOS
binaries, see below)
* the memory size in kilobytes for which this system image has been created
(possible values are 20 to 64, and if you have a `CPMnn.COM` file, this
value usually corresponds to the `nn` part of the file name; if you have a
file called `CPM62.COM`, the memory size should be 62 kilobytes)
* Microsoft's Utility Software Package or rather the Macro-80/Link-80
assembler and linker (`m80.com` and `l80.com`) from this package (to
assemble and link `tcpm` and `sysutil`)
* (optional) the utility programs from the CP/M 2 distribution like `pip.com`,
`stat.com`, or `movcpm.com` (this you will need if you want to change the
memory size of your CP/M system image later on)

Please bear in mind that CP/M 2, its utilities and the Utility Software
Package may be old and nowadays of little commercial value, but they are still
not free software.

Compile `tcpm_disk.c` using your host system C compiler:
```sh
cc -o tcpm_disk tcpm_disk.c
```

Convert `sysutil.mac` to the CP/M line end convention and assemble
and link it using `tnylpo` and `m80.com`/`l80.com`:
```sh
tnylpo-convert -u sysutil.mac -c sysutil.mac
tnylpo m80 =sysutil
tnylpo l80 sysutil/n,sysutil/e
```
Some shells require you to use `\=` instead of `=` in the `tnylpo m80` line.

Create a system disk image for disk A. This example assumes that you have a
CP/M 2 system image called `cpm62.com`, which was created by `MOVCPM`/`SAVE`
(see below);
the name of the new disk image must be acceptable for `tnylpo` (8.3, only
lower case letters), and `diska.dta` is the default value assumed by `tcpm`:
```sh
./tcpm_disk -m cpm62.com -p sysutil.com create diska.dta
```
This will create a bootable disk image which contains a single file,
`SYSUTIL.COM`, which can be used to copy further files into your `tcpm`
disk(s).

(Optional) Create a disk image for disk B (again, you may choose your
own name, but `diskb.dta` is the default):
```sh
./tcpm_disk create diskb.dta
```
This will create a non-bootable disk image with an empty directory.

Edit `tcpm.mac` with your favourite text editor (`vi`, right?). Change
the definition of the symbol `sys_size_kb` near the top of the file to your
memory size in kilobytes (`62` in our example).

Then convert `tcpm.mac` to the CP/M line end convention and assemble/link
it using `tnylpo` and `m80.com`/`l80.com`:
```sh
tnylpo-convert -u tcpm.mac -c tcpm.mac
tnylpo m80 =tcpm
tnylpo l80 tcpm/n,tcpm/e
```
(Again, you may have to change `=` to `\=`.)

Now you should be able to boot CP/M:
```sh
tnylpo tcpm
```
If you chose to use an image name other than `diska.dta` (resp.
`diskb.dta`), e.g., `tom.dsk` (resp. `jerry.dsk`), you will have
to supply it on the `tcpm` command line:
```sh
tnylpo tcpm tom.dsk jerry.dsk
```
If you didn't create a disk image for drive B, just omit it on the
command line. This will give you a CP/M system in which only disk A
is accessible (until you use e.g., `sysutil change b: x:jerry.dsk` to
attach a disk image to drive B).

CP/M should now start up and show you something like:
```
62K CP/M VERS 2.2

A>
```
You can now enter CP/M commands like `dir`:
```
A>dir
A: SYSUTIL  COM
A>
```
There should be a single file on disk A, `sysutil.com`, which was placed there
by `tcpm_disk` during image initialization. To exit CP/M (and `tnylpo`), type:
```
A>sysutil halt
bye
```

## How do I populate a disk image with files?
`sysutil` provides the `copy` subcommand to transfer files into (and out of)
a `tcpm` disk image. The special drive designation `x:` allows you to
access files in the directory `tnylpo` uses as its drive A (by default, this
is the current working directory at the time you started `tnylpo tcpm`). Since
the files are accessed using the BDOS emulation of `tnylpo`, only files
conforming to `tnylpo`'s file name restrictions (8.3, only lower case
letters) are accessible. To copy e.g., `pip.com` and `stat.com` into
disk image A (assuming they already reside in the directory `tnylpo` uses
as drive A), use:
```
A>sysutil
*copy x:pip.com a:pip.com
*copy x:stat.com a:
*

A>
```
Note that if you specify only a drive name as destination, `sysutil copy` will
use the file name and extension of the source file. Furthermore, `sysutil`
doesn't support wildcard characters in file names, so you need to copy
files one by one. Please keep in mind that the `x:` drive/directory usually
also contains the disk image files for drive A and B as well as `tcpm.com`,
so be careful not to overwrite them when you copy files out of the
CP/M environment.

The command
```
A>sysutil dir x:
```
will show you the contents of of the `x:` directory (or rather the
files conforming to the file name restrictions of `tnylpo` residing there).

## How do I change or detach disk images?
Again, this is done using `sysutil`. The `change` subcommand allows you to
attach a new disk image to drive A or B, implicitly detaching the currently
attached image (this is akin to swapping floppy disks in a real
CP/M system). When changing the image for drive A, `sysutil` will
check if the new disk image contains a CP/M image and otherwise will refuse
the change.

An image attached to drive B may be detached with the
`eject` subcommand of `sysutil`, which will cause drive B to be unaccessible
until a new disk image is attached with the `change` subcommand. Since
drive A must be attached to a system disk image at all times, `eject`
does not work with this drive.

The current state of both drives and their attached disk images may be
inspected with the `status` subcommand of `sysutil`.
```
A>sysutil change a: x:turbo.dta
A>sysutil eject b:
A>sysutil status
drive a: x:turbo.dta
drive b: no image

A>
```

## What is the format of the disk images?
`tcpm` disk images consist of 8192 records of 128 bytes and therefore have
a total capacity of one megabyte. Logically, they are structured into 128
tracks of 64 sectors each, of which the fist track is reserved for the
CP/M image and the second track contains the disk directory.

The first sector on the first track contains a signature in its first two
bytes, either `DS` for a system disk image (containing CCP and BDOS binaries
in sectors 2 to 45 of the first track) or `DD` for a data disk image (which
may be attached to drive B only). Disk images may be converted from system
to data disk images and vice versa using the `sys` and `nosys` subcommands of
`tcpm_disk`. Sectors 46 to 64 of the first track of a system disk image and
sectors 2 to 64 of the first track of a data disk image
(as well as bytes 3 to 128 of the first sector
on the first track) are not used in any way (resp. are reserved for malware).

The second track contains 64 directoy records of 4 directory entries each,
giving a maximum of 256 directory entries per disk. 

Tracks 3 to 128 are structured into 252 disk blocks of 32 sectors
(4 kilobytes, two blocks per track), which (including the two directory blocks)
gives the disk image a total capacity of 254 blocks and results in eight
bit block numbers in the directory entries (16 blocks, i.e. 64 kilobytes
per physical extent).

No sector translation is performed by the BIOS, i.e., the sectors of a
track are stored in ascending order, followed by the sectors of the next
higher track.

Since CP/M never tries to read a data block it has not written to beforehand,
`tcpm_disk` by default allocates only the system and directory tracks (and any
data blocks it needs to store the program file specified with the `-p`
option) on creating a disk image. If you need to allocate the full megabyte of
a disk image (e.g., if you need to run a program which uses direct BIOS calls
to do its own data management), you can request this with the `-a` option
during image creation.

## What is a CP/M 2 system image?
This is a file containing (at least) the binary code for the CP/M 2
Console Command Processor (CCP) and Basic Disk Operating System (BDOS)
for a specific memory configuration (20 kilobytes at minimum and
64 kilobytes at maximum, putting the
start of the CCP to location 3400h resp. 0e400h at runtime);
additionally, system images may contain the code of a boot loader and
of the Basic Input/Output System (BIOS) as well, but in the `tcpm`
environment, these are not required (resp. supplied by `tcpm`). The CCP is
exactly 2048 bytes in size, and the BDOS 3584 bytes.
A raw system image starts with the CCP code, followed by the BDOS code
(and optionally, the BIOS code), while a `MOVCPM` system image created
e.g., by the CP/M command sequence
```
A>movcpm 64 *
A>save 34 cpm64.com
```
will contain the CCP at file offsets 880h to 107fh and the BDOS at file
offsets 1080h to 1e7fh. `tcpm_disk` supports both variants with its
`-s` resp. `-m` options (if your system images uses some other layout,
you will have to extract the CCP and BDOS parts using the `dd` command,
effectively creating a raw system image).

## How does CP/M (resp. how does `tcpm`) interact with `tnylpo`?
`tnylpo` doesn't care too much about what happens with the data
structures it stored in the memory of the virtual machine; most of them are
just initialized before program startup for the benefit of the application
program (to supply it with a CP/M compatible environment) and are never
referenced by `tnylpo` itself (e.g., the jump to the BDOS in location 5,
the allocation vector, the disk parameter block, or the BIOS jump vector).
What usually needs to be preserved are the `ret` instructions in the topmost
19 bytes of main memory, since the system calls emulated by `tnylpo` are
activated by executing them.

In the current implementation, the BIOS installed by `tcpm` is small enough
that it can coexist with `tnylpo`'s minimal BDOS and BIOS areas even if the
maximal CP/M memory configuration (64 kilobytes) is in use, but this may
change if the `tcpm` BIOS grows in a future version. 

User programs running in the `tcpm` environment can use the `tnylpo` BDOS
and BIOS emulation (at the price of no longer being conforming CP/M programs)
&mdash; this is in fact how `sysutil` and the `tcpm` BIOS itself access
external files and disk images.
Of course, `tnylpo` system services no longer can be activated by
calling location 5 (or by using the address in location 1 to access the BIOS
jump vector), since this now belongs to CP/M. Instead, the 'magic'
`ret` instructions need to be called directly, e.g., `call 0ffffh-18` will
activate the BDOS emulation (likewise, `call 0ffffh-13` will call the emulated
CONOUT BIOS entry).

If you plan to call the `tnylpo` interface from your programs, keep the
following caveats in mind:
* don't use `Reset Disk System` (BDOS 13) or `Select Disk` (BDOS 14),
since these modify the drive number in location 4 which now belongs to CP/M.
* likewise, don't use `Get I/O Byte` (BDOS 7) or `Set I/O Byte` (BDOS 8),
since they access the I/O byte in location 3, which now belongs to CP/M
(but since the `tcpm` BIOS ignores the I/O byte anyway, this doesn't
really matter).
* calling `System Reset` (BDOS 0) (or jumping to location 0ffffh-16 to
activate the BIOS WBOOT entry) will terminate `tnylpo` and thus implicitly
the `tcpm` environment.
* avoid using `Get Addr(Alloc)` (BDOS 27) or `Get Addr(Diskparams)` (BDOS 31),
since they return pointers to data in the `tnylpo` BIOS area, which in future
releases of `tcpm` might have been overwritten by an enlarged `tcpm` BIOS.

## Who needs this?
Admittedly, hardly anybody. But it was fun to write some Z80 assembly
language code again, and I needed to check a compatibility issue
between CP/M 2 and `tnylpo` and still have no access to a real CP/M 2
computer (using one of the many competing CP/M emulators was naturally
no option).

## What is the legal situation?
`tcpm`, `tcpm_disk` and `sysutil` were written by Georg Brein
(`tnylpo@gmx.at`) and are subject to the same BSD-style license as `tnylpo`.
