
TARGETS = tinytar tar.com

all: $(TARGETS)

tinytar: tinytar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<

tar.com: tinytar.c Makefile
	zcc +cpm -v -DWILDCARD -o $@ $<

.PHONY: clean
clean:
	rm $(TARGETS)
