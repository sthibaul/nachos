// console.cc 
//	Routines to simulate a serial port to a console device.
//	A console has input (a keyboard) and output (a display).
//	These are each simulated by operations on UNIX files.
//	The simulated device is asynchronous,
//	so we have to invoke the interrupt handler (after a simulated
//	delay), to signal that a byte has arrived and/or that a written
//	byte has departed.
//
//  DO NOT CHANGE -- part of the machine emulation
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "console.h"
#include "system.h"
#include <langinfo.h>

// Dummy functions because C++ is weird about pointers to member functions
static void ConsoleReadPoll(void *c) 
{ Console *console = (Console *)c; console->CheckCharAvail(); }
static void ConsoleWriteDone(void *c)
{ Console *console = (Console *)c; console->WriteDone(); }

//----------------------------------------------------------------------
// Console::Console
// 	Initialize the simulation of a hardware console device.
//
//	"readFile" -- UNIX file simulating the keyboard (NULL -> use stdin)
//	"writeFile" -- UNIX file simulating the display (NULL -> use stdout)
// 	"readAvail" is the interrupt handler called when a character arrives
//		from the keyboard
// 	"writeDone" is the interrupt handler called when a character has
//		been output, so that it is ok to request the next char be
//		output
//----------------------------------------------------------------------

Console::Console(const char *readFile, const char *writeFile, VoidFunctionPtr readAvail, 
		VoidFunctionPtr writeDone, void *callArg)
{
    if (readFile == NULL)
	readFileNo = 0;					// keyboard = stdin
    else
    	readFileNo = OpenForReadWrite(readFile, TRUE);	// should be read-only
    if (writeFile == NULL)
	writeFileNo = 1;				// display = stdout
    else
    	writeFileNo = OpenForWrite(writeFile);

    // set up the stuff to emulate asynchronous interrupts
    writeHandler = writeDone;
    readHandler = readAvail;
    handlerArg = callArg;
    putBusy = FALSE;
    incoming = EOF;

    // start polling for incoming packets
    interrupt->Schedule(ConsoleReadPoll, this, ConsoleTime, ConsoleReadInt);
}

//----------------------------------------------------------------------
// Console::~Console
// 	Clean up console emulation
//----------------------------------------------------------------------

Console::~Console()
{
    if (readFileNo != 0)
	Close(readFileNo);
    if (writeFileNo != 1)
	Close(writeFileNo);
}

//----------------------------------------------------------------------
// Console::CheckCharAvail()
// 	Periodically called to check if a character is available for
//	input from the simulated keyboard (eg, has it been typed?).
//
//	Only read it in if there is buffer space for it (if the previous
//	character has been grabbed out of the buffer by the Nachos kernel).
//	Invoke the "read" interrupt handler, once the character has been 
//	put into the buffer. 
//----------------------------------------------------------------------

void
Console::CheckCharAvail()
{
    unsigned char c, d;
    int n;

    // schedule the next time to poll for a packet
    interrupt->Schedule(ConsoleReadPoll, this, ConsoleTime, 
			ConsoleReadInt);

    // do nothing if character is already buffered, or none to be read
    if ((incoming != EOF) || !PollFile(readFileNo))
        return;          

    // otherwise, read character and tell user about it
    n = ReadPartial(readFileNo, &c, sizeof(c));
    if (n == 0) {
        incoming = EOF;
    } else if (strcmp(nl_langinfo(CODESET),"UTF-8")) {
        /* Not UTF-8, assume 8bit locale */
        incoming = c;
    } else
        /* UTF-8, decode */
    if (!(c & 0x80)) {
        /* ASCII */
        incoming = c;
    } else {
        if ((c & 0xe0) != 0xc0)
            /* continuation char or more than three bytes, drop */
            return;
        if (c & 0x1c)
            /* Not latin1, drop */
            return;
        /* latin1 UTF-8 char, read second char */
        n = ReadPartial(readFileNo,  &d, sizeof(d));
        if (n == 0) {
            incoming = EOF;
        } else if ((d & 0xc0) != 0x80) {
            /* Odd, drop */
            return;
        } else {
            incoming = (c & 0x03) << 6 | d;
        }
    }
    stats->numConsoleCharsRead++;
    (*readHandler)(handlerArg);        
}

//----------------------------------------------------------------------
// Console::WriteDone()
// 	Internal routine called when it is time to invoke the interrupt
//	handler to tell the Nachos kernel that the output character has
//	completed.
//----------------------------------------------------------------------

void
Console::WriteDone()
{
    putBusy = FALSE;
    stats->numConsoleCharsWritten++;
    (*writeHandler)(handlerArg);
}

//----------------------------------------------------------------------
// Console::GetChar()
// 	Read a character from the input buffer, if there is any there.
//	Either return the character, or EOF if none buffered.
//----------------------------------------------------------------------

int
Console::GetChar()
{
   int ch = incoming;

   incoming = EOF;
   return ch;
}

//----------------------------------------------------------------------
// Console::PutChar()
// 	Write a character to the simulated display, schedule an interrupt 
//	to occur in the future, and return.
//----------------------------------------------------------------------

void
Console::PutChar(int ch)
{
    unsigned char c;
    ASSERT(putBusy == FALSE);
    if (ch < 0x80 || strcmp(nl_langinfo(CODESET),"UTF-8")) {
	/* Not UTF-8 or ASCII, assume 8bit locale */
	c = ch;
        WriteFile(writeFileNo, &c, sizeof(c));
    } else if (ch < 0x100) {
	/* Non-ASCII UTF-8, thus two bytes */
	c = ((ch & 0xc0) >> 6) | 0xc0;
        WriteFile(writeFileNo, &c, sizeof(c));
	c = (ch & 0x3f) | 0x80;
        WriteFile(writeFileNo, &c, sizeof(c));
    } /* Else not latin1, drop */
    putBusy = TRUE;
    interrupt->Schedule(ConsoleWriteDone, this, ConsoleTime,
					ConsoleWriteInt);
}
