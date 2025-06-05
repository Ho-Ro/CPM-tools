
CPMTOOLS = tar.com tarz.com time.com gunzip.com grep.com be.com unzip.com zip.com zipdir.com

TESTTOOLS = gunzipb.com gunzipu.com grep_tst.com

LINUXTOOLS = tinytar gunzip

CFLAGS = -Wall -Wextra -Wpedantic -std=c89

all: $(LINUXTOOLS) $(CPMTOOLS)

grep.com: grep.as Makefile
	./tnylpo z80as grep.as
	./tnylpo linq -z -ptext=100h,data,bss -c100h -ogrep.com grep.obj

# test file for simpler CP/M build
grep_tst.com: grep_tst.as Makefile
	./tnylpo zas grep_tst.as
	./tnylpo linq -z -c100h -ogrep_tst.com grep_tst.obj

time.com: time.asm
	./tnylpo zsm4 =time.asm
	./tnylpo link time
	rm -f time.rel

tarz.com: tar.c Makefile
	zcc --opt-code-speed=all +cpm -o $@ $<

tar.com: tar.c Makefile
	tnylpo c:htc -v -o $<

be.com: be.c Makefile
	tnylpo c:htc -v -o -n $<

unzip.com: unzip.z80 Makefile
	tnylpo zmac unzip
	tnylpo zml unzip
	rm -f unzip.rel

zip.com: zip.z80 Makefile
	tnylpo zmac zip
	tnylpo zml zip
	rm -f zip.rel

zipdir.com: zipdir.z80 Makefile
	tnylpo zsm4 =zipdir.z80
	tnylpo link zipdir
	rm -f zipdir.rel

# test files to compare buffered and unbuffered fgetc() anf fputc() speed of z88dk libc
gunzipb.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -o $@ $<

gunzipu.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -DUNBUFFERED -o $@ $<


gunzip.com: gunzip.c Makefile
	./tnylpo c:htc -dMYTZ=1 -n -e$@ $<


tinytar: tar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<


gunzip: gunzip.c Makefile
	gcc $(CFLAGS) -o $@ $<


.PHONY: testtools
testtools: $(TESTTOOLS)


.PHONY: clean
clean:
	-rm -f *.err *.sym *~


.PHONY: distclean
distclean: clean
	-rm -f $(CPMTOOLS) $(LINUXTOOLS) $(TESTTOOLS)
