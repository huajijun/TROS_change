#ifdef __riscv_xlen
#define  STORE sd
#define LOAD    ld
#define REGBYTES    8
#else
#define  STORE sw
#define LOAD    lw
#define REGBYTES    4
#endif
