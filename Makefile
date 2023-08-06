SUBDIRS := code

all: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

# Define PATH to install
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

install:
	install -d $(BINDIR) || true
	install code/userprog/nachos $(BINDIR)/nachos
	install code/threads/nachos $(BINDIR)/nachos-threads

clean:
	@set -e; \
	for i in $(SUBDIRS); do \
		$(MAKE) -C $$i $@; \
	done

.PHONY: all clean $(SUBDIRS)
