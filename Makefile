all:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes all
	make -C dmarelay all
	make -C smap all
	make -C smaprpc all

clean:
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes clean
	make -C dmarelay clean
	make -C smap clean
	make -C smaprpc clean
