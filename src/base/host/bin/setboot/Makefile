#
# Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
#
# This file is part of the EROS Operating System.
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
#


# format utility for cross-formatting a volume.  THIS IS NOT A NATIVE TOOL
default: package

EROS_SRC=../../../..

include $(EROS_SRC)/build/make/makevars.mk

TARGETS=$(BUILDDIR)/setboot
OPTIM=-g
OBJECTS=$(BUILDDIR)/setboot.o
DEF+=   -D_REVEAL_KERNEL_KEY_TYPES_
INC=	-I$(EROS_ROOT)/host/include $(XENV_INCLUDE)
LIBS=	$(EROS_ROOT)/host/lib/liberos.a
DIRS=

include $(EROS_SRC)/build/make/makerules.mk

all: $(TARGETS)

$(BUILDDIR)/setboot: $(OBJECTS) $(LIBS)
	$(GPLUS) $(GPLUSFLAGS) -o $(BUILDDIR)/setboot $(OBJECTS) $(LIBS)

install: all
	$(INSTALL) -d $(EROS_ROOT)/host
	$(INSTALL) -d $(EROS_ROOT)/host/bin
	$(INSTALL) -m 755 $(TARGETS) $(EROS_ROOT)/host/bin

-include $(BUILDDIR)/.*.m
