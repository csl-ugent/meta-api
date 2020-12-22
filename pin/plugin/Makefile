PIN=../docker/build/pin-3.16-98275-ge0db48c31-gcc-linux

%.o : %.cpp
	g++ -Wall -Werror -Wno-error=unused-variable -Wno-unknown-pragmas -D__PIN__=1 -DPIN_CRT=1 -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -DTARGET_IA32E -DHOST_IA32E -fPIC -DTARGET_LINUX -fabi-version=2 -faligned-new  -I${PIN}/source/include/pin -I${PIN}/source/include/pin/gen -isystem ${PIN}/extras/stlport/include -isystem ${PIN}/extras/libstdc++/include -isystem ${PIN}/extras/crt/include -isystem ${PIN}/extras/crt/include/arch-x86_64 -isystem ${PIN}/extras/crt/include/kernel/uapi -isystem ${PIN}/extras/crt/include/kernel/uapi/asm-x86 -I${PIN}/extras/components/include -I${PIN}/extras/xed-intel64/include/xed -I${PIN}/source/tools/Utils -I${PIN}/source/tools/InstLib -O3 -fomit-frame-pointer -fno-strict-aliasing -c -o $@ -g $<

%.so : common.o %.o
	g++ -shared -Wl,--hash-style=sysv ${PIN}/intel64/runtime/pincrt/crtbeginS.o -Wl,-Bsymbolic -Wl,--version-script=${PIN}/source/include/pin/pintool.ver -fabi-version=2 -o $@ $^ -L${PIN}/intel64/runtime/pincrt -L${PIN}/intel64/lib -L${PIN}/intel64/lib-ext -L${PIN}/extras/xed-intel64/lib -lpin -lxed ${PIN}/intel64/runtime/pincrt/crtendS.o -lpin3dwarf  -ldl-dynamic -nostdlib -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic -g

.PHONY: branch
branch: cond_branch.so

.PHONY: pairs
pairs: pairs.so

# /build/pin-3.16-98275-ge0db48c31-gcc-linux/pin -appdebug -t /data/BOEK/evaluation/metaapi/pin/plugin/cond_branch.so -- /usr/bin/evince > log
