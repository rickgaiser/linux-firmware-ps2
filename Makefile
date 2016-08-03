all:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes all
	make -C dmarelay all
	make -C smaprpc all
	make -C eedebug all

clean:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes clean
	make -C dmarelay clean
	make -C smaprpc clean
	make -C eedebug clean
	rm -f compiled_irx/*.irx

install:
	cp intrelay/bin/intrelay-dev9.irx       compiled_irx/
	cp intrelay/bin/intrelay-dev9-rpc.irx   compiled_irx/
	cp intrelay/bin/intrelay-direct.irx     compiled_irx/
	cp intrelay/bin/intrelay-direct-rpc.irx compiled_irx/
	cp dmarelay/bin/dmarelay.irx            compiled_irx/
	cp smaprpc/smaprpc.irx                  compiled_irx/
	cp eedebug/eedebug.irx                  compiled_irx/
	cp compiled_irx/*.irx			../linux/firmware/ps2/
	cp compiled_irx/ps2sdk/*.irx		../linux/firmware/ps2/

