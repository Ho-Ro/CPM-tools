
CPMTOOLS = tar.com tarz.com time.com gunzip.com grep.com be.com \
u3.com unzip.com zip.com zipdir.com

TESTTOOLS = gunzipb.com gunzipu.com grep_tst.com

LINUXTOOLS = tinytar gunzip

CFLAGS = -Wall -Wextra -Wpedantic -std=c89


TNYLPO = ./tnylpo
Z80AS = $(TNYLPO) z80as
MAC = $(TNYLPO) mac
ZAS = $(TNYLPO) zas
ZMAC = $(TNYLPO) zmac
ZSM4 = $(TNYLPO) zsm4
LOAD = $(TNYLPO) load
LINK = $(TNYLPO) link
LINQ = $(TNYLPO) linq
ZML = $(TNYLPO) zml


all: $(LINUXTOOLS) $(CPMTOOLS)

grep.com: grep.as Makefile
	$(Z80AS) grep.as
	$(LINQ) -z -ptext=100h,data,bss -c100h -ogrep.com grep.obj

# test file for simpler CP/M build
grep_tst.com: grep_tst.as Makefile
	$(ZAS) grep_tst.as
	$(LINQ) -z -c100h -ogrep_tst.com grep_tst.obj

time.com: time.asm
	$(ZSM4) =time.asm
	$(LINK) time
	rm -f time.rel

tarz.com: tar.c Makefile
	zcc --opt-code-speed=all +cpm -o $@ $<

tar.com: tar.c Makefile
	$(TNYLPO) c:htc -v -o $<

be.com: be.c Makefile
	$(TNYLPO) c:htc -v -o -n $<

unzip.com: unzip.z80 Makefile
	$(ZMAC) unzip
	$(ZML) unzip
	rm -f unzip.rel

zip.com: zip.z80 Makefile
	$(ZMAC) zip
	$(ZML) zip
	rm -f zip.rel

zipdir.com: zipdir.z80 Makefile
	$(ZSM4) =zipdir.z80
	$(LINK) zipdir
	rm -f zipdir.rel

u3.com: u3.asm
	$(MAC) u3
	$(LOAD) u3
	rm -f u3.hex u3.prn u3.sym

# test files to compare buffered and unbuffered fgetc() anf fputc() speed of z88dk libc
gunzipb.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -o $@ $<

gunzipu.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -DUNBUFFERED -o $@ $<


gunzip.com: gunzip.c Makefile
	$(TNYLPO) c:htc -dMYTZ=1 -n -e$@ $<


tinytar: tar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<


gunzip: gunzip.c Makefile
	gcc $(CFLAGS) -o $@ $<


.PHONY: testtools
testtools: $(TESTTOOLS)


.PHONY: clean
clean:
	-rm -f *.err *.sym *.prn *.lis *~


.PHONY: distclean
distclean: clean
	-rm -f $(CPMTOOLS) $(LINUXTOOLS) $(TESTTOOLS)
