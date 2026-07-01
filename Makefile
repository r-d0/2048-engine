CFLAGS := -Wall -Werror -MMD -MP -Iinclude -O2 -O3
LDFLAGS := -lncurses -lm

SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c, build/%.o, $(notdir $(SRC)))
DEPS := $(OBJ:.o=.d)

all: exec

exec: $(OBJ)
	gcc $^ $(LDFLAGS) -o $@ 

build/%.o: src/%.c | build
	gcc $< -c -o $@ $(CFLAGS)

build:
	mkdir -p build

-include $(DEPS)

clean:
	rm -rf build/ exec

.PHONY: all clean
