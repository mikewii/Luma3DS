ifneq ($(strip $(shell firmtool -v 2>&1 | grep usage)),)
$(error "Please install firmtool v1.1 or greater")
endif

TOPD 		?= 	$(CURDIR)
NAME		:=	$(notdir $(CURDIR))
REVISION	:=	$(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/')

SUBFOLDERS	:=	sysmodules arm11 arm9 k11_extension

IP 			:=  59
FTP_HOST 	:=	192.168.0.
FTP_PORT	:=	"5000"
FTP_PATH	:=	""

.PHONY:	all release clean $(SUBFOLDERS)

all:		boot.firm

release:	$(NAME)$(REVISION).zip

clean:
	@$(foreach dir, $(SUBFOLDERS), $(MAKE) -C $(dir) clean &&) true
	@rm -rf *.firm *.zip

re: clean all
	
send:
	@echo "Sending luma over FTP"
	@python $(TOPD)/send_ftp.py $(TOPD)/boot.firm $(FTP_PATH) "$(FTP_HOST)$(IP)" $(FTP_PORT)

$(NAME)$(REVISION).zip:	boot.firm exception_dump_parser
	@zip -r $@ $^ -x "*.DS_Store*" "*__MACOSX*"

boot.firm:	$(SUBFOLDERS)
	@firmtool build $@ -D sysmodules/sysmodules.bin arm11/arm11.elf arm9/arm9.elf k11_extension/k11_extension.elf \
	-A 0x18180000 -C XDMA XDMA NDMA XDMA
	@echo built... $(notdir $@)

$(SUBFOLDERS):
	@$(MAKE) -C $@ all
