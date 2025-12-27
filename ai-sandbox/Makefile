CC	=	gcc
CFLAGS =	-Wall -Wextra -D_GNU_SOURCE
TARGET =	ai-sandbox
SRCS =	src/main.c src/namespace.c
OBJS =	$(SRCS:.c=.o)

all: $(TARGET)

$(TARGET):	$(OBJS)
	$(CC)	$(CFLAGS) -o $@ $^
	@echo ""
	@echo "✓ Build complete!"
	@echo "Run with: sudo ./$(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "✓ Clean complete"

.PHONY: all clean
