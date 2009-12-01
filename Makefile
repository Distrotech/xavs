# Makefile

include config.mak

all: default

SRCS = common/mc.c common/predict.c common/pixel.c common/macroblock.c \
       common/frame.c common/dct.c common/cpu.c common/cabac.c \
       common/common.c common/mdate.c common/set.c \
       common/quant.c common/vlc.c \
       encoder/analyse.c encoder/me.c encoder/ratecontrol.c \
       encoder/set.c encoder/macroblock.c encoder/cabac.c \
       encoder/cavlc.c encoder/vlctest.c encoder/encoder.c

SRCCLI = xavs.c matroska.c muxers.c

# Visualization sources
ifeq ($(VIS),yes)
SRCS   += common/visualize.c common/display-x11.c
endif

# MMX/SSE optims
ifneq ($(AS),)
X86SRC0 = cabac-a.asm dct-a.asm deblock-a.asm mc-a.asm mc-a2.asm \
          pixel-a.asm predict-a.asm quant-a.asm sad-a.asm \
          cpu-a.asm dct-32.asm
X86SRC = $(X86SRC0:%=common/x86/%)

ifeq ($(ARCH),X86)
ARCH_X86 = yes
ASMSRC   = $(X86SRC) common/x86/pixel-32.asm
endif

ifeq ($(ARCH),X86_64)
ARCH_X86 = yes
ASMSRC   = $(X86SRC:-32.asm=-64.asm)
ASFLAGS += -DARCH_X86_64
endif

ifdef ARCH_X86
ASFLAGS += -Icommon/x86/
SRCS   += common/x86/mc-c.c common/x86/predict-c.c
OBJASM  = $(ASMSRC:%.asm=%.o)
$(OBJASM): common/x86/x86inc.asm common/x86/x86util.asm
checkasm: tools/checkasm-a.o
endif
endif

# AltiVec optims
ifeq ($(ARCH),PPC)
ALTIVECSRC += common/ppc/mc.c common/ppc/pixel.c common/ppc/dct.c \
              common/ppc/quant.c common/ppc/deblock.c \
              common/ppc/predict.c
SRCS += $(ALTIVECSRC)
$(ALTIVECSRC:%.c=%.o): CFLAGS += $(ALTIVECFLAGS)
endif

# VIS optims
ifeq ($(ARCH),UltraSparc)
ASMSRC += common/sparc/pixel.asm
OBJASM  = $(ASMSRC:%.asm=%.o)
endif

ifneq ($(HAVE_GETOPT_LONG),1)
SRCS += extras/getopt.c
endif

OBJS = $(SRCS:%.c=%.o)
OBJCLI = $(SRCCLI:%.c=%.o)
DEP  = depend

.PHONY: all default fprofiled clean distclean install uninstall dox test testclean

default: $(DEP) xavs$(EXE)

libxavs.a: .depend $(OBJS) $(OBJASM)
	$(AR) rc libxavs.a $(OBJS) $(OBJASM)
	$(RANLIB) libxavs.a

$(SONAME): .depend $(OBJS) $(OBJASM)
	$(CC) -shared -o $@ $(OBJS) $(OBJASM) $(SOFLAGS) $(LDFLAGS)

xavs$(EXE): $(OBJCLI) libxavs.a 
	$(CC) -o $@ $+ $(LDFLAGS)

checkasm: tools/checkasm.o libxavs.a
	$(CC) -o $@ $+ $(LDFLAGS)

%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<
# delete local/anonymous symbols, so they don't show up in oprofile
	-@ $(STRIP) -x $@

.depend: config.mak
	rm -f .depend
	$(foreach SRC, $(SRCS) $(SRCCLI), $(CC) $(CFLAGS) $(ALTIVECFLAGS) $(SRC) -MT $(SRC:%.c=%.o) -MM -g0 1>> .depend;)

config.mak:
	./configure

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

SRC2 = $(SRCS) $(SRCCLI)
# These should cover most of the important codepaths
OPT0 = --crf 30 -b1 -m1 -r1 --me dia --no-cabac --direct temporal --ssim --no-weightb
OPT1 = --crf 16 -b2 -m3 -r3 --me hex --no-8x8dct --direct spatial --no-dct-decimate -t0
OPT2 = --crf 26 -b4 -m5 -r2 --me hex --cqm jvt --nr 100 --psnr --no-mixed-refs --b-adapt 2
OPT3 = --crf 18 -b3 -m9 -r5 --me umh -t1 -A all --b-pyramid --direct auto --no-fast-pskip
OPT4 = --crf 22 -b3 -m7 -r4 --me esa -t2 -A all --psy-rd 1.0:1.0
OPT5 = --frames 50 --crf 24 -b3 -m10 -r3 --me tesa -t2
OPT6 = --frames 50 -q0 -m9 -r2 --me hex -Aall
OPT7 = --frames 50 -q0 -m2 -r1 --me hex --no-cabac

ifeq (,$(VIDS))
fprofiled:
	@echo 'usage: make fprofiled VIDS="infile1 infile2 ..."'
	@echo 'where infiles are anything that xavs understands,'
	@echo 'i.e. YUV with resolution in the filename, y4m, or avisynth.'
else
fprofiled:
	$(MAKE) clean
	mv config.mak config.mak2
	sed -e 's/CFLAGS.*/& -fprofile-generate/; s/LDFLAGS.*/& -fprofile-generate/' config.mak2 > config.mak
	$(MAKE) xavs$(EXE)
	$(foreach V, $(VIDS), $(foreach I, 0 1 2 3 4 5 6 7, ./xavs$(EXE) $(OPT$I) --threads 1 $(V) -o $(DEVNULL) ;))
	rm -f $(SRC2:%.c=%.o)
	sed -e 's/CFLAGS.*/& -fprofile-use/; s/LDFLAGS.*/& -fprofile-use/' config.mak2 > config.mak
	$(MAKE)
	rm -f $(SRC2:%.c=%.gcda) $(SRC2:%.c=%.gcno)
	mv config.mak2 config.mak
endif

clean:
	rm -f $(OBJS) $(OBJASM) $(OBJCLI) $(SONAME) *.a xavs xavs.exe .depend TAGS
	rm -f checkasm checkasm.exe tools/checkasm.o tools/checkasm-a.o
	rm -f $(SRC2:%.c=%.gcda) $(SRC2:%.c=%.gcno)
	- sed -e 's/ *-fprofile-\(generate\|use\)//g' config.mak > config.mak2 && mv config.mak2 config.mak
	rm -f ./*~ 
	rm -f encoder/*~ 
	rm -f common/*~ 

distclean: clean
	rm -f config.mak config.h xavs.pc
	rm -rf test/

install: xavs$(EXE) $(SONAME)
	install -d $(DESTDIR)$(bindir) $(DESTDIR)$(includedir)
	install -d $(DESTDIR)$(libdir) $(DESTDIR)$(libdir)/pkgconfig
	install -m 644 xavs.h $(DESTDIR)$(includedir)
	install -m 644 libxavs.a $(DESTDIR)$(libdir)
	install -m 644 xavs.pc $(DESTDIR)$(libdir)/pkgconfig
	install xavs$(EXE) $(DESTDIR)$(bindir)
	$(RANLIB) $(DESTDIR)$(libdir)/libxavs.a
ifeq ($(SYS),MINGW)
	$(if $(SONAME), install -m 755 $(SONAME) $(DESTDIR)$(bindir))
else
	$(if $(SONAME), ln -sf $(SONAME) $(DESTDIR)$(libdir)/libxavs.$(SOSUFFIX))
	$(if $(SONAME), install -m 755 $(SONAME) $(DESTDIR)$(libdir))
endif
	$(if $(IMPLIBNAME), install -m 644 $(IMPLIBNAME) $(DESTDIR)$(libdir))

uninstall:
	rm -f $(DESTDIR)$(includedir)/xavs.h $(DESTDIR)$(libdir)/libxavs.a
	rm -f $(DESTDIR)$(bindir)/xavs $(DESTDIR)$(libdir)/pkgconfig/xavs.pc
	$(if $(SONAME), rm -f $(DESTDIR)$(libdir)/$(SONAME) $(DESTDIR)$(libdir)/libxavs.$(SOSUFFIX))

etags: TAGS

TAGS:
	etags $(SRCS)

dox:
	doxygen Doxyfile

ifeq (,$(VIDS))
test:
	@echo 'usage: make test VIDS="infile1 infile2 ..."'
	@echo 'where infiles are anything that xavs understands,'
	@echo 'i.e. YUV with resolution in the filename, y4m, or avisynth.'
else
test:
	perl tools/regression-test.pl --version=head,current --options='$(OPT0)' --options='$(OPT1)' --options='$(OPT2)' $(VIDS:%=--input=%)
endif

testclean:
	rm -f test/*.log test/*.avs
	$(foreach DIR, $(wildcard test/xavs-r*/), cd $(DIR) ; make clean ; cd ../.. ;)
