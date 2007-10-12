/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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
#include <eros/Invoke.h>
#include <domain/ConstructorKey.h>
#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <idl/capros/GPT.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>

#define KR_OSTREAM KR_APP(0)
#define KR_ZSF     KR_APP(1)
#define KR_SEG     KR_APP(2)
#define KR_SCRATCH0 KR_TEMP0
#define KR_SCRATCH1 KR_TEMP1

#define dbg_init    0x1
#define dbg_test    0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define TEST_ADDR  0x400000

/* This is truly sleazy -- it turns into one of:

   (kprintf) (args... )
   (kdprintf) (args... )

   according to the test result */

#define KPRINTF(x) ( (dbg_##x & dbg_flags) ? kdprintf : kprintf )

const uint32_t __rt_stack_pointer = 0x10000;
/* Flag as unkept so bootstrap code doesn't clobber KR_APP(0). */
const uint32_t __rt_unkept = 1;

void
setup()
{
  uint32_t result;
  
  KPRINTF(init)(KR_OSTREAM, "About to buy new root seg:\n");

  capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_SCRATCH0);

  KPRINTF(init)(KR_OSTREAM, "Set l2v:\n");

  capros_GPT_setL2v(KR_SCRATCH0, 22);
  
  KPRINTF(init)(KR_OSTREAM, "Fetch current space:\n");

  capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH1);

  KPRINTF(init)(KR_OSTREAM, "Insert it in new node:\n");

  capros_GPT_setSlot(KR_SCRATCH0, 0x0, KR_SCRATCH1);

  KPRINTF(init)(KR_OSTREAM, "Build new zero segment:\n");

  result = constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_SEG);

  KPRINTF(init)(KR_OSTREAM,
	   "result: 0x%08x. Insert result in new seg node:\n",
	   result); 

  capros_GPT_setSlot(KR_SCRATCH0, 1, KR_SEG);

  KPRINTF(init)(KR_OSTREAM, "Make new thing be my address space:\n");

  capros_Process_swapAddrSpace(KR_SELF, KR_SCRATCH0, KR_VOID);
}

int
main()
{
  uint32_t value;
  uint32_t addr = TEST_ADDR;
  
  setup();
  
  KPRINTF(test)(KR_OSTREAM, "About to read word from VCS:\n");

  value = *((uint32_t *) addr);
  
  KPRINTF(test)(KR_OSTREAM, "Result was 0x%x\n", value);

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 1;
  
  KPRINTF(test)(KR_OSTREAM, "Reread word from VCS:\n");
  
  value = *((uint32_t *) TEST_ADDR);
  
  KPRINTF(test)(KR_OSTREAM, "New value is: 0x%08x\n", value);


  addr = TEST_ADDR + EROS_PAGE_SIZE;

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 2;
  
  KPRINTF(test)(KR_OSTREAM, "Reread word from VCS:\n");

  value = *((uint32_t *) (TEST_ADDR + EROS_PAGE_SIZE));
  
  KPRINTF(test)(KR_OSTREAM, "New value is: 0x%08x\n", value);


  /* following tests traversal suppression */
  addr = TEST_ADDR + 2 * EROS_PAGE_SIZE;

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 3;
  
  KPRINTF(test)(KR_OSTREAM, "Reread word from VCS:\n");

  value = *((uint32_t *) (TEST_ADDR + 2 * EROS_PAGE_SIZE));
  
  KPRINTF(test)(KR_OSTREAM, "New value is: 0x%08x\n", value);


  addr = TEST_ADDR + (EROS_PAGE_SIZE * EROS_NODE_SIZE);


#if 1 
  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr);

  *((uint32_t *) addr) = 4;
  
  KPRINTF(test)(KR_OSTREAM, "Reread word from VCS:\n");

  value = *((uint32_t *) addr);
  
  KPRINTF(test)(KR_OSTREAM, "New value is: 0x%08x\n", value);
#endif
  

  KPRINTF(test)(KR_OSTREAM, "About to destroy VCS: 0x%08x\n", value);
  value = capros_key_destroy(KR_SEG);
  KPRINTF(test)(KR_OSTREAM, "Return code is: 0x%08x\n", value);

  KPRINTF(test)(KR_OSTREAM, "Test PASSED\n");
  return 0;
}
