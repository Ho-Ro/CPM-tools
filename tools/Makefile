
CPMTOOLS = tar.com gunzip.com
LINUXTOOLS = tinytar gunzip

CFLAGS =  -Wall -Wextra -Wpedantic -std=c89

all: $(CPMTOOLS) $(LINUXTOOLS)


gunzip.com: gunzip.c
	tnylpo cc -v -e$@ $<

gunzip: gunzip.c
	gcc $(CFLAGS) -o $@ $<


tar.com: tinytar.c
	tnylpo cc -v -e$@ $<

tinytar: tinytar.c
	gcc $(CFLAGS) -o $@ $<


.PHONY: clean
clean:
	-rm $(CPMTOOLS) $(LINUXTOOLS) *~
