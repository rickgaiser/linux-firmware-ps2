all:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes all
	make -C dmarelay all
	make -C smaprpc all

clean:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes clean
	make -C dmarelay clean
	make -C smaprpc clean

install:
	cp intrelay/bin/intrelay-dev9.irx       ../linux/firmware/ps2/
	cp intrelay/bin/intrelay-dev9-rpc.irx   ../linux/firmware/ps2/
	cp intrelay/bin/intrelay-direct.irx     ../linux/firmware/ps2/
	cp intrelay/bin/intrelay-direct-rpc.irx ../linux/firmware/ps2/
	cp dmarelay/bin/dmarelay.irx            ../linux/firmware/ps2/
	cp smaprpc/smaprpc.irx                  ../linux/firmware/ps2/
