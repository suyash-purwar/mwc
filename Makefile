TARGET: mwc

PHONY: all clean

all: $(TARGET)

run: mwc
	./mwc

mwc: mwc.c
	gcc cores.c mwc.c -o build/build

clean:
	rm -rf mwc
