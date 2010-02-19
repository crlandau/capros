/*
 * Copyright (C) 2010, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linuxk/lsync.h>
#include <idl/capros/PCIDriverConstructorExtended.h>

#define KR_PCIDrvr_PCIDev KR_APP2(0)
// KR_PCIAPP(0) is first available cap register for driver code
#define KR_PCIAPP(n)      KR_APP2(1+(n))

#ifndef __ASSEMBLER__

extern struct pci_dev thePCIDev;
extern capros_PCIDriverConstructorExtended_NewDeviceData theNdd;

int PCIDriver_mainInit(const char * devName);

#endif // __ASSEMBLER__
