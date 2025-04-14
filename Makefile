# Top level makefile, the real shit is at src/Makefile

default: all

# $@代表的就是目标的意思，$(MAKE)代表的就是make
.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

.PHONY: install
