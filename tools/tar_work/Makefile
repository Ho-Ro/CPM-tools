
TARGETS = tinytar tar.com

all: $(TARGETS)

tar.com: tinytar.c Makefile
	zcc +cpm -v -o $@ $<

tinytar: tinytar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<

.PHONY: clean
clean:
	rm $(TARGETS)

