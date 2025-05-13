
TARGETS = tinytar tar.com

all: $(TARGETS)

tar.com: tar.c Makefile
	zcc --opt-code-speed  +cpm -o $@ $<

tinytar: tar.c Makefile
	gcc -Wall -Wextra -Wpedantic -std=c89 -o $@ $<

.PHONY: clean
clean:
	rm $(TARGETS)

