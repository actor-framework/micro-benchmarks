.PHONY: all run

SHELL=/bin/bash

all:
	@for a in $$(ls build); do \
		if [ -f build/$$a/build.ninja ]; then \
			ninja -C build/$$a || exit 1; \
		elif [ -f build/$$a/Makefile ]; then \
			$(MAKE) -C build/$$a || exit 1; \
		fi; \
	done

run: all
	@for a in $$(ls build); do \
		$$(pwd)/build/$$a/micro-benchmark; \
	done
