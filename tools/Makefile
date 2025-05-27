
CPMTOOLS = tar.com time.com gunzip.com

TESTTOOLS = gunzipb.com gunzipu.com

LINUXTOOLS = tinytar gunzip

CFLAGS = -Wall -Wextra -Wpedantic -std=c89

all: $(LINUXTOOLS) $(CPMTOOLS)

time.rel: time.asm
	cpm zsm4 =$<

time.com: time.rel
	cpm link $<

tar.com: tar.c Makefile
	zcc --opt-code-speed=all +cpm -o $@ $<


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


.PHONY: clean
clean:
	-rm -f $(CPMTOOLS) $(LINUXTOOLS) $(TESTTOOLS) *.rel *~
