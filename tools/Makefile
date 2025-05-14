
CPMTOOLS = tar.com gunzip.com HiTechC_tools/gunzip.com

LINUXTOOLS = tinytar gunzip

CFLAGS = -Wall -Wextra -Wpedantic -std=c89

all: $(LINUXTOOLS) $(CPMTOOLS)


tar.com: tar.c Makefile
	zcc --opt-code-speed +cpm -o $@ $<


gunzip.com: gunzip.c Makefile
	zcc --opt-code-speed +cpm -DMYTZ=1 -o $@ $<


HiTechC_tools/gunzip.com: HiTechC_tools/gunzip.c
	make -C HiTechC_tools

tinytar: tar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<


gunzip: gunzip.c Makefile
	gcc $(CFLAGS) -o $@ $<


.PHONY: clean
clean:
	make -C HiTechC_tools clean
	-rm -f $(CPMTOOLS) $(LINUXTOOLS) *~
