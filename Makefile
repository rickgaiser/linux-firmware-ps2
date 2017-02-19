INSTALL_DIR = compiled_irx/ps2sdk/
LINUX_DIR   = ../linux/firmware/ps2/
PS2SDK_MODS = ps2dev9 freesd audsrv poweroff
PS2SDK_OBJS = $(addsuffix .irx, $(addprefix $(LINUX_DIR), $(PS2SDK_MODS)))


all:
	make -C dev9_dma all
	make -C eedebug all
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no all
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes all
	make -C pata_ps2 all
	make -C smaprpc all

clean:
	make -C dev9_dma clean
	make -C eedebug clean
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no clean
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes clean
	make -C pata_ps2 clean
	make -C smaprpc clean
	rm -f $(LINUX_DIR)*.irx

$(LINUX_DIR)%.irx: $(INSTALL_DIR)%.irx
	cp $< $@

install: $(PS2SDK_OBJS)
	make -C dev9_dma install
	make -C eedebug install
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=no install
	make -C intrelay DEV9_SUPPORT=no RPC_IRQ_SUPPORT=yes install
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=no install
	make -C intrelay DEV9_SUPPORT=yes RPC_IRQ_SUPPORT=yes install
	make -C pata_ps2 install
	make -C smaprpc install
