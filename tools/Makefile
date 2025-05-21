
CPMTOOLS = tar.com time.com gunzip.com gunzipu.com HiTechC_tools/gunzip.com

LINUXTOOLS = tinytar gunzip

CFLAGS = -Wall -Wextra -Wpedantic -std=c89

all: $(LINUXTOOLS) $(CPMTOOLS)

time.rel: time.asm
	cpm zsm4 =$<

time.com: time.rel
	cpm link $<

tar.com: tar.c Makefile
	zcc --opt-code-speed=all +cpm -o $@ $<


gunzip.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -o $@ $<


gunzipu.com: gunzip.c Makefile
	zcc --opt-code-speed=all +cpm -DMYTZ=1 -DUNBUFFERED -o $@ $<


HiTechC_tools/gunzip.com: HiTechC_tools/gunzip.c
	make -C HiTechC_tools

tinytar: tar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<


gunzip: gunzip.c Makefile
	gcc $(CFLAGS) -o $@ $<


.PHONY: clean
clean:
	make -C HiTechC_tools clean
	-rm -f $(CPMTOOLS) $(LINUXTOOLS) *.rel *~
