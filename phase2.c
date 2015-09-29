/* ------------------------------------------------------------------------
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
int getNextID();
int getNextSlot();
void static clockHandler(int dev, void *args);
void static diskHandler(int dev, void *args);
void static terminalHandler(int dev, void *args);
static void enableInterrupts();
static void disableInterrupts();
void check_kernel_mode(char* name);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

int idCount = -1;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];
mailSlot MailSlots[MAXSLOTS];
int numBoxes = 10;
int numSlots = 0;
mailbox interruptBox;

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
        MailBoxTable[i].slotSize = -1;
        MailBoxTable[i].headPtr = NULL;
        MailBoxTable[i].blockStatus = 1;
    }

    for (int i = 0; i < MAXSLOTS; i++) {
        MailSlots[i].mboxID = -1;
        MailSlots[i].status = -1;
        MailSlots[i].nextSlot = NULL;
    }

    // Initialize USLOSS_IntVec and system call handlers,
    USLOSS_IntVec[USLOSS_CLOCK_INT] = clockHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = diskHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = terminalHandler;

    // allocate mailboxes for interrupt handlers.  Etc... 
    interruptBox = MailBoxTable[MboxCreate(0, 0)];

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

    // no open mailboxes
    if (ID == -1)
      return -1;

    // initialize Values
    mailbox *mbox = &(MailBoxTable[ID]);
    mbox->mboxID = ID;
    mbox->numSlots = slots;
    mbox->numFullSlots = 0;
    mbox->headPtr = NULL;
    mbox->slotSize = slot_size;
    mbox->blockStatus = NOT_BLOCKED;

    return ID;
} /* MboxCreate */

int MBoxRelease(int mailboxID) {
    check_kernel_mode("MboxRelease");

    return 0;
}

/* ------------------------------------------------------------------------
   Name - getNextID
   Purpose - get next mailbox id
   returns - mbox id or -1 if none are open.
   ----------------------------------------------------------------------- */
int getNextID() {
    idCount++;
    int initialCount = idCount;

    if (idCount > MAXMBOX) {
      idCount = 0;
    }

    // while we are pointing to an already initialized mailbox
    while (MailBoxTable[idCount].mboxID != -1) {
        idCount++;

        // if (we check every slot and none were open)
        if (idCount == initialCount) {
          return -1;
        }

        // wrap arround if at end.
        if (idCount > MAXMBOX) {
          idCount = 0;
        }
    }
    return idCount;
} /* getNextID */

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

    // get the mail box
    mailbox *mbox = &(MailBoxTable[mbox_id]);

    // check message size isn't too big
    if (msg_size > MAX_MESSAGE) {
        return -1;
    }

    // check for free slots
    if (numSlots >= MAXSLOTS) {
        USLOSS_Halt(1);
    }

    // get the slot to put a message in
    numSlots++;
    slotPtr slot = &MailSlots[getNextSlot()];

    return 0;
} /* MboxSend */


/* ------------------------------------------------------------------------
   Name - getNextOpenSlot
   Purpose - find next open mail slot
   Parameters - none
   Returns - the address to the next open mail slot
   ----------------------------------------------------------------------- */
int getNextSlot() {

    return 0;
} /* mailSlot */

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

    mailbox *mbox = &(MailBoxTable[mbox_id]);

    if (msg_size > mbox->slotSize) {
      
    }

    return 0;
} /* MboxReceive */

static void clockHandler(int dev, void *arg) {

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
        USLOSS_Console("%s(): Called while in user mode. Halting...\n", name);
        USLOSS_Halt(1);
    }
}
