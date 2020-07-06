OUTPUT_ARCH(i386:x86-64)
TARGET(elf64-x86-64)
EXTERN(__export_parasite_head_start)

SECTIONS
{
	.text : {
		*(.head.text)
		ASSERT(DEFINED(__export_parasite_head_start),
			"Symbol __export_parasite_head_start is missing");
		*(.text*)
		*(.compel.exit)
		*(.compel.init)
	}

	.data : {
		*(.data*)
		*(.bss*)
	}

	.rodata : {
		*(.rodata*)
	}

  .got : {
. = ALIGN(8);
__export_parasite_got = .;
. = . + 8*100; /* 100 entries for GOT table */
__export_parasite_got_end = .;
  }

	.toc : ALIGN(8) {
		*(.toc*)
	}

	/DISCARD/ : {
		*(.debug*)
		*(.comment*)
		*(.note*)
		*(.group*)
		*(.eh_frame*)
	}

/* Parasite args should have 4 bytes align, as we have futex inside. */
. = ALIGN(4);
__export_parasite_args = .;
}
