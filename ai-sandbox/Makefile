CC      = gcc
CFLAGS  = -Wall -Wextra -D_GNU_SOURCE
LDFLAGS = -lyaml

TARGET  = ai-run


SRCS =  src/main.c \
        src/namespace.c \
        src/policy.c \
        src/network.c \
        src/firewall.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "✓ Build complete!"
	@echo "Run with: sudo ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "✓ Clean complete"

.PHONY: all clean
