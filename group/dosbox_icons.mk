ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)\epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)\epoc32\data\z
endif
# -------------------------------------------------------------------
# TODO: Configure these
# -------------------------------------------------------------------
TARGETDIR=$(ZDIR)\RESOURCE\apps
#ICONTARGETFILENAME=$(TARGETDIR)\dosbox_aif.mif
ICONTARGETFILENAME=$(TARGETDIR)\dosbox_aif.mbm

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing


RESOURCE :
	mifconv $(ICONTARGETFILENAME) \
		 /c24,8 dosbox.bmp
#RESOURCE :
#	mifconv $(ICONTARGETFILENAME) \
#		/c8,8 dosbox.svg
		
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing
