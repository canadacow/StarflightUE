#include "patch.h"

#include <stdlib.h>
#include <stdio.h>

#include "../cpu/cpu.h"
// #include "../disasOV/global.h"  // Not needed for UE build

// Wrapper macros for memory access that use global currentMemory
#define Read8_Patch(offset) Read8(currentMemory, offset)
#define Write8_Patch(offset, val) Write8(currentMemory, offset, val)
#define Read16_Patch(offset) Read16(currentMemory, offset)
#define Write16_Patch(offset, val) Write16(currentMemory, offset, val)

#ifdef STARFLT1

void DecryptDictionary(int linkp) {
    if (linkp == 0) return;

    for(int i=0; i<5000; i++) {
        //printf("ßx%04x\n", linkp);
        unsigned char bitfield = Read8_Patch(linkp);
        int length = (bitfield & 0x1F);
        printf("0x%04x %2i '", linkp, length);
        if (length == 0) { // very strange
        } else
            if (length == 1) {
                printf("%c", Read8_Patch(linkp+1)&0x7F);
            } else
            {
                int j;
                for(j=1; j<=length; j++) {
                    unsigned char c = Read8_Patch(linkp+j);
                    unsigned char x = (c ^ 0x7F) & 0x7F;
                    printf("%c", x);
                    Write8_Patch(linkp+j, x);
                    if (j == length+1) exit(1);
                    if ((c & 0x80) != 0) {
                        Write8_Patch(linkp+j, x | 0x80);
                        break;
                    }
                }
                //if (j != length) printf(" <-- wrong");
            }
            //else {
            //}
            printf("'\n");

            linkp = Read16_Patch(linkp-2);
            if (linkp == 0) return;
    }
}
void EnableInterpreter()
{
    // Patch to start Forth interpreter
    Write16_Patch(0x0a53, 0x0000); // BOOT-HOOK

    //Write16_Patch(0x2420, 0x0F22-2); // "0"

    Write16_Patch(0x2420, 0x3a48-2); // "NOP"
    Write16_Patch(0x2422, 0x3a48-2); // "NOP"
    Write16_Patch(0x2424, 0x3a48-2); // "NOP"
    DecryptDictionary(DICTLIST1);
    DecryptDictionary(DICTLIST2);
    DecryptDictionary(DICTLIST3);
    DecryptDictionary(DICTLIST4);
    DecryptDictionary(DICTLIST5);
}

void DisableInterpreterOutput()
{
    Write16_Patch(0x2420+34, 0x3a46); // CR in QUIT word
    Write16_Patch(0x03c3, 0x1692-2); // print "ok"
    Write16_Patch(0x1d3e + 114, 0xe32); // Drop EMIT in (EXPECT)
}

#elif STARFLT2
#error Not supported
#endif
