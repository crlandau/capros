/*
 * Copyright (C) 2007, Strawberry Development Group
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/arch/arm/IRQSWI.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  9

/* It is intended that this should be a small space process. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1; /* do not mess with keeper */

#define MASK_CPSR_IRQDisable 0x80
int
main(void)
{
  uint32_t flags1, flags2;

  kprintf(KR_OSTREAM, "IOPL: About to issue IO instruction\n");

  flags1 = capros_irq_disable();
  assert(! (flags1 & MASK_CPSR_IRQDisable));

  flags2 = capros_irq_disable();	// just to read the CPSR
  assert(flags2 & MASK_CPSR_IRQDisable);

  kprintf(KR_OSTREAM, "IOPL: While IRQ disabled\n");

  // IRQ still disabled after call to KR_OSTREAM?
  flags2 = capros_irq_disable();	// just to read the CPSR
  assert(flags2 & MASK_CPSR_IRQDisable);
  
  capros_irq_put(flags1);

  flags2 = capros_irq_disable();	// just to read the CPSR
  assert(! (flags2 & MASK_CPSR_IRQDisable));

  capros_irq_enable();

  kprintf(KR_OSTREAM, "IOPL: Success\n");

  return 0;
}
