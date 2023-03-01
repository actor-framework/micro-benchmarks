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

run-json: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark --benchmark_filter=json; \
	done

run-actors: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark --benchmark_filter=actors; \
	done

run-pattern-matching: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark --benchmark_filter=pattern_matching; \
	done

run-serialization: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark --benchmark_filter=serialization; \
	done

run-socket-communication: all
	@for a in $$(find build -maxdepth 3 -name 'CMakeCache.txt' -exec dirname {} \; | sort); do \
		$$(pwd)/$$a/micro-benchmark --benchmark_filter=socket_communication; \
	done
