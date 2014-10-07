ROMDIR = $(DATDIR)/test-romfs-neo
DAT += $(OUTDIR)/$(DATDIR)/test-romfs-neo.o

$(OUTDIR)/$(ROMDIR).o: $(OUTDIR)/$(ROMDIR).bin
	@mkdir -p $(dir $@)
	@echo "    OBJCOPY "$@
	@$(CROSS_COMPILE)objcopy -I binary -O elf32-littlearm -B arm \
		--prefix-sections '.romfs-neo' $< $@

$(OUTDIR)/$(ROMDIR).bin: $(ROMDIR) $(OUTDIR)/$(TOOLDIR)/mkromfs-neo
	@mkdir -p $(dir $@)
	@echo "    MKROMFS_NEO "$@
	@$(OUTDIR)/$(TOOLDIR)/mkromfs-neo -d $< $@

$(ROMDIR):
	@mkdir -p $@

$(OUTDIR)/%/mkromfs-neo: %/mkromfs-neo.c
	@mkdir -p $(dir $@)
	@echo "    CC      "$@
	@gcc -Wall -o $@ $^
