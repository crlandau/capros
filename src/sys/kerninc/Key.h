#ifndef __KERNINC_KEY_H__
#define __KERNINC_KEY_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <disk/KeyStruct.h>
#include <disk/Key-inline.h>
#include <kerninc/ObjectHeader.h>

struct ObjectLocator;

/* 
 * Key.  A Key is a capability, in the sense of Hydra or the Sigma-7.
 * EROS is a pure capability system, which means that Keys are the
 * only namespace known to the system.  They are therefore the *only*
 * way that one can refer to an object within EROS.
 * 
 * A Key names a Page (see Page.hxx), a Node (see Node.hxx), a Number,
 * or one of the EROS devices or miscellaneous kernel tools.
 * 
 * Keys have an on-disk and in-core form.  Both are 3 words.  A valid
 * in-core key points to an in-core object.
 * 
 * Before being used, a Key must be converted to in-core form, but
 * this is done in a lazy fashion, so many of the Keys that happen to
 * be in core remain encoded in the on-disk form.  A Key that has been
 * converted for use in its in-core form is said to be "prepared."
 * 
 * Number, Device, and Miscellaneous keys have the same form on disk
 * as in core:
 * 
 *      +--------+--------+--------+--------+
 *      |             Data[31:0]            |
 *      +--------+--------+--------+--------+
 *      |             Data[63:32]           |
 *      +--------+--------+--------+--------+
 *      |000 Type|    Data[87:64]           |
 *      +--------+--------+--------+--------+
 * 
 * NOTE: Reorder per machine's natural word order for long long
 * 
 * In the in-core form, the P bit is always set (1).  This eliminates
 * the need to special case the check for Key preparedness for this
 * class of Keys.
 * 
 * The Type field encodes only primary keys, and is always 5 bits
 * wide. In the Number key, the Data slots hold the numeric value.  In
 * Device and Miscellaneous key, they differentiate the called device
 * or tool.
 * 
 * The Sanity field is used by the system sanity checker to verify
 * that the world is sane.  It's value should not be examined or
 * counted on.
 * 
 * 
 * Page and Node keys (all other cases) are a bit different in core
 * then they are on disk:
 * 
 *      +--------+--------+--------+--------+
 *      |          Allocation Count         |    PREPARED IN CORE
 *      +--------+--------+--------+--------+
 *      |   Pointer to ObHeader for object  |
 *      +--------+--------+--------+--------+
 *      |Phh Type|Datauint8_t|    OID[47:32]   |
 *      +--------+--------+--------+--------+
 * 
 *      +--------+--------+--------+--------+
 *      |          Allocation Count         |    ON DISK
 *      +--------+--------+--------+--------+
 *      |             OID[31:0]             |
 *      +--------+--------+--------+--------+
 *      |000 Type|Datauint8_t|    OID[47:32]   |
 *      +--------+--------+--------+--------+
 * 
 * For an interpretation of the fields, see eros/KeyType.hxx.
 * 
 * The Datauint8_t is an 8 bit value handed to the recipient when the Key
 * is invoked.  It allows the recipient to handle multiple client
 * classes by providing each client class with a unique Datauint8_t.
 * 
 * The ObHeader Pointer is an in-core pointer to the object denoted by
 * the Key.
 * 
 * The Allocation Count is used to rescind keys to pages and notes.
 * This is described in the rescind logic for Keys.
 * 
 */

typedef struct KeyBits Key;

extern Key key_VoidKey;

bool key_DoPrepare(Key* thisPtr);
bool key_DoValidate(Key * thisPtr);
bool CheckObjectType(OID oid, struct ObjectLocator * pObjLoc,
                     unsigned int baseType);

#ifndef NDEBUG
bool key_IsValid(const Key* thisPtr);
#endif
int key_ValidKeyPtr(const Key * pKey);

void key_MakeUnpreparedCopy(Key * dst, const Key * src);

/* ALL OF THE NH ROUTINES MUST BE CALLED ON NON-HAZARDED KEYS */

/* Unprepare key with intention to overwrite immediately, so
 * no need to remember that the key was unprepared.
 */

INLINE void 
key_NH_Unchain(Key* thisPtr)
{
  keyBits_Unchain(thisPtr);
}

/* Convert the key to its unprepared form. */
void key_NH_Unprepare(Key* thisPtr);

void key_NH_Set(KeyBits *thisPtr, KeyBits* kb);
void key_NH_Move(Key * to, Key * from);

void key_SetToProcess(Key * k,
  Process * p, unsigned int keyType, unsigned int keyData);
void key_SetToNumber(KeyBits *thisPtr, uint32_t hi, uint32_t mid, uint32_t lo);

INLINE void 
key_NH_SetToVoid(Key* thisPtr)
{
#ifdef __KERNEL__
  keyBits_Unchain(thisPtr);
#endif

  keyBits_InitToVoid(thisPtr);
}

void key_SetToObj(Key * key, ObjectHeader * pObj,
  unsigned int kkt, unsigned int keyPerms, unsigned int keyData);

OID  key_GetKeyOid(const Key* thisPtr);
ObjectHeader *key_GetObjectPtr(const Key* thisPtr);
uint32_t key_GetAllocCount(const Key* thisPtr);

void key_Print(const Key* thisPtr);

#ifdef OPTION_OB_MOD_CHECK
uint32_t key_CalcCheck(const Key * thisPtr);
#endif

#ifdef OPTION_DDB
void db_eros_print_number_as_string(Key * k);
#endif

#endif /* __KERNINC_KEY_H__ */
