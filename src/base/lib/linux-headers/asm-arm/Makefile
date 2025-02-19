#
# Copyright (C) 2007, 2009, 2010, Strawberry Development Group.
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

default: interfaces

# For now, only ep93xx is supported.
ASMARCH=ep93xx
ASMARCH_DIR=arch-$(ASMARCH)

ASM_HEADERS=$(wildcard *.h)
ASMARCH_HEADERS=$(wildcard $(ASMARCH_DIR)/*.h)
ASMMACH_HEADERS=$(wildcard mach/*.h)

EROS_SRC=../../../..
CROSS_BUILD=yes

include $(EROS_SRC)/build/make/makevars.mk

include $(EROS_SRC)/build/make/makerules.mk

interfaces:
	$(INSTALL) -d $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/$(ASMARCH_DIR)
	$(INSTALL) -d $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/mach/
	$(INSTALL) -m 644 $(ASM_HEADERS) $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/
	$(INSTALL) -m 644 $(ASMARCH_HEADERS) $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/$(ASMARCH_DIR)/
	$(INSTALL) -m 644 $(ASMMACH_HEADERS) $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/mach/
# arm is the only target that uses asm/arch/, and we only support one
# architecture at a time, so the following kludge works:
	ln --symbolic --force $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/$(ASMARCH_DIR)/ $(EROS_ROOT)/include/linux-arch/$(LINUX2624_TARGET)/asm/arch
