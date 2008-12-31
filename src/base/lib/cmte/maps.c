/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Procedures for the maps area of our address space.

We implement a modified buddy system to allocate the address space
at LK_MAPS_BASE.
We allocate only whole pages.

The address of a block of size s is always a multiple of 2**(ceiling(log2(s))).
*/

#include <eros/target.h>
#include <eros/container_of.h>
#include <eros/Invoke.h>	// get RC_OK
#include <domain/cmtesync.h>
#include <domain/CMTESemaphore.h>
#include <string.h>
#include <eros/Link.h>
#include <eros/fls.h>
#include <eros/ffs.h>
#include <idl/capros/Void.h>
#include <idl/capros/Node.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <domain/assert.h>

#define logMaxBlockSize 7 // in pages

// numpages is the number of pages we could allocate. It can be as large as
// ((LK_DATA_BASE - LK_MAPS_BASE) >> EROS_LGPAGE_SIZE)
// but to save space in mpages[], use a more reasonable maximum:
#define numpages 128

#define dbg_alloc 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct mpage {
  Link listLink;	// valid if isFree
  unsigned short kval;	// valid if isFree
  bool isFree;
} mpages[numpages];

// avail_list[n] is for blocks of size 2**n pages.
Link avail_list[logMaxBlockSize + 1];

static CMTEMutex_DECLARE_Unlocked(mapsLock);

static void
liberateOneBlock(unsigned long jAddr, unsigned int j)
{
  DEBUG(alloc) kprintf(KR_OSTREAM, "maps liberating 2**%d at 0x%x\n", j, jAddr);
  struct mpage * jmp = &mpages[jAddr];
  jmp->isFree = true;
  jmp->kval = j;
  link_insertAfter(&avail_list[j], &jmp->listLink);
}

static void
maps_liberate_locked(unsigned long pageAddr, unsigned long numPages)
{
  do {
    DEBUG(alloc) kprintf(KR_OSTREAM,
                         "maps liberating %d at 0x%x\n", numPages, pageAddr);
    // Free the smallest block at the end.
    unsigned int j = ffs32(numPages);
    if (j > logMaxBlockSize) {
      j = logMaxBlockSize;
    }
    unsigned long jSize = 1UL << j;
    unsigned long jAddr = pageAddr + numPages - jSize;

    assert(! mpages[jAddr].isFree);

    // Buddy system liberation per Knuth v.1 sec.2.5
recheck:
    if (j < logMaxBlockSize) {
      unsigned long pAddr = jAddr ^ jSize;	// potential buddy
      if (pAddr < numpages) {
        struct mpage * pmp = &mpages[pAddr];
        if (pmp->isFree
            && pmp->kval == j) {
          // Merge with buddy.
          link_UnlinkUnsafe(&pmp->listLink);	// remove from free list
          j++;
          if (pAddr < jAddr)
            jAddr = pAddr;
          goto recheck;
        }
      }
    }
    // No buddy to merge with.
    assert(numPages >= jSize);
    liberateOneBlock(jAddr, j);

    numPages -= jSize;
  } while (numPages);
}

void *
maps_pgOffsetToAddr(unsigned long pageAddr)
{
  return (void *) ((pageAddr << EROS_PAGE_LGSIZE) + LK_MAPS_BASE);
}

unsigned long
maps_addrToPgOffset(unsigned long addr)
{
  return (addr - LK_MAPS_BASE) >> EROS_PAGE_LGSIZE;
}

void
maps_liberate(unsigned long pgOffset, unsigned long numPages)
{
  CMTEMutex_lock(&mapsLock);

  maps_liberate_locked(pgOffset, numPages);

  CMTEMutex_unlock(&mapsLock);
}

// Returns page offset within maps area, or -1 if can't allocate.
long
maps_reserve(unsigned long numPages)
{
  DEBUG(alloc) kprintf(KR_OSTREAM, "maps reserving %d\n", numPages);
  unsigned int k = fls32(numPages - 1);
  // 2**(k-1) < numPages <= 2**(k)
  assert(k <= logMaxBlockSize);	// else block is too big

  CMTEMutex_lock(&mapsLock);

  // Look for a free block.
  unsigned int j;
  for (j = k; j <= logMaxBlockSize; j++)
    if (! link_isSingleton(&avail_list[j]))
      goto foundj;

  return -1;	// too bad
  

foundj: ;
  struct mpage * jmp
    = container_of(avail_list[j].next, struct mpage, listLink);
  assert(jmp->isFree);
  link_UnlinkUnsafe(&jmp->listLink);	// remove from free list
  jmp->isFree = false;
  unsigned long block = jmp - mpages;
  DEBUG(alloc) kprintf(KR_OSTREAM,
                       "maps reserve found 2**%d at 0x%x %#x\n", j, block, jmp);

  unsigned long x = block;
  unsigned long jj = 1UL << j;
  while (jj != numPages) {
    j--;
    jj = 1UL << j;
    if (jj >= numPages) {
      // Liberate upper half, repeat on lower half
      liberateOneBlock(x + jj, j);
    } else {
      // Repeat on upper half of the block.
      numPages -= jj;
      x += jj;
      mpages[x].isFree = false;
    }
  }

  CMTEMutex_unlock(&mapsLock);

  return block;
}

void
maps_init(void)
{
  int i;
  for (i=0; i <= logMaxBlockSize; i++) {
    link_Init(&avail_list[i]);
  }
  // mpages is initially zero.
  // Free all of the address space.
  // We are in initialization, so no need to lock.
  maps_liberate_locked(0, numpages);
}

// Uses KR_TEMP0 and KR_TEMP1.
result_t
maps_mapPage(unsigned long pgOffset, cap_t pageCap)
{
  result_t result;
  int gpt17slot = pgOffset / capros_GPT_nSlots;
  int gpt12slot = pgOffset % capros_GPT_nSlots;

  DEBUG(alloc) kprintf(KR_OSTREAM, "maps mapPage at 0x%x\n", pgOffset);

  CMTEMutex_lock(&mapsLock);

  result = capros_GPT_getSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP1);
  assert(result == RC_OK);

  // Copy one key.
  result = capros_GPT_setSlot(KR_TEMP1, gpt12slot, pageCap);
  if (result == RC_capros_key_Void) {
    // Need to create the l2v == 12 GPT
    // (We never free this GPT, even if all the space in it is unmapped.)
    result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_TEMP1);
    if (result != RC_OK)
      return result;
    result = capros_GPT_setL2v(KR_TEMP1, EROS_PAGE_LGSIZE);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP1);
    assert(result == RC_OK);

    // Now that the GPT12 is there, store pageCap again.
    result = capros_GPT_setSlot(KR_TEMP1, gpt12slot, pageCap);
    assert(result == RC_OK);
  }
  else
    assert(result == RC_OK);

  CMTEMutex_unlock(&mapsLock);

  return RC_OK;
}

void
maps_getCap(unsigned long pgOffset, cap_t pageCap)
{
  result_t result;
  int gpt17slot = pgOffset / capros_GPT_nSlots;
  int gpt12slot = pgOffset % capros_GPT_nSlots;

  result = capros_GPT_getSlot(KR_MAPS_GPT, gpt17slot, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_GPT_getSlot(KR_TEMP1, gpt12slot, pageCap);
  assert(result == RC_OK);
}
