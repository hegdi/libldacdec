CC = gcc
CFLAGS = -MMD -MP -O2 -g
CFLAGS += -Wall -Wextra
CFLAGS += -Ilibldac/inc -Ilibldac/src
LDLIBS = -lm
VPATH += libldac/src/

all: ldacdec

ldacenc: ldacenc.o ldaclib.o ldacBT.o

ldacenc: LDLIBS = -lm -lsndfile -lsamplerate
ldacenc: encldac.o ldaclib.o ldacBT.o

ldacdec: ldacdec.o bit_allocation.o huffCodes.o bit_reader.o utility.o

mdct_imdct: LDLIBS = -lsndfile -lm
#mdct_imdct: CFLAGS += -DSINGLE_PRECISION
mdct_imdct: mdct_imdct.o ldaclib.o imdct.o

.PHONY: clean
clean:
	rm -f *.d *.o ldacenc ldacdec

test:
	./ldacinfo test.ldac

-include *.d


