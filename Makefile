CC = gcc
CFLAGS = -g -Wall -Wextra -Wpedantic -Wconversion -Wdouble-promotion -Wunused -Wshadow -Wsign-conversion -fsanitize=undefined
INCLUDES = -I.

COMMON_SRCS = fs_utils.c bitmap.c allocator.c dir.c 
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

# mkfs
MKFS_SRCS = mkfs.c
MKFS_OBJS = $(MKFS_SRCS:.c=.o) $(COMMON_OBJS)
MKFS_TARGET = mkfs.pangya

# df
DF_SRCS = df.c
DF_OBJS = $(DF_SRCS:.c=.o) $(COMMON_OBJS)
DF_TARGET = df.pangya

# ls
LS_SRCS = ls.c
LS_OBJS = $(LS_SRCS:.c=.o) $(COMMON_OBJS)
LS_TARGET = ls.pangya

# stat
STAT_SRCS = stat.c
STAT_OBJS = $(STAT_SRCS:.c=.o) $(COMMON_OBJS)
STAT_TARGET = stat.pangya

# mkdir
MKDIR_SRCS = mkdir.c
MKDIR_OBJS = $(MKDIR_SRCS:.c=.o) $(COMMON_OBJS)
MKDIR_TARGET = mkdir.pangya

# rm
RM_SRCS = rm.c
RM_OBJS = $(RM_SRCS:.c=.o) $(COMMON_OBJS)
RM_TARGET = rm.pangya

# cp
CP_SRCS = cp.c
CP_OBJS = $(CP_SRCS:.c=.o) $(COMMON_OBJS)
CP_TARGET = cp.pangya

# TARGETS
TARGETS = $(MKFS_TARGET) $(DF_TARGET) $(LS_TARGET) $(STAT_TARGET) $(MKDIR_TARGET) $(RM_TARGET) $(CP_TARGET)
OBJS = $(COMMON_OBJS) $(MKFS_SRCS:.c=.o) $(DF_SRCS:.c=.o) $(LS_SRCS:.c=.o) $(STAT_SRCS:.c=.o) $(MKDIR_SRCS:.c=.o) $(RM_SRCS:.c=.o) $(CP_SRCS:.c=.o)

.PHONY: all clean

all: $(TARGETS)

$(MKFS_TARGET): $(MKFS_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(DF_TARGET): $(DF_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(LS_TARGET): $(LS_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(STAT_TARGET): $(STAT_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(MKDIR_TARGET): $(MKDIR_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(RM_TARGET): $(RM_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

$(CP_TARGET): $(CP_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGETS)
