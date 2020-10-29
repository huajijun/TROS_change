all:
	riscv64-ls-elf-gcc -nostartfiles -nostdlib stage0.S main.c heap2.c  common.c list2.c para.S  queue2.c  task.c time.c -Tlink.ld -o test
spike: all
	spike -d -m0x10000000:0x10000000 test

dump: all
	riscv64-ls-elf-objdump -x -d test > test.dmp
	riscv64-ls-elf-objdump -j .data -s test >> test.dmp
other:
	#riscv64-ls-elf-gcc -nostartfiles -nostdlib assem.S test.c -L../lib -lsbi -Tass.lds -o test
	dtc -I dts -O dtb -o output.dts  ./platform/kendryte/k210/k210.dts
	fdtdump -sd output.dts
test: prac.S vma.lds
	riscv64-ls-elf-gcc -nostartfiles -nostdlib   -Tvma.lds paddr.c physical_alloc.c prac.S -o test
clean:
	rm test -rf
