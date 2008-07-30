/*
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <disk/DiskNode.h>
#include <disk/GenerationHdr.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Check.h>
#include <kerninc/ObjH-inline.h>
#include <idl/capros/Range.h>
#include <idl/capros/MigratorTool.h>
#include <eros/Invoke.h>
#include <eros/machine/IORQ.h>

#define dbg_ckpt	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_ckpt )

#define DEBUG(x) if (dbg_##x & dbg_flags)

LID logCursor = 0;	// next place to write in the main log
LID logWrapPoint;
LID oldestNonRetiredGenLid;
LID oldestNonNextRetiredGenLid;
LID workingGenFirstLid;
frame_t logSizeLimited;
GenNum retiredGeneration = 0;

unsigned int ckptState = ckpt_NotActive;

long numKRODirtyPages = 0;	// including pots
long numKRONodes = 0;
unsigned int KROPageCleanCursor;
unsigned int KRONodeCleanCursor;

PageHeader * GenHdrPageH = NULL;
LID nextProcDirLid;
PageHeader * * nextProcDirFramePP;
PageHeader * * ProcDirFramesWritten;
unsigned long numDirEntsToSave;
unsigned int nextRangeToSync;
unsigned int rangesSynced;

PageHeader * reservedPages = NULL;
unsigned int numReservedPages = 0;


/*
If no checkpoint is active, nextRetiredGeneration is undefined.
If a checkpoint is active and the migratedGenNumber field in the
  DiskGenerationHdr has been fixed,
  nextRetiredGeneration has the value in that field.
If a checkpoint is active and the migratedGenNumber field in the
  DiskGenerationHdr has not been fixed,
  nextRetiredGeneration has zero.
*/
GenNum nextRetiredGeneration = 0;

GenNum migratedGeneration = 0;	// the latest fully migrated generation

DEFQUEUE(WaitForCkptInactive);
DEFQUEUE(WaitForCkptNeeded);
DEFQUEUE(WaitForObjDirWritten);

/* During a checkpoint, persistent pages can be in the following states:

 -- OFLG_ --  I/O Generation(s) DirEnt Notes
KRO Dirty Wkg  *1         *2    exists
 1    1    0   0  Wkg and Cur     no   need to clean
 1    1    1   0  Wkg             no   need to clean
 1    1    0  cln Wkg and Cur     no   Before the demarcation event, this page
                                       began to be cleaned and was redirtied.
 1    1    1  cln Wkg             no   Ditto, and the page was COW'ed before
                                       the clean finished.
 1    0    0  cln Wkg and Cur     yes  being cleaned
 1    0    1  cln Wkg             yes  being cleaned
 0    0    0   0  Wkg and Cur     yes  
 0    0    0  fet Wkg and Cur     yes  being fetched
 0    1    0   0          Cur     no   can't be cleaned now

 *1 This state is a combination of ioreq and OFLG_Fetching:
    0: ioreq == 0                       (no I/O)
    cln: ioreq != 0, OFLG_Fetching == 0 (being cleaned)
    fet: ioreq != 0, OFLG_Fetching == 1 (being fetched)

 *2 "Cur" means this page is the current version and can be found
    by objH_Lookup.
*/

INLINE void
minEqualsL(long * var, long value)
{
  if (*var > value)
    *var = value;
}

/* Return the amount of log needed to checkpoint all the dirty objects. */
unsigned long
CalcLogReservation(unsigned long numDirtyObjects[],
  unsigned long existingLogEntries)
{
  unsigned long total = 0;
  total += (numDirtyObjects[capros_Range_otNode] + DISK_NODES_PER_PAGE - 1)
             / DISK_NODES_PER_PAGE;
  total += numDirtyObjects[capros_Range_otPage];
  total += numDirtyLogPots;
  // Space to write out the log directory:
#define DirEntsPerPage (EROS_PAGE_SIZE / sizeof(ObjectDescriptor))
  total += (existingLogEntries
            + numDirtyObjects[capros_Range_otNode]
            + numDirtyObjects[capros_Range_otPage] + DirEntsPerPage - 1)
           / DirEntsPerPage;
  // Space to write out the maximum process directory:
#define ProcEntsPerPage (EROS_PAGE_SIZE / sizeof(struct DiskProcessDescriptor))
  total += (KTUNE_NCONTEXT + ProcEntsPerPage - 1) / ProcEntsPerPage;
  total += 1;	// for the generation header frame
  return total;
#undef ProcEntsPerPage
#undef DirEntsPerPage
}

// May Yield.
static void
ReservePages(unsigned int numPagesWanted)
{
  while (numReservedPages < numPagesWanted) {
    PageHeader * pageH = objC_GrabPageFrame();
    numReservedPages++;
    // Link into free list:
    *(PageHeader * *)&pageH->kt_u.free.freeLink.next = reservedPages;
    reservedPages = pageH;
  }
}

// Declare a demarcation event, which is the beginning of a checkpoint.
// Yields.
void
DeclareDemarcationEvent(void)
{
  assert(!ckptIsActive());

  ckptState = ckpt_Phase1;
  sq_WakeAll(&WaitForCkptNeeded, false);
}

void
CheckpointPage(PageHeader * pageH)
{
  ObjectHeader * pObj = pageH_ToObj(pageH);

  assert(! objH_GetFlags(pObj, OFLG_KRO));

  if (objH_GetFlags(pObj, OFLG_DIRTY)
    /* If this page is being cleaned, we must make it KRO to ensure
       it stays clean: */
      || (pageH->ioreq && ! objH_GetFlags(pObj, OFLG_Fetching))) {
    // Make this page Kernel Read Only.
    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtDataPage:
      pageH_MakeReadOnly(pageH);
    }
    objH_SetFlags(pObj, OFLG_KRO);
    if (objH_GetFlags(pObj, OFLG_DIRTY)) {
      numKRODirtyPages++;
    } else {
      /* This page is being cleaned and is clean.
      Since it is now KRO, we know it will stay clean, so we can create
      the log directory entry now. */
      CreateLogDirEntryForNonzeroPage(pageH);
    }
  }
}

static void
DoPhase1Work(void)
{
  int i;
  // Grab a page for the generation header, if we haven't already:
  if (! GenHdrPageH)
    GenHdrPageH = objC_GrabPageFrame();
  DiskGenerationHdr * genHdr
    = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);

  long numActivities = KTUNE_NACTIVITY - numFreeActivities;
  /* Note, some of the Activitys will be for non-persistent processes,
  so this is an upper bound on the number we need.
  It won't hurt to have an extra free page or two around. */

  // Some can go in the generation header:
  numActivities -= (EROS_PAGE_SIZE - sizeof(DiskGenerationHdr))
                   / sizeof(struct DiskProcessDescriptor);
  minEqualsL(&numActivities, 0);
  // Number of pages we need for the rest:
#define numActivitiesPerPage (EROS_PAGE_SIZE / sizeof(struct DiskProcessDescriptor))
  long pagesToReserve = (numActivities + numActivitiesPerPage - 1)
                       / numActivitiesPerPage;
  // Reserve at least 2 for object directory frames.
  // We reserve them now rather than in phase 3, because at that time
  // no cleaning can be done, so pages might not be available.
  /* FIXME: What is the right number to reserve?
  We don't know the exact number of directory frames that will be needed,
  because the directory entries haven't all been created yet.
  We should have a few for each disk. */
  minEqualsL(&pagesToReserve, 2);
  ReservePages(pagesToReserve);
  /* Note, if the above Yields, when restarted we will start at
  DoPhase1Work, which will recalculate numActivities,
  which may have changed. */

  /* This is the moment of demarcation.
  From here through the end of DoPhase1Work, we must NOT Yield.
  This must be atomic. */

  // Don't checkpoint a broken system:
  check_Consistency("before ckpt");

  KROPageCleanCursor = 0;
  KRONodeCleanCursor = 0;

  // Scan all pages.
  unsigned long objNum;
  for (objNum = 0; objNum < objC_nPages; objNum++) {
    PageHeader * pageH = objC_GetCorePageFrame(objNum);

    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtTagPot:
    case ot_PtHomePot:
      assertex(pageH, ! objH_GetFlags(pageH_ToObj(pageH), OFLG_KRO));

      if (objH_GetFlags(pageH_ToObj(pageH), OFLG_DIRTY)) {
        assert(!"complete");	// figure this out later
      }
      break;

    case ot_PtDataPage:
      if (! OIDIsPersistent(pageH_ToObj(pageH)->oid))
        break;
    case ot_PtLogPot:
      assertex(pageH, ! objH_GetFlags(pageH_ToObj(pageH), OFLG_KRO));

      CheckpointPage(pageH);
      break;
    }
  }

  // Scan all nodes.
  for (objNum = 0; objNum < objC_nNodes; objNum++) {
    Node * pNode = objC_GetCoreNodeFrame(objNum);
    ObjectHeader * pObj = node_ToObj(pNode);

    if (pObj->obType == ot_NtFreeFrame)
      continue;

    if (! OIDIsPersistent(pObj->oid))
      break;

    if (objH_GetFlags(pObj, OFLG_DIRTY)) {
      assert(! objH_GetFlags(pObj, OFLG_KRO));
      // Make this node Kernel Read Only.

      /* Unpreparing the node ensures that when we next try to dirty
       * the node, we will notice it is KRO. */
      node_Unprepare(pNode);
      numKRONodes++;
      objH_SetFlags(pObj, OFLG_KRO);
    }
  }

  // Save Activity's to the process directory.
  struct DiskProcessDescriptor * dpd;
  dpd = (struct DiskProcessDescriptor *)
        ((char *)genHdr + sizeof(DiskGenerationHdr));
  unsigned int dpdsInCurrentPage = 0;
  bool inHdr = true;
  nextProcDirFramePP = &reservedPages;
  ProcDirFramesWritten = nextProcDirFramePP;
  // Where to store the number of descriptors in the current page:
  uint32_t * numDpdsLoc = &genHdr->processDir.nDescriptors;
  genHdr->processDir.firstDirFrame = 0;
  genHdr->processDir.nDirFrames = 0;	// so far
  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];
    if (act->state != act_Free) {
      OID procOid;
      ObCount procAllocCount;
      if (act->context) {	// process info is in the Process structure
        Process * proc = act->context;
        if (proc_IsKernel(proc))
          continue;
        ObjectHeader * pObj = node_ToObj(proc->procRoot);
#if 0
        printf("P1 act=%#x proc=%#x root=%#x\n", act, proc, pObj);
#endif
        procOid = pObj->oid;
        procAllocCount = pObj->allocCount;
      } else {
        if (! keyBits_IsType(&act->processKey, KKT_Process))
          continue;	// process was rescinded
        procOid = key_GetKeyOid(&act->processKey);
        procAllocCount = key_GetAllocCount(&act->processKey);
      }
      if (OIDIsPersistent(procOid)) {
        // Is there room for another DiskProcessDescriptor in this page?
        kva_t roomInPage = (- (kva_t)dpd) & EROS_PAGE_MASK;
        if (roomInPage < sizeof(struct DiskProcessDescriptor)) {
          // Finish the current page.
          *numDpdsLoc = dpdsInCurrentPage;
          // Set up the next page.
          PageHeader * pageH = *nextProcDirFramePP;
          assert(pageH);	// else we didn't reserve enough!
          LID lid = NextLogLoc();
          if (inHdr) {		// this is the first full frame
            genHdr->processDir.firstDirFrame = lid;
            nextProcDirLid = lid;
            inHdr = false;
          }
          genHdr->processDir.nDirFrames++;
          numDpdsLoc = (uint32_t *)pageH_GetPageVAddr(pageH);
          dpd = (struct DiskProcessDescriptor *)
                ((kva_t)numDpdsLoc + sizeof(uint32_t));
          // Follow the chain:
          nextProcDirFramePP
            = (PageHeader * *)& pageH->kt_u.free.freeLink.next;
        }
        uint8_t hazToSave;
        if (act->state == act_Sleeping && act->actHazard == actHaz_None)
          // On restart, wake sleepers with an error.
          hazToSave = actHaz_WakeRestart;
        else
          hazToSave = act->actHazard;;
        // Use memcpy, because dpd is unaligned and packed.
        memcpy(&dpd->oid, &procOid, sizeof(OID));
        memcpy(&dpd->allocCount, &procAllocCount, sizeof(ObCount));
        memcpy(&dpd->actHazard, &hazToSave, sizeof(dpd->actHazard));
        dpd++;
        dpdsInCurrentPage++;
      }
    }
  }
  // Finish the last page.
  *numDpdsLoc = dpdsInCurrentPage;

  // Free any unused reserved pages:
  while (1) {
    PageHeader * pageH = *nextProcDirFramePP;
    if (! pageH)
      break;
    // Follow the chain:
    nextProcDirFramePP
      = (PageHeader * *)& pageH->kt_u.free.freeLink.next;
    ReleasePageFrame(pageH);
  }

  monotonicTimeOfLastDemarc = sysT_NowPersistent();

  ckptState = ckpt_Phase2;
}

// Note, dod is not aligned!
static unsigned int
WriteDirEntsToPage(struct DiskObjectDescriptor * dod)
{
  unsigned int numDirEntsInPage = 0;
  while (1) {
    if (! numDirEntsToSave)
      break;		// no more to write
    kva_t roomInPage = (- (kva_t)dod) & EROS_PAGE_MASK;
    if (roomInPage < sizeof(struct DiskObjectDescriptor))
      break;		// no more will fit

    const ObjectDescriptor * od = ld_findNextObject(workingGenerationNumber);
    assert(od);		// else ran out before count ran out
    // dod is unaligned and packed, so use memcpy.
    memcpy(&dod->oid, &od->oid, sizeof(OID));
    memcpy(&dod->allocCount, &od->allocCount, sizeof(ObCount));
    memcpy(&dod->callCount, &od->callCount, sizeof(ObCount));
    memcpy(&dod->lid, &od->logLoc, sizeof(LID));
    memcpy(&dod->type, &od->type, sizeof(uint8_t));

    dod++;
    numDirEntsInPage++;
    numDirEntsToSave--;
  }
  return numDirEntsInPage;
}

static void
DoPhase2Work(void)
{
  // FIXME! Since the checkpoint process has high priority,
  // it will always get IORequest blocks before users,
  // so "clean me first" won't work!

  // Write the process directory frames.
  while (ProcDirFramesWritten != nextProcDirFramePP) {
    PageHeader * pageH = *ProcDirFramesWritten;
    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    DEBUG(ckpt) printf("Writing proc dir frame %#x\n", pageH);
    ioreq->pageH = pageH;	// link page and ioreq
    pageH->ioreq = ioreq;

    ObjectRange * rng = LidToRange(nextProcDirLid);
    assert(rng);	// it had better be mounted

    ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = OIDToFrame(nextProcDirLid - rng->start);
    ioreq->doneFn = &IOReq_EndWrite;
    sq_Init(&ioreq->sq);	// won't be used
    ioreq_Enqueue(ioreq);

    ProcDirFramesWritten = (PageHeader * *)&pageH->kt_u.free.freeLink.next;
    nextProcDirLid = IncrementLID(nextProcDirLid);
  }

  while (numKRONodes)
    CleanAKRONode();

  /* The stages in the life of a typical page are:
  1. Page is dirty
  2. A demarcation event occurs. The page is now dirty and KRO.
  3. The page is queued to be cleaned by the disk driver.
     It is marked KRO and clean. (We always mark a page clean *before*
     cleaning it, so we will know if it is dirtied while being cleaned.)
  4. The cleaning finishes. The page is now clean and not KRO. */
  while (numKRODirtyPages)
    CleanAKROPage();

  // Phase 2 is committed. Nothing below Yields.

  // All the dir ents for the working generation have now been created,
  // even though pages may still be queued for the disk driver.
  numDirEntsToSave = ld_numWorkingEntries();
  ld_resetScan(workingGenerationNumber);

  // Some can go in the generation header:
  DiskGenerationHdr * genHdr
    = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);
  struct DiskObjectDescriptor * dod = (struct DiskObjectDescriptor *)
      (char *)genHdr
      + sizeof(DiskGenerationHdr)
      + genHdr->processDir.nDescriptors
        * sizeof(struct DiskProcessDescriptor);

  // Write dir ents to the generation header:
  genHdr->objectDir.nDescriptors = WriteDirEntsToPage(dod);
  genHdr->objectDir.firstDirFrame = 0;	// no frame yet
  genHdr->objectDir.nDirFrames = 0;

  nextRangeToSync = 0;
  rangesSynced = 0;

  ckptState = ckpt_Phase3;
}

static void IOReq_EndObDirWrite(IORequest * ioreq);

static void
WriteAPageOfObDirEnts(PageHeader * pageH, IORequest * ioreq)
{
  // Fill the page with dir ents:
  uint32_t * objDirFrame = (uint32_t *)pageH_GetPageVAddr(pageH);
  struct DiskObjectDescriptor * dod
    = (struct DiskObjectDescriptor *)(objDirFrame + 1);
  *objDirFrame = WriteDirEntsToPage(dod);

  // Write it to disk:
  DEBUG(ckpt) printf("Writing ob dir frame %#x\n", pageH);
  ioreq->pageH = pageH;	// link page and ioreq
  pageH->ioreq = ioreq;

  // There is no cleaning going on, so the LIDs we allocate here
  // will be consecutive.
  LID lid = NextLogLoc();
  DiskGenerationHdr * genHdr
    = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);
  if (genHdr->objectDir.firstDirFrame == 0)
    genHdr->objectDir.firstDirFrame = lid;
  genHdr->objectDir.nDirFrames++;

  ObjectRange * rng = LidToRange(lid);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(lid - rng->start);
  ioreq->doneFn = &IOReq_EndObDirWrite;
  sq_Init(&ioreq->sq);	// won't be used
  ioreq_Enqueue(ioreq);
}

static void
IOReq_EndObDirWrite(IORequest * ioreq)
{
  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));
  PageHeader * pageH = ioreq->pageH;

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  if (numDirEntsToSave) {
    // Use this page and ioreq to write another page of dir ents.
    WriteAPageOfObDirEnts(pageH, ioreq);
    if (! numDirEntsToSave)	// we have now written them all
      sq_WakeAll(&WaitForObjDirWritten, false);
  } else {
    IOReq_Deallocate(ioreq);
    ReleasePageFrame(pageH);
  }
}

static void
IOReq_EndSync(IORequest * ioreq)
{
  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));

  if (++rangesSynced >= nextRangeToSync)
    sq_WakeAll(&WaitForObjDirWritten, false);

  IOReq_Deallocate(ioreq);
}

static void
DoPhase3Work(void)
{
  /* Pages for the directory were reserved earlier.
  Now would be a bad time to reserve pages, because nothing can be cleaned. */
  // Now put all the reserved pages to work as object directory frames:
  while (reservedPages) {
    PageHeader * pageH = reservedPages;
    // Unlink it:
    reservedPages = *(PageHeader * *)&pageH->kt_u.free.freeLink.next;
    if (numDirEntsToSave) {
      IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
      WriteAPageOfObDirEnts(pageH, ioreq);
    } else {
      ReleasePageFrame(pageH);
    }
  }

  if (numDirEntsToSave) {
    SleepOnPFHQueue(&WaitForObjDirWritten);
  }

  // Verify that there are no more dir ents:
  assert(ld_findNextObject(workingGenerationNumber) == NULL);

  // Wait for all the previous log writes to get to nonvolatile media.
  while (nextRangeToSync < nLidRanges) {
    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    ObjectRange * rng = &lidRanges[nextRangeToSync++];

    ioreq->pageH = NULL;
    ioreq->requestCode = capros_IOReqQ_RequestType_synchronizeCache;
    ioreq->objRange = rng;
    ioreq->doneFn = &IOReq_EndSync;
    sq_Init(&ioreq->sq);	// won't be used
    ioreq_Enqueue(ioreq);
  }
  if (rangesSynced < nextRangeToSync)
    SleepOnPFHQueue(&WaitForObjDirWritten);

  assert(!"complete");
}

void
DoCheckpointStep(void)
{
  switch (ckptState) {
  default: ;
    assert(false);

  case ckpt_NotActive:
    DEBUG(ckpt) printf("DoCheckpointStep not active\n");
    act_SleepOn(&WaitForCkptNeeded);
    act_Yield();
    
  case ckpt_Phase1:
    DEBUG(ckpt) printf("DoCheckpointStep P1\n");
    DoPhase1Work();
  case ckpt_Phase2:
    DEBUG(ckpt) printf("DoCheckpointStep P2\n");
    DoPhase2Work();
  case ckpt_Phase3:
    DEBUG(ckpt) printf("DoCheckpointStep P3\n");
    DoPhase3Work();
    break;
  }
}

void
CheckpointThread(void)
{
  DEBUG(ckpt) printf("Start Checkpoint thread, act=%#x\n", checkpointActivity);

  Message Msg = {
    .snd_invKey = KR_MigrTool,
    .snd_code = OC_capros_MigratorTool_checkpointStep,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .rcv_limit = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID
  };

  for (;;) {
    CALL(&Msg);
  }
}
