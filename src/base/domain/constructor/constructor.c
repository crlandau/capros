/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

/* A constructor is responsible for building program instances.  The
 * constructor holds copies of each entry of the constituents node. In
 * addition, it holds the target process' keeper key, address space
 * key, symbol table ke, and initial PC.
 *
 * BOOTSTRAP NOTE 1:
 * 
 * To simplify system image construction, the constructor does some
 * minimal analysis at startup time.  If KC_PROD_CON0 and KC_PROD_XCON
 * are not void, they are accepted as holding the product constituents,
 * and KC_DCC should hold the domain creator for the constituents.
 * 
 * If initial constituents are found, the factory startup code assumes
 * that the factory should be initially sealed, and that no holes beyond
 * those that are apparent from the initial constituents are present,
 * and that KC_DCC is the actual domain creator.
 */

#include <string.h>
#include <stdbool.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/PCC.h>
#include <idl/capros/Constructor.h>

#include <domain/domdbg.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#include "constituents.h"
#include "debug.h"

uint32_t __rt_unkept = 1;

#define KR_SCRATCH      KR_APP(0)
#define KR_OSTREAM      KR_APP(2)
#define KR_YIELDCRE     KR_APP(3)
#define KR_NEWDOM       KR_APP(5)

#define KR_PROD_CON0    KR_APP(6) /* product's constituents */
#define KR_PROD_XCON    KR_APP(7) /* product's extended constituents */

#define KR_ARG0    KR_ARG(0)
#define KR_ARG1    KR_ARG(1)
#define KR_ARG2    KR_ARG(2)

/* Extended Constituents: */
#define XCON_KEEPER     0
#define XCON_ADDRSPACE  1
#define XCON_SYMTAB     2
#define XCON_PC         3

typedef struct {
  int frozen;
  int has_holes;
} ConstructorInfo;

void
CheckDiscretion(uint32_t kr, ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyInfo;
  bool isDiscreet;
  
  capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);

  result = capros_Discrim_verify(KR_SCRATCH, kr, &isDiscreet);
  if (result == RC_OK && isDiscreet)
    return;

  capros_Node_getSlot(KR_CONSTIT, KC_YIELDCRE, KR_SCRATCH);

  result = capros_ProcCre_amplifyGateKey(KR_SCRATCH, kr,
               KR_SCRATCH, 0, &keyInfo);

  if (result == RC_OK && keyInfo == 0) {
    uint32_t isDiscreet;
    /* This key is a requestor's key to a constructor. Ask the
       constructor if it is discreet */
    result = capros_Constructor_isDiscreet(kr, &isDiscreet);
    if (result == RC_OK && isDiscreet)
      return;
  }

  ci->has_holes = 1;
}

void
InitConstructor(ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyType;

  ci->frozen = 0;
  ci->has_holes = 0;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  DEBUG(init) kdprintf(KR_OSTREAM, "constructor init\n");
  
  capros_Node_getSlot(KR_CONSTIT, KC_PROD_CON0, KR_PROD_CON0);

  result = capros_key_getType(KR_PROD_CON0, &keyType);
  if (result != RC_capros_key_Void) {
    /* This is an initially frozen constructor.  Use the provided
       constituents, and assume that KC_YIELDCRE already holds the
       proper domain creator. */
    capros_Node_getSlot(KR_CONSTIT, KC_YIELDCRE, KR_YIELDCRE);
    capros_Node_getSlot(KR_CONSTIT, KC_PROD_CON0, KR_PROD_CON0);
    capros_Node_getSlot(KR_CONSTIT, KC_PROD_XCON, KR_PROD_XCON);

    ci->frozen = 1;
  }
  else {
    /* Build a new domain creator for use in crafting products */

    /* use KR_YIELDCRE and KR_DISCRIM as scratch regs for a moment: */
    capros_Node_getSlot(KR_CONSTIT, KC_PCC, KR_YIELDCRE);
    capros_Process_getSchedule(KR_SELF, KR_SCRATCH);

    result = capros_PCC_createProcessCreator(KR_YIELDCRE,
               KR_BANK, KR_SCRATCH, KR_YIELDCRE);
    DEBUG(init) kdprintf(KR_OSTREAM, "GOT DOMCRE Result is 0x%08x\n", result);
  
    capros_SpaceBank_alloc2(KR_BANK,
                            capros_Range_otNode | (capros_Range_otNode << 8),
                            KR_PROD_CON0, KR_PROD_XCON);
  }
}

/* In spite of unorthodox fabrication, the constructor self-destructs
   in the usual way. */
void
Sepuku()
{
  /* FIX: giving up the constituent nodes breaks our products */
  
  /* Give up the first constituent node. */
  /* Give up the second constituent node */
  capros_SpaceBank_free2(KR_BANK, KR_PROD_CON0, KR_PROD_XCON);

  /* capros_Node_getSlot(KR_CONSTIT, KC_MYDOMCRE, KR_DOMCRE); */
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPACE, KR_PROD_CON0);

  capros_SpaceBank_free1(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_PROD_CON0, KR_SELF,
		     KR_CREATOR, KR_BANK, 1);
}

uint32_t
MakeNewProduct(Message *msg)
{
  uint32_t result;

  DEBUG(product) kdprintf(KR_OSTREAM, "Making new product...\n");

  result = capros_ProcCre_createProcess(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  if (result != RC_OK) {
    kdprintf(KR_OSTREAM, "make prod failed with 0x%x\n", result);
    return result;
  }

  /* NOTE that if capros_ProcCre_createProcess succeeded, we know it's a good
     space bank. */
  
  /* Build a constiuents node, since we will need the scratch register
     later */
  result = capros_SpaceBank_alloc1(KR_ARG0, capros_Range_otNode, KR_SCRATCH);
  if (result != RC_OK)
    goto destroy_product;

  /* clone the product constituents into the new constituents node: */
  capros_Node_clone(KR_SCRATCH, KR_PROD_CON0);

  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_CONSTIT, KR_SCRATCH, KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Populate new domain\n");

  /* Install protospace into the domain root: */
  (void) capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPACE, KR_SCRATCH);
  (void) capros_Process_swapAddrSpaceAndPC32(KR_NEWDOM, KR_SCRATCH,
           0, // protospace PC, well known to be zero
           KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Installed protospace\n");

  /* Install the schedule key into the domain: */
  (void) capros_Process_swapSchedule(KR_NEWDOM, KR_ARG1, KR_VOID);
  
  DEBUG(product) kdprintf(KR_OSTREAM, "Installed sched\n");

  /* Keeper constructor to keeper slot */
  (void) capros_Node_getSlot(KR_PROD_XCON, XCON_KEEPER, KR_SCRATCH);
  (void) capros_Process_swapKeeper(KR_NEWDOM, KR_SCRATCH, KR_VOID);

  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_SELF, KR_NEWDOM, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_CREATOR, KR_YIELDCRE, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_BANK, KR_ARG0, KR_VOID);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_SCHED, KR_ARG1, KR_VOID);

  DEBUG(product) kdprintf(KR_OSTREAM, "Sched in target KR_SCHED\n");

  (void) capros_Node_getSlot(KR_PROD_XCON, XCON_ADDRSPACE, KR_SCRATCH);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, PSKR_SPACE, KR_SCRATCH, KR_VOID);

  (void) capros_Node_getSlot(KR_PROD_XCON, XCON_SYMTAB, KR_SCRATCH);
  (void) capros_Process_swapSymSpace(KR_NEWDOM, KR_SCRATCH, KR_VOID);

  (void) capros_Node_getSlot(KR_PROD_XCON, XCON_PC, KR_SCRATCH);
  (void) capros_Process_swapKeyReg(KR_NEWDOM, PSKR_PROC_PC, KR_SCRATCH, KR_VOID);

  /* User ARG2 to key arg slot 0 */
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_ARG(0), KR_ARG2, KR_VOID);

  /* Resume key to KR_RETURN */
  (void) capros_Process_swapKeyReg(KR_NEWDOM, KR_RETURN, KR_RETURN, KR_VOID);

  /* Make up a fault key to the new process so we can set it in motion */

  DEBUG(product) kdprintf(KR_OSTREAM, "About to call get fault key\n");
  (void) capros_Process_makeResumeKey(KR_NEWDOM, KR_SCRATCH);

  DEBUG(start) kdprintf(KR_OSTREAM, "Invoking fault key to yield...\n");

  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = 0;		/* ordinary restart */
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_len = 0;
  msg->snd_invKey = KR_SCRATCH;
  
	/* We would like to pass the order code on to the product,
	but it is not receiving a message, so the following doesn't matter. */
  return msg->rcv_code;

destroy_product:
  (void) capros_ProcCre_destroyProcess(KR_YIELDCRE, KR_ARG0, KR_NEWDOM);
  return RC_capros_key_NoMoreNodes;
}

int
is_not_discreet(uint32_t kr, ConstructorInfo *ci)
{
  uint32_t result;
  uint32_t keyInfo;
  bool isDiscreet;
  
  capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);
  
  DEBUG(misc) kdprintf(KR_OSTREAM, "constructor: is_not_discreet(): discrim_verify\n");

  capros_Discrim_verify(KR_SCRATCH, kr, &isDiscreet);
  if (isDiscreet)
    return 0;			/* ok */

  DEBUG(misc) kdprintf(KR_OSTREAM, "constructor: is_not_discreet(): proccre_amplify\n");
  result = capros_ProcCre_amplifyGateKey(KR_YIELDCRE, kr,
                                 KR_SCRATCH, 0, &keyInfo);
  if (result == RC_OK && keyInfo == 0) {
    uint32_t isDiscreet;
    /* This key is a requestor's key to a constructor. Ask the
       constructor if it is discreet */
    result = capros_Constructor_isDiscreet(kr, &isDiscreet);
    if (result == RC_OK && isDiscreet)
      return 0;			/* ok */
  }

  return 1;
}


/* Someday this should start building up a list of holes... */
void
add_new_hole(uint32_t kr, ConstructorInfo *ci)
{
  ci->has_holes = 1;
}

uint32_t
insert_constituent(uint32_t ndx, uint32_t kr, ConstructorInfo *ci)
{
  DEBUG(build) kdprintf(KR_OSTREAM, "constructor: insert constituent %d\n", ndx);

  if (ndx >= EROS_NODE_SIZE)
    return RC_capros_key_RequestError;
    
  if ( is_not_discreet(kr, ci) )
    add_new_hole(kr, ci);
  
  /* now insert the constituent in the proper constituents node: */
  capros_Node_swapSlot(KR_PROD_CON0, ndx, kr, KR_VOID);
	    
  return RC_OK;
}

uint32_t
insert_xconstituent(uint32_t ndx, uint32_t kr, ConstructorInfo *ci)
{
  DEBUG(build) kdprintf(KR_OSTREAM, "constructor: insert xconstituent %d\n", ndx);

  if (ndx == XCON_PC) {
    uint32_t obClass;
    
    /* This copy IS redundant with the one in is_not_discreet(), but
       this path is not performance critical and clarity matters too. */
    capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM, KR_SCRATCH);
  
    capros_Discrim_classify(KR_SCRATCH, kr, &obClass);
    if (obClass != capros_Discrim_clNumber)
      return RC_capros_key_RequestError;
  }

  if (ndx > XCON_PC)
    return RC_capros_key_RequestError;
    
  if ( is_not_discreet(kr, ci) )
    add_new_hole(kr, ci);
  
  /* now insert the constituent in the proper constituents node: */
  capros_Node_swapSlot(KR_PROD_XCON, ndx, kr, KR_VOID);
	    
  return RC_OK;
}

void
ProcessRequest(Message *msg, ConstructorInfo *ci)
{
  switch (msg->rcv_code) {
  case OC_capros_Constructor_isDiscreet:
    {
      if (ci->frozen && !ci->has_holes)
	msg->snd_w1 = 1;
      else
	msg->snd_w1 = 0;
      msg->snd_code = RC_OK;

      break;
    }      
    
  case OC_capros_Constructor_request:
    {
      DEBUG(product) kdprintf(KR_OSTREAM, "constructor: request product\n");

      msg->snd_code = MakeNewProduct(msg);
      break;
    }      
    
  case OC_capros_Constructor_seal:
    {
      DEBUG(build) kdprintf(KR_OSTREAM, "constructor: seal\n");

      ci->frozen = 1;

      capros_Process_makeStartKey(KR_SELF, 0, KR_NEWDOM);
      msg->snd_key0 = KR_NEWDOM;

      break;
    }      

  case OC_capros_Constructor_insertConstituent:
    {
      msg->snd_code = insert_constituent(msg->rcv_w1, KR_ARG0, ci);
      
      break;
    }      

  case OC_capros_Constructor_insertKeeper:
    {
      msg->snd_code = insert_xconstituent(XCON_KEEPER, KR_ARG0, ci);
      
      break;
    }      

  case OC_capros_Constructor_insertAddrSpace:
    {
      msg->snd_code = insert_xconstituent(XCON_ADDRSPACE, KR_ARG0, ci);
      
      break;
    }      

  case OC_capros_Constructor_insertSymtab:
    {
      msg->snd_code = insert_xconstituent(XCON_SYMTAB, KR_ARG0, ci);
      
      break;
    }      

  case OC_capros_Constructor_insertPC:
    {
      msg->snd_code = insert_xconstituent(XCON_PC, KR_ARG0, ci);
      
      break;
    }      

  case OC_capros_key_destroy:
    {
      Sepuku();
      // does not return
    }
  
  case OC_capros_key_getType:
    msg->snd_w1 = AKT_ConstructorRequestor;
    //msg->snd_w1 = IKT_capros_Constructor;
    break;

  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;
  }
}

int
main()
{
  Message Msg;
  Message * msg = &Msg;	// to address it consistently
  ConstructorInfo ci;
  
  InitConstructor(&ci);

  /* Recover our own process key from constit1 into slot 1 */
  capros_Process_makeStartKey(KR_SELF, 1, KR_SCRATCH);

  msg->snd_key0 = KR_SCRATCH;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_code = 0;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_len = 0;
  msg->snd_data = 0;
  msg->snd_invKey = KR_RETURN;

  msg->rcv_key0 = KR_ARG0;
  msg->rcv_key1 = KR_ARG1;
  msg->rcv_key2 = KR_ARG2;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_data = 0;
  msg->rcv_limit = 0;

  while (1) {
    /* no need to re-initialize rcv_len, since always 0 */
    RETURN(msg);
  
    /* set the default message to be sent: */
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

    if (msg->rcv_keyInfo == 0) {	// builder's key
      ProcessRequest(msg, &ci);
    } else {			// requestor's key
      switch (msg->rcv_code) {
        case OC_capros_key_getType:
          /* FIXME: This is wrong: each different constructor requestor should
          have its own key type. */
          msg->snd_w1 = AKT_ConstructorRequestor;
          break;

        default:
          msg->snd_code = MakeNewProduct(msg);
      }
    }
  }

  return 0;
}
