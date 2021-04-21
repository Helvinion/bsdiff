CC=gcc
CC_FLAGS=-Wall -Werror -Wextra
CC_DIFF_DEFINES=-DBSDIFF_EXECUTABLE
CC_PATCH_DEFINES=-DBSPATCH_EXECUTABLE
LD_FLAGS=-lbz2

BSDIFF=bsdiff
BSDIFF_SRC=bsdiff.c

BSPATCH=bspatch
BSPATCH_SRC=bspatch.c

all: bsdiff bspatch

${BSDIFF}: ${BSDIFF_SRC}
	${CC} ${CC_FLAGS} ${CC_DIFF_DEFINES} $^ -o $@ ${LD_FLAGS}

${BSPATCH}: ${BSPATCH_SRC}
	${CC} ${CC_FLAGS} ${CC_PATCH_DEFINES} $^ -o $@ ${LD_FLAGS}

clean::

distclean:: clean
	rm -f ${BSDIFF}
	rm -f ${BSPATCH}
