CC = gcc

CFLAGS = -MMD -MP -O2 -g
CFLAGS += -Wall -Wextra
CFLAGS += -Ilibldac/inc -Ilibldac/src
#CFLAGS += -DDEBUG
#CFLAGS += -fsanitize=address
LDLIBS = -lm
#LDFLAGS = -fsanitize=address
VPATH += libldac/src/
LDFLAGS += -L.

all: ldacdec ldacenc

libldacdec.so: LDFLAGS += -shared -fpic -Wl,-soname,libldacdec.so.1
libldacdec.so: CFLAGS += -fpic
libldacdec.so: libldacdec.o bit_allocation.o huffCodes.o bit_reader.o utility.o imdct.o spectrum.o

ldacenc: ldacenc.o ldaclib.o ldacBT.o

ldacenc: LDLIBS += -lsndfile -lsamplerate
ldacenc: ldacenc.o ldaclib.o ldacBT.o

ldacdec: ldacdec.o libldacdec.so
ldacdec: LDFLAGS += -Wl,-rpath=.
ldacdec: LDLIBS += -lldacdec -lsndfile

mdct_imdct: LDLIBS += -lsndfile
#mdct_imdct: CFLAGS += -DSINGLE_PRECISION
mdct_imdct: mdct_imdct.o ldaclib.o imdct.o

%.so:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.d *.o ldacenc ldacdec libldacdec.so

-include *.d


