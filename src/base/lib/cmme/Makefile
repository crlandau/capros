#
# Copyright (C) 2009, 2010, Strawberry Development Group.
#
# This file is part of the CapROS Operating System.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# This material is based upon work supported by the US Defense Advanced
# Research Projects Agency under Contract No. W31P4Q-07-C-0070.
# Approved for public release, distribution unlimited.

HEADERS=CMME.h CMMEMaps.h

EROS_SRC=../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

INC=-I$(BUILDDIR) -I$(EROS_ROOT)/include
OBJECTS=$(BUILDDIR)/cmme_stack_size.o \
  $(BUILDDIR)/cmmemaps.o

TARGETS=$(BUILDDIR)/libcmme.a
TARGETS+=$(BUILDDIR)/dyncmmeprotospace
TARGETS+=$(BUILDDIR)/dyncmmestart.o

DYNOBJECTS=$(CAPROS_DOMAIN)/interpreter.o
DYNOBJECTS+=$(BUILDDIR)/dyncmmeprotospace.o

include $(EROS_SRC)/build/make/makerules.mk

$(BUILDDIR)/libcmme.a: $(OBJECTS) $(MAKE_BUILDDIR)
	$(RM) $@
	$(AR) -cr $@ $(OBJECTS)
	$(RANLIB) $@

$(BUILDDIR)/dyncmmeprotospace: $(DYNOBJECTS)
	$(TARGET_LD) -N -Ttext 0 -static -e _start $(DYNOBJECTS) -o $@

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/include/domain
	$(INSTALL) -m 644 $(HEADERS) $(EROS_ROOT)/include/domain
	$(INSTALL) -d $(CAPROS_DOMAIN)
	$(INSTALL) -m 644 dyncmme.map $(CAPROS_DOMAIN)

install: $(TARGETS)
	$(INSTALL) -m 0644 $(TARGETS) $(CAPROS_DOMAIN)

-include $(BUILDDIR)/.*.m
