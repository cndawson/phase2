/*------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

  ------------------------------------------------------------------------ */

#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);

void block(int mboxID, int block);
void unblock(int mboxID, int block);
int getNextID();
int getSlot();
void sendToSlot(mailbox *mbox, void *msg_ptr, int msg_size);
void removeSlot(int mboxID);
void static clockHandler(int dev, void *args);
void static diskHandler(int dev, void *args);
void static terminalHandler(int dev, void *args);
static void enableInterrupts();
static void disableInterrupts();
void check_kernel_mode(char* name);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

int numBoxes = 0;
int nextFreeBox = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
mailSlot MailSlots[MAXSLOTS];
mailbox clockBox;
mailbox termBoxes[TERMBOXMAX];
mailbox diskBoxes[DISKBOXMAX];

process processTable[MAXPROC];

int clockTicks = 0;
// also need array of mail slots, array of function ptrs to system call 
// handlers, ...

/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");

    // check we are in kernel
    check_kernel_mode("start1");

    // Disable interrupts
    disableInterrupts();

    // Initialize the mail box table, slots, & other data structures.
    for (int i = 0; i < MAXMBOX; i++) {
        MailBoxTable[i].mboxID = -1;
        MailBoxTable[i].numSlots = -1;
        MailBoxTable[i].numSlotsUsed = -1;
        MailBoxTable[i].slotSize = -1;
        MailBoxTable[i].headPtr = NULL;
        MailBoxTable[i].endPtr = NULL;
        MailBoxTable[i].blockStatus = 1;
    }

    for (int i = 0; i < MAXSLOTS; i++) {
        MailSlots[i].mboxID = -1;
        MailSlots[i].status = -1;
        MailSlots[i].nextSlot = NULL;
        MailSlots[i].message[0] = '\0';
        MailSlots[i].size = -1;
    }

    for (int i = 0; i < MAXPROC; i++) {
        processTable[i].pid = -1;
        processTable[i].blockStatus = NOT_BLOCKED;
        processTable[i].message[0] = '\0';
        processTable[i].size = -1;
        processTable[i].mboxID = -1;
        processTable[i].timeAdded = -1;
    }

    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = terminalHandler;

    // allocate mailboxes for interrupt handlers.  Etc... 
    clockBox = MailBoxTable[MboxCreate(0, 0)];

    for (int i = 0; i < TERMBOXMAX; i++) {
        termBoxes[i] = MailBoxTable[MboxCreate(0, 0)];
    }

    for (int i = 0; i < DISKBOXMAX; i++) {
        diskBoxes[i] = MailBoxTable[MboxCreate(0, 0)];
    }

    // all done creating stuff, re enable interrupts
    enableInterrupts();

    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    int kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    int status;
    if ( join(&status) != kid_pid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }

    return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
                mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    check_kernel_mode("MboxCreate");

    // find an id
    int ID = getNextID();

    // check slot_size isn't too big
    if (slot_size > MAX_MESSAGE) {
        return -1;
    }

    // no open mailboxes
    if (ID == -1) {
        return -1;
    }

    // initialize Values
    mailbox *mbox = &(MailBoxTable[ID]);
    mbox->mboxID = ID;
    mbox->numSlots = slots;
    mbox->numSlotsUsed = 0;
    mbox->headPtr = NULL;
    mbox->slotSize = slot_size;
    mbox->blockStatus = NOT_BLOCKED;

    return ID;
} /* MboxCreate */

int MboxRelease(int mailboxID) {
    check_kernel_mode("MboxRelease");

    // get the mailbox
    mailbox *mbox = &(MailBoxTable[mailboxID]);

    // check to make sure the mail box is in use
    if (mbox->mboxID == -1) {
        return -1;
    }

    // unblock all processes blocked on this mailbox
    for (int i = 0; i < MAXPROC; i++) {
        // find all processes blocked on said mailbox
        if (processTable[i].mboxID == mailboxID) {
            // zap those processes and unblock them
            zap(processTable[i].pid);
            unblockProc(processTable[i].pid);

            // remove their info from the procTable
            processTable[i].pid = -1;
            processTable[i].blockStatus = NOT_BLOCKED;
            processTable[i].message[0] = '\0';
            processTable[i].size = -1;
            processTable[i].mboxID = -1;
            processTable[i].timeAdded = -1;
        }
    }

    // null out all slots used by the mailbox
    slotPtr pre = NULL;
    slotPtr slot = mbox->headPtr;
    while (slot != NULL) {
        slot->mboxID = -1;
        slot->status = -1;
        slot->message[0] = '\0';
        slot->size = -1;

        if (pre != NULL) {
            pre->nextSlot = NULL;
        }

        pre = slot;
        slot = slot->nextSlot;
    }

    // null out the removed mailbox
    MailBoxTable[mailboxID].mboxID = -1;
    MailBoxTable[mailboxID].numSlots = -1;
    MailBoxTable[mailboxID].numSlotsUsed = -1;
    MailBoxTable[mailboxID].slotSize = -1;
    MailBoxTable[mailboxID].headPtr = NULL;
    MailBoxTable[mailboxID].endPtr = NULL;
    MailBoxTable[mailboxID].blockStatus = 1;

    // check if zapped
    if (isZapped()) {
        return -3;
    }

    return 0;
}

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxSend");
    disableInterrupts();

    // get the mail box
    mailbox *mbox = &(MailBoxTable[mbox_id]);

    // check message size isn't too big
    if (msg_size > MAX_MESSAGE) {
        return -1;
    }
    else if (msg_size > mbox->slotSize) {
        return -1;
    }

    // check for 0-slot mbox
    if (mbox->numSlots == 0) {
        unblock(mbox->mboxID, RECEIVEBLOCK);
        
        enableInterrupts();
        return 0;
    }

    // check if no free slots 
    if (mbox->numSlots == mbox->numSlotsUsed) {
        block(mbox->mboxID, SENDBLOCK);
    
        // checked to make sure process wasn't zapped
        if (isZapped()) {
            return -3;
        }

        // check to make mailbox still exists
        if (mbox->mboxID == -1) {
            return -3;
        }
    }

    // write to that slot
    sendToSlot(mbox, msg_ptr, msg_size);
    
    // unblock people waiting on this mailbox
    unblock(mbox->mboxID, RECEIVEBLOCK);

    enableInterrupts();
    return 0;
} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
    check_kernel_mode("MboxReceive");
    disableInterrupts();

    // get the mailbox
    mailbox *mbox = &(MailBoxTable[mbox_id]);

    // check message size isn't too big
    if (msg_size > MAX_MESSAGE) {
        return -1;
    }

    // check for zero slot
    if (mbox->numSlots == 0) {
        block(mbox_id, RECEIVEBLOCK);

        enableInterrupts();
        return 0;
    }

    // check if no messages
    if (mbox->numSlotsUsed == 0) {
        block(mbox_id, RECEIVEBLOCK);
    
        // no longer blocked
        // checked to make sure process wasn't zapped
        if (isZapped()) {
            return -3;
        }

        // check to make sure mailbox still exists
        if (mbox->mboxID == -1) {
            return -3;
        }
    }

    // get the size of the message to save for later
    int size = mbox->headPtr->size;

    // copy information from slot to msg_ptr and remove that slot
    memcpy(msg_ptr, mbox->headPtr->message, msg_size);
    removeSlot(mbox_id);

    // unblock send blocked people
    unblock(mbox_id, SENDBLOCK);

    enableInterrupts();
    return size; 
} /* MboxReceive */

int MboxCondSend(int mailboxID, void *message, int message_size) {
    return 0;
}

void block(int mboxID, int block) {
    // get the correct process from the process table
    process *proc = &processTable[getpid() % 50];

    // add info to that process
    proc->pid = getpid();
    proc->blockStatus = block;
    proc->mboxID = mboxID;
    proc->timeAdded = USLOSS_Clock();

    // actually block
    blockMe(block);
}

void unblock(int mboxID, int block) {
    // multiple processes may be blocked on the same mailbox, we only want to unblock one of them
    // this keeps track of it. Start at -1 incase no one is blocked on that mailbox
    int unblockID = -1;

    // go through the entire proc list and find the process to be unblocked
    for (int i = 0; i < MAXPROC; i++) {
        // find a process blocked on this mailbox and make sure they are the same type of block
        if (processTable[i].mboxID == mboxID && processTable[i].blockStatus == block) {
            // if this is the first process to be found
            if (unblockID == -1) {
                // set the unblockid to this new processes index in the table
                unblockID = i;
            }
            // otherwise
            else {
                // compare the start times of the old process and this new found one
                if (processTable[unblockID].timeAdded > processTable[i].timeAdded) {
                    // if the new found process was blocked before the last found process, swap
                    unblockID = i;
                }
            }
        }
    }

    // check to see if anyone needs to be unblocked
    if (unblockID != -1) {
        // save their pid
        int pid = processTable[unblockID].pid;

        // empty out this slot in the processTable
        processTable[unblockID].pid = -1;
        processTable[unblockID].blockStatus = -1;
        processTable[unblockID].message[0] = '\0';
        processTable[unblockID].size = -1;
        processTable[unblockID].mboxID = -1;
        processTable[unblockID].timeAdded = -1;

        // unblock the process
        unblockProc(pid);
    }
}

/* ------------------------------------------------------------------------
   Name - getNextID
   Purpose - get next mailbox id
   returns - mbox id or -1 if none are open.
   ----------------------------------------------------------------------- */
int getNextID() {
    int initial = nextFreeBox;

    // check for number of boxes in use
    if (numBoxes >= MAXMBOX) {
        return -1;
    }

    // go through all the boxes until you find a free one
    while (MailBoxTable[nextFreeBox].mboxID != -1) {
        nextFreeBox++;

        // if you get to the final box, reset
        if (nextFreeBox == MAXMBOX) {
            nextFreeBox = 0;
        }

        // if you loop all the way around quit
        if (nextFreeBox == initial) {
            return -1;
        }
    }

    // inc numboxes
    numBoxes++;

    return nextFreeBox;
} /* getNextID */

/* ------------------------------------------------------------------------
   Name - getSlot
   Purpose - find next open mail slot
   Parameters - none
   Returns - the address to the next open mail slot
   ----------------------------------------------------------------------- */
int getSlot() {
    int i; // loop variable

    // find a free slot in the slot array
    for (i = 0; i < MAXSLOTS; i++) {
        if (MailSlots[i].status != 1) {
            break;
        }
    }

    // if it got to this point without finding a free slot, halt
    if (i == MAXSLOTS) {
        USLOSS_Console("Out of free mailbox slots.\n");
        USLOSS_Halt(1);
    }

    // return the free slot index
    return i;
} /* mailSlot */

/*
 * Helper function for send
 *  - put message in a slot
 */
void sendToSlot(mailbox *mbox, void *msg_ptr, int msg_size)
{
    // get the slot to insert in
    slotPtr slot = &MailSlots[getSlot()];

    // assign the slot to a mailbox
    // if mailbox is empty, set this slot as the first slot in the box
    if (mbox->headPtr == NULL) {
        mbox->headPtr = slot;
    }
    // otherwise adjust who the old endptr's next is
    else {
        mbox->endPtr->nextSlot = slot;
    }

    // set the box's endptr to this new slot and inc the mailbox's size
    mbox->endPtr = slot;

    // put info in the slot
    slot->mboxID = mbox->mboxID;
    slot->status = 1;
    slot->nextSlot = NULL;
    memcpy(slot->message , msg_ptr, msg_size);
    slot->size = msg_size;

    // increment numSlotsUsed
    mbox->numSlotsUsed++;
}

void removeSlot(int mboxID) {
    // get the mailbox and slot
    mailbox *mbox = &(MailBoxTable[mboxID]);
    slotPtr slot = mbox->headPtr;

    // remove this slot from the mailbox
    mbox->headPtr = slot->nextSlot;

    // if slot was also the endptr, set endptr to null
    if (mbox->headPtr == NULL) {
        mbox->endPtr = NULL;
    }

    // null out the slot
    slot->mboxID = -1;
    slot->status = -1;
    slot->nextSlot = NULL;
    slot->message[0] = '\0';

    // dec how many slots it is using
    mbox->numSlotsUsed--;
}

static void clockHandler(int dev, void *arg) {
    // check if dispatcher should be called
    if (readCurStartTime() >= 80000) {
        timeSlice();
    }

    // inc that a clock interrupt happened
    clockTicks++;

    // every fith interrupt do a conditional send to its mailbox
    if (clockTicks % 5 == 0) {
        MboxCondSend(clockBox.mboxID, NULL, 0);
    }
}

static void diskHandler(int dev, void *arg) {

}

static void terminalHandler(int dev, void *arg) {

}

/*
 * Enables the interrupts 
 */
static void enableInterrupts(int dev, void *args)
{
  /* turn the interrupts ON iff we are in kernel mode */
  if ( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
    //not in kernel mode
    USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
    USLOSS_Console("disable interrupts\n");
    USLOSS_Halt(1);
  } else {
    /* We ARE in kernel mode */
    USLOSS_PsrSet( USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
  }
}

/*
 * Disables the interrupts.
 */
static void disableInterrupts()
{
    /* turn the interrupts OFF iff we are in kernel mode */
    if( (USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0 ) {
        //not in kernel mode
        USLOSS_Console("Kernel Error: Not in kernel mode, may not ");
        USLOSS_Console("disable interrupts\n");
        USLOSS_Halt(1);
    } else
        /* We ARE in kernel mode */
        USLOSS_PsrSet( USLOSS_PsrGet() & ~USLOSS_PSR_CURRENT_INT );
} /* disableInterrupts */

/*
 * Checks if we are in Kernel mode
 */
void check_kernel_mode(char *name) {
    if ((USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()) == 0) {
        USLOSS_Console("%s(): Called while in user mode by process %d. Halting...\n", name, getpid());
        USLOSS_Halt(1);
    }
}
