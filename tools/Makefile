
CPMTOOLS = tar.com gunzip.com

LINUXTOOLS = tinytar gunzip

CFLAGS =  -Wall -Wextra -Wpedantic -std=c89

all: $(CPMTOOLS) $(LINUXTOOLS)


gunzip: gunzip.c Makefile
	gcc $(CFLAGS) -o $@ $<


gunzip.com: gunzip.c Makefile
	tnylpo cc -v -n -dMYTZ=1 -e$@ $<


tar.com: tinytar.c Makefile
	make -C tar_work $@
	cp tar_work/$@ .

tinytar: tinytar.c Makefile
	make -C tar_work $@
	cp tar_work/$@ .


.PHONY: clean
clean:
	make -C tar_work clean
	-rm $(CPMTOOLS) $(LINUXTOOLS) *~
