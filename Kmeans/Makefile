# =============================================================================
# Variables
# =============================================================================

CC 			= dpu-upmem-dpurte-clang
CPP			= g++

NR_TASKLETS = 11

COMMON_FLAGS = -Wall -Wextra -Wno-unused-label -Wno-unused-function
DPU_FLAGS	= $(COMMON_FLAGS) -O3 --std=c11 -I../Lib -I../Backends/$(FOLDER)
HOST_FLAGS	= $(COMMON_FLAGS) -O3 --std=c++11 -DNR_TASKLETS=$(NR_TASKLETS)

LDFLAGS  	= -L../Backends/$(FOLDER)
LIBS     	= -l$(LIB)

BUILD_DIR	= bin
HOST_DIR	= host
DPU_DIR		= dpu

HOST_TARGET = ${BUILD_DIR}/host
DPU_TARGET 	= ${BUILD_DIR}/dpu

HOST_SRCS 	= $(wildcard $(HOST_DIR)/*.cpp)
HOST_OBJS	= ${HOST_SRCS:.cpp=.o}
DPU_SRCS	= $(wildcard $(DPU_DIR)/*.c)
DPU_OBJS	= ${DPU_SRCS:.c=.o}

RM			= rm -f

DIR 		:= $(shell mkdir -p ${BUILD_DIR})

# =============================================================================
# Defines
# =============================================================================

DEFINES += -DNR_TASKLETS=$(NR_TASKLETS)

ifdef TX_IN_MRAM
	DEFINES += -DTX_IN_MRAM
endif

ifdef DATA_IN_MRAM
	DEFINES += -DDATA_IN_MRAM
endif

ifdef BACKOFF
	DEFINES += -DBACKOFF
endif

ifdef N_CLUSTERS
	DEFINES += -DN_CLUSTERS=$(N_CLUSTERS)
endif

# =============================================================================
# Rules
# =============================================================================

.PHONY: all clean
all: $(DPU_TARGET) $(HOST_TARGET)

%.o: %.c
	$(CC) $(DPU_FLAGS) $(DEFINES) -c $< -o $@

%.o: %.cpp
	$(CC)  -c $< -o $@

$(DPU_TARGET): $(DPU_OBJS)
	$(CC) $(LDFLAGS) -DNR_TASKLETS=$(NR_TASKLETS) $^ $(LIBS) -o $@

$(HOST_TARGET): $(HOST_SRCS)
	$(CPP) $(HOST_FLAGS) $^ -o $@ `dpu-pkg-config --cflags --libs dpu` -g

clean:
	$(RM) -r $(BUILD_DIR) $(HOST_DIR)/*.o $(DPU_DIR)/*.o