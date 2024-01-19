# =============================================================================
# Variables
# =============================================================================

CC 		= dpu-upmem-dpurte-clang
LD 		= dpu-upmem-dpurte-clang

CFLAGS 	= -Wall -Wextra -Wno-unused-label -Wno-unused-function -O3

TM 		= tiny
TMLIB 	= lib$(TM).a

SRCS	= tiny.c
OBJS	= ${SRCS:.c=.o}

RM		= rm -f

# =============================================================================
# Defines
# =============================================================================

ifdef TX_IN_MRAM
	DEFINES += -DTX_IN_MRAM
endif

ifdef LT_IN_MRAM
	DEFINES += -DLT_IN_MRAM
endif

ifdef DATA_IN_MRAM
	DEFINES += -DDATA_IN_MRAM
endif

ifdef BACKOFF
	DEFINES += -DBACKOFF
endif

ifdef WRITE_BACK_CTL
	DEFINES += -DWRITE_BACK_CTL
else
	ifdef WRITE_BACK_ETL
		DEFINES += -DWRITE_BACK_ETL
	else
		DEFINES += -DWRITE_THROUGH_ETL
	endif
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

# gcc -I/home/andre/Documents/PIM_tinySTM/src 
# -o prog main.c -L/home/andre/Documents/PIM_tinySTM/lib/ -l tiny