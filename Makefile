# Compel
#
__nmk_dir=$(CURDIR)/nmk/scripts/
export __nmk_dir
include $(__nmk_dir)include.mk
include $(__nmk_dir)macro.mk
ARCH=x86

include Makefile.versions

LDARCH		:= i386:x86-64
DEFINES		:= -DCONFIG_X86_64
DEFINES			+= -D_FILE_OFFSET_BITS=64
DEFINES			+= -D_GNU_SOURCE

CFLAGS_PIE		+= -DCR_NOGLIBC
export CFLAGS_PIE

LDARCH ?= $(ARCH)
export LDARCH
export PROTOUFIX DEFINES

AFLAGS			+= -D__ASSEMBLY__
CFLAGS			+= $(USERCFLAGS) $(WARNINGS) $(DEFINES) -iquote include/ \
	-Werror=implicit-function-declaration \
	-Werror=implicit-int \
	-Werror=pointer-sign

HOSTCFLAGS		+= $(WARNINGS) $(DEFINES) -iquote include/
export AFLAGS CFLAGS HOSTCFLAGS

COMPEL		:= compel/compel-host
COMPEL_BIN		:= compel/compel-host-bin

all: $(COMPEL_BIN)

include Makefile.compel

compel-deps		+= compel/include/asm
compel-deps		+= $(COMPEL_VERSION_HEADER)
compel-deps		+= $(CONFIG_HEADER)
compel-deps		+= include/common/asm
compel-plugins		+= compel/plugins/std.lib.a compel/plugins/fds.lib.a

LIBCOMPEL_SO		:= libcompel.so
LIBCOMPEL_A		:= libcompel.a
export LIBCOMPEL_SO LIBCOMPEL_A

compel/%: $(compel-deps) $(compel-plugins) .FORCE
	$(Q) $(MAKE) $(build)=compel $@

include/common/asm: include/common/arch/$(ARCH)/asm
	$(call msg-gen, $@)
	$(Q) ln -s ./arch/$(ARCH)/asm $@