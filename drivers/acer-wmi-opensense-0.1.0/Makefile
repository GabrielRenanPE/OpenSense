# SPDX-License-Identifier: GPL-2.0-or-later
KDIR ?= /lib/modules/$(shell uname -r)/build
LLVM_FLAGS := $(shell grep -q '^CONFIG_CC_IS_CLANG=y' "$(KDIR)/.config" && echo LLVM=1)
.PHONY: all clean
all:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" $(LLVM_FLAGS) modules
clean:
	$(MAKE) -C "$(KDIR)" M="$(CURDIR)" clean
