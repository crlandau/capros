#
# OpenSSL/crypto/blowfish/Makefile
#

DIR=	bf
TOP=	../..
CC=	cc
CPP=	$(CC) -E
INCLUDES=
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

BF_ENC=		bf_enc.o
# or use
#DES_ENC=	bx86-elf.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=bftest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=bf_skey.c bf_ecb.c bf_enc.c bf_cfb64.c bf_ofb64.c 
LIBOBJ=bf_skey.o bf_ecb.o $(BF_ENC) bf_cfb64.o bf_ofb64.o

SRC= $(LIBSRC)

EXHEADER= blowfish.h
HEADER=	bf_pi.h bf_locl.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
bx86-elf.s: asm/bf-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) bf-586.pl elf $(CFLAGS) $(PROCESSOR) > ../$@)
# COFF
bx86-cof.s: asm/bf-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) bf-586.pl coff $(CFLAGS) $(PROCESSOR) > ../$@)
# a.out
bx86-out.s: asm/bf-586.pl ../perlasm/x86asm.pl ../perlasm/cbc.pl
	(cd asm; $(PERL) bf-586.pl a.out $(CFLAGS) $(PROCESSOR) > ../$@)

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

# We need to use force because 'install' matches 'INSTALL' on case
# insensitive systems
FRC.install:
install: FRC.install
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist ; \
	do  \
	(cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done;

tags:
	ctags $(SRC)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.s *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.
