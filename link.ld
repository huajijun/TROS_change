OUTPUT_ARCH("riscv")
ENTRY(_stage0)

_STACK_SIZE = DEFINED(_STACK_SIZE) ? _STACK_SIZE : 0x1000;

SECTIONS
{
	. = /* XIP address*/
	.text : {*(.text)}

	.data : {
	_gp = . + 0x800;
		*(.data)
	}

	.stack : {
       _stack_end = .;
       . += _STACK_SIZE;
       . = ALIGN(4);
       _stack = .;
       __stack = _stack;
    }

	start_addr = .
	.payload : { *(.payload)	}
	end_addr = .
}