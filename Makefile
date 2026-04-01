CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -pedantic -std=c11 -O2
CFLAGS += -Iinclude -Isrc

PREFIX  ?= /usr/local
CONFDIR ?= /etc/vigil
LOGDIR  ?= /var/log/vigil

# Sources
VIGILD_SRC  = $(wildcard src/vigild/*.c)
VIGILCTL_SRC = $(wildcard src/vigilctl/*.c)
TEST_SRC    = $(wildcard tests/*.c)

# Objects
VIGILD_OBJ  = $(VIGILD_SRC:.c=.o)
VIGILCTL_OBJ = $(VIGILCTL_SRC:.c=.o)

.PHONY: all clean install uninstall test

all: vigild vigilctl

vigild: $(VIGILD_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

vigilctl: $(VIGILCTL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Library sources needed by tests (everything except main.c to avoid duplicate main)
TEST_LIB_SRC = $(filter-out src/vigild/main.c, $(VIGILD_SRC))

test: $(TEST_SRC)
	@for t in $(TEST_SRC); do \
		echo "=== $$t ==="; \
		$(CC) $(CFLAGS) -o test_bin $$t $(TEST_LIB_SRC) -Iinclude -Isrc && ./test_bin; \
		rm -f test_bin; \
	done

install: vigild vigilctl
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 vigild $(DESTDIR)$(PREFIX)/bin/
	install -m 755 vigilctl $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(CONFDIR)
	install -m 644 vigil.toml.example $(DESTDIR)$(CONFDIR)/vigil.toml
	install -d $(DESTDIR)$(LOGDIR)
	install -m 644 vigil.service $(DESTDIR)/etc/systemd/system/
	@echo "Run: systemctl daemon-reload && systemctl enable vigil"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/vigild
	rm -f $(DESTDIR)$(PREFIX)/bin/vigilctl
	rm -f $(DESTDIR)/etc/systemd/system/vigil.service
	@echo "Config and logs preserved in $(CONFDIR) and $(LOGDIR)"

clean:
	rm -f vigild vigilctl $(VIGILD_OBJ) $(VIGILCTL_OBJ) test_bin
