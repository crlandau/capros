/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

/* Metaconstructor builds constructors.  This is a primordial domain,
 * and it is one of the places where the recursion stops.
 *
 * Metaconstructor is considered a constructor by administrative fiat,
 * even though it does not run the constructor code. */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Number.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>

#include <domain/domdbg.h>
#include <domain/ConstructorKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/PccKey.h>
#include <domain/Runtime.h>
#include "constituents.h"

uint32_t __rt_unkept = 1;

#define DEBUG if (0)
/* #define DEBUG if (1) */

#define KR_DISCRIM      KR_APP(0)
#define KR_YIELDCRE     KR_APP(1) /* constructor domain creator */
#define KR_SCRATCH      KR_APP(2) /* constructor domain creator */
#define KR_CON_SEG      KR_APP(3) /* constructor program segment */
#define KR_OSTREAM      KR_APP(4) /* our mouth */
#define KR_NEWDOM       KR_APP(5) /* where new constructor goes */
#define KR_YIELDBITS    KR_APP(6)
#define KR_RO_YIELDBITS KR_APP(7)

#define KR_ARG0    KR_ARG(0)
#define KR_ARG1    KR_ARG(1)
#define KR_ARG2    KR_ARG(2)

typedef struct {
  uint32_t constructor_pc;
} MetaConInfo;

/* On startup, entrypt of constructor is in KR_ARG0 */
void
InitMetaCon(MetaConInfo *mci)
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  DEBUG kdprintf(KR_OSTREAM, "Metacon inits\n");

  capros_Node_getSlot(KR_CONSTIT, KC_CON_PC, KR_NEWDOM);
  capros_Number_getWord(KR_NEWDOM, &mci->constructor_pc);

  capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM, KR_DISCRIM);
  capros_Node_getSlot(KR_CONSTIT, KC_YIELDCRE, KR_YIELDCRE);
  capros_Node_getSlot(KR_CONSTIT, KC_CON_SEG, KR_CON_SEG);
  /*   capros_Node_getSlot(KR_CONSTIT, KC_CON_CONSTIT, KR_CON_CONSTIT); */

  /* Create a runtime bits node appropriate for our yields: */
  capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_YIELDBITS);
  capros_Node_clone(KR_YIELDBITS, KR_RTBITS);
  capros_Node_swapSlot(KR_YIELDBITS, RKT_CREATOR, KR_YIELDCRE, KR_VOID);

  /* Now make a read-only yieldbits key. */
  capros_Node_reduce(KR_YIELDBITS, capros_Node_readOnly, KR_RO_YIELDBITS); 
}

uint32_t
MakeNewProduct(Message *msg, MetaConInfo *mci)
{
  uint32_t result;

  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = 0;		/* ordinary restart */
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_len = 0;

  result = proccre_create_process(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  if (result != RC_OK)
    return result;

  /* NOTE that if proccre_create_process succeeded, we know it's a good
     space bank. */
  
  /* Install the schedule.  KR_ARG1 can be reused after this. */
  (void) capros_Process_swapSchedule(KR_NEWDOM, KR_ARG1, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, 5, KR_ARG1, KR_VOID);

#define KR_ALTSCRATCH KR_ARG1
  /* Build a constituents node: */
  result = capros_SpaceBank_alloc1(KR_ARG0, capros_Range_otNode, KR_ALTSCRATCH);
  if (result != RC_OK)
    goto destroy_product;

  /* The new constructor constituents are the same as ours, so just
     duplicate that: */

  capros_Node_clone(KR_ALTSCRATCH, KR_CONSTIT);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, 1, KR_ALTSCRATCH, KR_VOID);
#undef KR_ALTSCRATCH

  /* runtime bits to product KR 2 */
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_RTBITS, KR_RO_YIELDBITS, KR_VOID);

  /* Install the address space of the new constructor */
  (void) capros_Process_swapAddrSpaceAndPC32(KR_NEWDOM, KR_CON_SEG,
           mci->constructor_pc, KR_VOID);

  /* POPULATE KEY REGISTERS */
  
  /* Place the new domain creator in the appropriate key register of
     the new constructor domain. */
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_SELF, KR_NEWDOM, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_BANK, KR_ARG0, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_RETURN, KR_RETURN, KR_VOID);
  
  (void) capros_Process_makeResumeKey(KR_NEWDOM, KR_SCRATCH);

  msg->snd_invKey = KR_SCRATCH;

  return RC_OK;

destroy_product:
  (void) proccre_destroy_process(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  return RC_capros_key_NoMoreNodes;
}

int
ProcessRequest(Message *msg, MetaConInfo* mci)
{
  /*initialize the keys being sent*/
  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;

  switch (msg->rcv_code) {
  case OC_Constructor_IsDiscreet:
    {
      msg->snd_w1 = 1;		/* answer YES */
      msg->snd_code = RC_OK;

      return 1;
    }      
    
  case OC_Constructor_Request:
    {
      msg->snd_code = MakeNewProduct(msg, mci);
      return 1;
    }      
    
  case OC_Constructor_Seal:
    {
      msg->snd_code = 1;	/* answer YES */
      return 1;
    }      

  case OC_capros_key_getType:			/* check alleged keytype */
    {
      msg->snd_code = RC_OK;
      msg->snd_w1 = AKT_MetaConstructor;
      return 1;
    }      

  default:
    break;
  }

  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}

int
main()
{
  Message msg;
  MetaConInfo mci;
  
  InitMetaCon(&mci);

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;
  msg.snd_data = 0;
  msg.snd_invKey = KR_VOID;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_ARG1;
  msg.rcv_key2 = KR_ARG2;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;

  do {
    /* no need to re-initialize rcv_len, since always 0 */
    RETURN(&msg);
  } while (ProcessRequest(&msg, &mci));

  return 0;
}
