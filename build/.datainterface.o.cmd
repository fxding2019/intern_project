cmd_datainterface.o = gcc -Wp,-MD,./.datainterface.o.d.tmp  -m64 -pthread -I/home/chanawat/dpdk-20.05/lib/librte_eal/linux/include  -march=native -DRTE_MACHINE_CPUFLAG_SSE -DRTE_MACHINE_CPUFLAG_SSE2 -DRTE_MACHINE_CPUFLAG_SSE3 -DRTE_MACHINE_CPUFLAG_SSSE3 -DRTE_MACHINE_CPUFLAG_SSE4_1 -DRTE_MACHINE_CPUFLAG_SSE4_2 -DRTE_MACHINE_CPUFLAG_AES -DRTE_MACHINE_CPUFLAG_PCLMULQDQ -DRTE_MACHINE_CPUFLAG_AVX -DRTE_MACHINE_CPUFLAG_RDRAND -DRTE_MACHINE_CPUFLAG_RDSEED -DRTE_MACHINE_CPUFLAG_FSGSBASE -DRTE_MACHINE_CPUFLAG_F16C -DRTE_MACHINE_CPUFLAG_AVX2  -I/home/chanawat/intern_project/build/include -DRTE_USE_FUNCTION_VERSIONING -I/home/chanawat/dpdk-20.05/x86_x64-native-linux-gcc/include -include /home/chanawat/dpdk-20.05/x86_x64-native-linux-gcc/include/rte_config.h -D_GNU_SOURCE -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wpointer-arith -Wcast-align -Wnested-externs -Wcast-qual -Wformat-nonliteral -Wformat-security -Wundef -Wwrite-strings -Wdeprecated -Wno-missing-field-initializers -Wno-address-of-packed-member -std=c99 -DALLOW_EXPERIMENTAL_API   -O3 -g -Wfatal-errors -o datainterface.o -c /home/chanawat/intern_project/datainterface.c 