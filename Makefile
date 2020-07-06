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

all: run_as_exe

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

# musl libc
#

MUSL_VERSION ?= 1.2.0
MUSL_ARCHIVE ?= musl-$(MUSL_VERSION).tar.gz

$(MUSL_ARCHIVE):
	wget https://www.musl-libc.org/releases/$@

musl/ldso/dynlink.c: | musl/lib/libc.a;

musl: $(MUSL_ARCHIVE)
	rm -rf musl.tmp
	mkdir musl.tmp
	tar xzf $(MUSL_ARCHIVE) -C musl.tmp --strip-components=1
	mv musl.tmp musl

musl/lib/libc.a: musl
	cd musl && env -u CFLAGS -u AFLAGS -u HOSTCFLAGS ./configure --disable-shared
	env -u CFLAGS -u AFLAGS -u HOSTCFLAGS ${MAKE} -C musl

# run_as_exe + parasite
#

CFLAGS_PARASITE += -Imusl/arch/x86_64 \
	-Imusl/arch/generic \
	-Imusl/obj/src/internal \
	-Imusl/src/include \
	-Imusl/src/internal \
	-Imusl/obj/include \
	-Imusl/include

LDFLAGS_PARASITE += --exclude-libs ALL

run_as_exe: run_as_exe.c parasite.h compel/libcompel.a | $(COMPEL_BIN)
	$(CC) $(CFLAGS) $(shell $(COMPEL) includes) -o $@ $< $(shell $(COMPEL) --static libs)

parasite.h: parasite.po
	$(COMPEL) hgen -o $@ -f $<

parasite.po: parasite.o musl/lib/libc.a | $(COMPEL_BIN)
	ld $(LDFLAGS_PARASITE) $(shell $(COMPEL) ldflags) -o $@ $^ $(shell $(COMPEL) plugins)

parasite.o: parasite.c | $(COMPEL_BIN) musl/ldso/dynlink.c
	$(CC) $(CFLAGS_PARASITE) $(CFLAGS) -c $(shell $(COMPEL) cflags) -Wno-strict-prototypes -o $@ $^

clean:
	rm -f run_as_exe parasite.h parasite.po parasite.o
