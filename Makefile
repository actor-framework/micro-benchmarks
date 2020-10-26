.PHONY: all run

SHELL=/bin/bash

all:
	@for a in $$(find build -maxdepth 3 -name 'build.ninja' -exec dirname {} \; | sort); do \
		ninja -C $$a || exit 1; \
	done;
	@for a in $$(find build -maxdepth 3 -name 'Makefile' -exec dirname {} \; | sort); do \
		$(MAKE) -C $$a || exit 1; \
	done

run: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark; \
	done
