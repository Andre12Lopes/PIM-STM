# =============================================================================
# Variables
# =============================================================================

CC 		= dpu-upmem-dpurte-clang
LD 		= dpu-upmem-dpurte-clang

CFLAGS 	= -Wall -Wextra -Wno-unused-label -Wno-unused-function -Os

TM 		= norec
TMLIB 	= lib$(TM).a

SRCS	= norec.c
OBJS	= ${SRCS:.c=.o}

RM		= rm -f

# =============================================================================
# Defines
# =============================================================================

ifdef TX_IN_MRAM
	DEFINES += -DTX_IN_MRAM
endif

ifdef DATA_IN_MRAM
	DEFINES += -DDATA_IN_MRAM
endif

ifdef BACKOFF
	DEFINES += -DBACKOFF
endif

# =============================================================================
# Rules
# =============================================================================

.PHONY: all clean
all: $(TMLIB)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

$(TMLIB): $(OBJS)
	$(AR) crs $@ $^

clean:
	$(RM) $(TMLIB) $(OBJS)
