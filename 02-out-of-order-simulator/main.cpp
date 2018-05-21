/* Assembler for LC */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <iostream>
#define MAXLINELENGTH 1000
#define MAXNUMLABELS 65536
#define MAXLABELLENGTH 7 /* includes the null character termination */

#define ADD 0
#define NAND 1
#define LW 2
#define SW 3
#define BEQ 4
#define MULT 5
#define HALT 6
#define NOOP 7

#define NTAKEN 0    // 2-bit state machine
#define WNTAKEN 1
#define WTAKEN 2
#define TAKEN 3

using namespace std;

int opcoder(int instr){     // renamed for compatibility with smashing the assembler and simulator together :)
    return (instr & 0x1C00000) >> 22;
}


int rRegB(int instr){       // using a separate call for each because there's just five :)
    return (instr & 0x70000) >> 16;
}

int rRegA(int instr){
    return (instr & 0x380000) >> 19;
}


int rDestReg(int instr){
    return (instr & 0x7);
}

// I-Type decoder... needs to cast to short b/c negative numbers
int rOffset(int instr){
    int temp = instr & 0xFFFF;
    short dumb = (short)temp;
    return dumb;
}

// Adding in defines for simulator
#define NUMMEMORY 65536 /* maximum number of data words in memory */
#define NUMREGS 8 /* number of machine registers */
#define NOOPINSTRUCTION 0x1c00000


typedef struct bpStruct {
    int pc[64];  // branch instruction program counter
    int state[64]; // state of the current branch
} bpType;

typedef struct btbStruct { // maps into the BP
    int pc[3]; // source address
    int jump[3]; // where to jump
} btbType;

typedef struct renameStruct {
    int valid[8]; // current register is valid
    int physical[8]; // current mapping from R to P
    int robIndex[8]; // current instruction
} renameType;

typedef struct reorderStruct {
    vector<int> destReg; // architectural R0-R7
    vector<int> destVal; // value for the register
    vector<int> destValid; // 1 if dest has been calculated, 0 otherwise
    vector<int> op; // op code cause i am lazy
    vector<int> destP;
    vector<int> origPC;
} reorderStruct;

typedef struct resEntryStruct {
    int src1P = 0; // src1 physical
    int src2P = 0;
    int destP = 0;
    int src1V = 0; // value of source 1
    int src2V = 0;
    int src1Valid = 0; // is source 1 valid?
    int src2Valid = 0;
    int OP = 0; // opcode
    int pc1 = 0;
    int instr = 0; // too lazy to just pass the offset
    int isUsed = 0; // current entry is actually used
    int rIndex = 0;
} resEntryType;

typedef struct fetchStruct {
    int instr; // instruction to be fetched
    int valid; // current instruction is to be used
    int curPC; // where we at boy
}    fetchType;

typedef struct executeStruct{
    struct resEntryStruct anbEntry;
    struct resEntryStruct lsEntry;
    struct resEntryStruct multEntryA;
    struct resEntryStruct multEntryB;
    struct resEntryStruct multEntryC;
    int anbCounter  = 0, lsCounter = 0, m1Counter = 0, m2Counter = 0, m3Counter = 0, multLast = 0;
}   executeType;


typedef struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
    int cycles; /* number of cycles run so far */
    struct fetchStruct fetch1; // fetch the first intstruction
    struct fetchStruct fetch2; // fetch the second instruction
    int renameCur; // index of current physical register to rename, %32
    struct bpStruct myBP;
    struct btbStruct myBTB;
    struct renameStruct myRenameTable; // an 8 entry renaming table
    std::vector<resEntryStruct> myStations[3];
    struct reorderStruct myROB; // 16 entry buffer
    struct executeStruct myExecute;
} stateType;

struct stateStruct state; // global so we can feed in the instructions
int instrCount = 0;         // num of instructions

// assembler functions
int readAndParse(FILE *, char *, char *, char *, char *, char *);
int translateSymbol(char [MAXNUMLABELS][MAXLABELLENGTH], int labelAddress[], int, char *);
int isNumber(char *);
void testRegArg(char *);
void testAddrArg(char *);

// simulator functions
void run(void);
void printInstruction(int instr);
void printState(stateType *statePtr);
int wordCount = 0;
/* begin code for assembler */
int
main(int argc, char *argv[])
{
    char *inFileString, *outFileString;
    FILE *inFilePtr, *outFilePtr;
    int address;
    char label[MAXLINELENGTH], opcode[MAXLINELENGTH], arg0[MAXLINELENGTH],
    arg1[MAXLINELENGTH], arg2[MAXLINELENGTH], argTmp[MAXLINELENGTH];
    int i;
    int numLabels=0;
    int num;
    int addressField;
    char labelArray[MAXNUMLABELS][MAXLABELLENGTH];
    int labelAddress[MAXNUMLABELS];
    
     if (argc != 3) {
     printf("error: usage: %s <assembly-code-file> <machine-code-file>\n",
     argv[0]);
     exit(1);
     }
     
     inFileString = argv[1];
     outFileString = argv[2];
     
    inFileString = argv[1];
    outFileString = argv[2];
    inFilePtr = fopen(inFileString, "r");
    if (inFilePtr == NULL) {
        printf("error in opening %s\n", inFileString);
        exit(1);
    }
    outFilePtr = fopen(outFileString, "w");
    if (outFilePtr == NULL) {
        printf("error in opening %s\n", outFileString);
        exit(1);
    }
    
    /* map symbols to addresses */
    
    /* assume address start at 0 */
    for (address=0; readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2);
         address++) {
        
        //printf("%d: label=%s, opcode=%s, arg0=%s, arg1=%s, arg2=%s\n",
        // address, label, opcode, arg0, arg1, arg2);
        
        
        /* check for illegal opcode */
        if (strcmp(opcode, "add") && strcmp(opcode, "nand") &&
            strcmp(opcode, "lw") && strcmp(opcode, "sw") &&
            strcmp(opcode, "beq") && strcmp(opcode, "mult") &&
            strcmp(opcode, "halt") && strcmp(opcode, "noop") &&
            strcmp(opcode, ".fill") ) {
            printf("error: unrecognized opcode %s at address %d\n", opcode,
                   address);
            exit(1);
        }
        
        /* check register fields */
        if (!strcmp(opcode, "add") || !strcmp(opcode, "nand") ||
            !strcmp(opcode, "lw") || !strcmp(opcode, "sw") ||
            !strcmp(opcode, "beq") || !strcmp(opcode, "mult")) {
            testRegArg(arg0);
            testRegArg(arg1);
        }
        if (!strcmp(opcode, "add") || !strcmp(opcode, "nand") ||
            !strcmp(opcode, "mult")) {
            testRegArg(arg2);
        }
        
        /* check addressField */
        if (!strcmp(opcode, "lw") || !strcmp(opcode, "sw") ||
            !strcmp(opcode, "beq")) {
            testAddrArg(arg2);
        }
        if (!strcmp(opcode, ".fill")) {
            testAddrArg(arg0);
        }
        
        /* check for enough arguments */
        if ( (strcmp(opcode, "halt") && strcmp(opcode, "noop") &&
              strcmp(opcode, ".fill") && arg2[0]=='\0') ||
            (!strcmp(opcode, ".fill") && arg0[0]=='\0')) {
            printf("error at address %d: not enough arguments\n", address);
            exit(2);
        }
        
        if (label[0] != '\0') {
            /* check for labels that are too long */
            if (strlen(label) >= MAXLABELLENGTH) {
                printf("label too long\n");
                exit(2);
            }
            
            /* make sure label starts with letter */
            if (! sscanf(label, "%[a-zA-Z]", argTmp) ) {
                printf("label doesn't start with letter\n");
                exit(2);
            }
            
            /* make sure label consists of only letters and numbers */
            sscanf(label, "%[a-zA-Z0-9]", argTmp);
            if (strcmp(argTmp, label)) {
                printf("label has character other than letters and numbers\n");
                exit(2);
            }
            
            /* look for duplicate label */
            for (i=0; i<numLabels; i++) {
                if (!strcmp(label, labelArray[i])) {
                    printf("error: duplicate label %s at address %d\n",
                           label, address);
                    exit(1);
                }
            }
            /* see if there are too many labels */
            if (numLabels >= MAXNUMLABELS) {
                printf("error: too many labels (label=%s)\n", label);
                exit(2);
            }
            
            strcpy(labelArray[numLabels], label);
            labelAddress[numLabels++] = address;
        }
    }
    rewind(inFilePtr);
    for (address=0; readAndParse(inFilePtr, label, opcode, arg0, arg1, arg2);
         address++) {
        if (!strcmp(opcode, "add")) {
            num = (ADD << 22) | (atoi(arg0) << 19) | (atoi(arg1) << 16)
            | atoi(arg2);
        } else if (!strcmp(opcode, "nand")) {
            num = (NAND << 22) | (atoi(arg0) << 19) | (atoi(arg1) << 16)
            | atoi(arg2);
        } else if (!strcmp(opcode, "mult")) {
            num = (MULT << 22) | (atoi(arg0) << 19) | (atoi(arg1) << 16)
            | atoi(arg2);
        } else if (!strcmp(opcode, "halt")) {
            num = (HALT << 22);
        } else if (!strcmp(opcode, "noop")) {
            num = (NOOP << 22);
        } else if (!strcmp(opcode, "lw") || !strcmp(opcode, "sw") ||
                   !strcmp(opcode, "beq")) {
            /* if arg2 is symbolic, then translate into an address */
            if (!isNumber(arg2)) {
                addressField = translateSymbol(labelArray, labelAddress,
                                               numLabels, arg2);
                /*
                 printf("%s being translated into %d\n", arg2, addressField);
                 */
                if (!strcmp(opcode, "beq")) {
                    addressField = addressField-address-1;
                }
            } else {
                addressField = atoi(arg2);
            }
            
            
            if (addressField < -32768 || addressField > 32767) {
                printf("error: offset %d out of range\n", addressField);
                exit(1);
            }
            
            /* truncate the offset field, in case it's negative */
            addressField = addressField & 0xFFFF;
            
            if (!strcmp(opcode, "beq")) {
                num = (BEQ << 22) | (atoi(arg0) << 19) | (atoi(arg1) << 16)
                | addressField;
            } else {
                /* lw or sw */
                if (!strcmp(opcode, "lw")) {
                    num = (LW << 22) | (atoi(arg0) << 19) |
                    (atoi(arg1) << 16) | addressField;
                } else {
                    num = (SW << 22) | (atoi(arg0) << 19) |
                    (atoi(arg1) << 16) | addressField;
                }
            }
        } else if (!strcmp(opcode, ".fill")) {
            if (!isNumber(arg0)) {
                num = translateSymbol(labelArray, labelAddress, numLabels,
                                      arg0);
            } else {
                num = atoi(arg0);
            }
            wordCount++;
        }
        state.instrMem[instrCount] = num;
        state.dataMem[instrCount] = num;
        instrCount++;
    }
    // instrCount -= wordCount;
    run(); // assembler stuff is done so lets run the simulator
    return 0;
}

/*
 * Read and parse a line of the assembly-language file.  Fields are returned
 * in label, opcode, arg0, arg1, arg2 (these strings must have memory already
 * allocated to them).
 *
 * Return values:
 *     0 if reached end of file
 *     1 if all went well
 *
 * exit(1) if line is too long.
 */
int
readAndParse(FILE *inFilePtr, char *label, char *opcode, char *arg0,
             char *arg1, char *arg2)
{
    char line[MAXLINELENGTH];
    char *ptr = line;
    
    /* delete prior values */
    label[0] = opcode[0] = arg0[0] = arg1[0] = arg2[0] = '\0';
    
    /* read the line from the assembly-language file */
    if (fgets(line, MAXLINELENGTH, inFilePtr) == NULL) {
        /* reached end of file */
        return(0);
    }
    
    /* check for line too long */
    if (strlen(line) == MAXLINELENGTH-1) {
        printf("error: line too long\n");
        exit(1);
    }
    
    /* is there a label? */
    ptr = line;
    if (sscanf(ptr, "%[^\t\n ]", label)) {
        /* successfully read label; advance pointer over the label */
        ptr += strlen(label);
    }
    
    /*
     * Parse the rest of the line.  Would be nice to have real regular
     * expressions, but scanf will suffice.
     */
    sscanf(ptr, "%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]%*[\t\n\r ]%[^\t\n\r ]",
           opcode, arg0, arg1, arg2);
    return(1);
}

int
translateSymbol(char labelArray[MAXNUMLABELS][MAXLABELLENGTH],
                int labelAddress[MAXNUMLABELS], int numLabels, char *symbol)
{
    int i;
    
    /* search through address label table */
    for (i=0; i<numLabels && strcmp(symbol, labelArray[i]); i++) {
    }
    
    if (i>=numLabels) {
        printf("error: missing label %s\n", symbol);
        exit(1);
    }
    
    return(labelAddress[i]);
}

int
isNumber(char *string)
{
    /* return 1 if string is a number */
    int i;
    return( (sscanf(string, "%d", &i)) == 1);
}

/*
 * Test register argument; make sure it's in range and has no bad characters.
 */
void
testRegArg(char *arg)
{
    int num;
    char c;
    
    if (atoi(arg) < 0 || atoi(arg) > 7) {
        printf("error: register out of range\n");
        exit(2);
    }
    if (sscanf(arg, "%d%c", &num, &c) != 1) {
        printf("bad character in register argument\n");
        exit(2);
    }
}

/*
 * Test addressField argument.
 */
void
testAddrArg(char *arg)
{
    int num;
    char c;
    
    /* test numeric addressField */
    if (isNumber(arg)) {
        if (sscanf(arg, "%d%c", &num, &c) != 1) {
            printf("bad character in addressField\n");
            exit(2);
        }
    }
}
/* End code for assembler */

/* Begin code for simulator */
void run(){
    vector<int> trackJump;
    int fetched = 0;    // dumb stuff we have to keep track of
    int retired = 0;    // number of instructions we retired
    int branches = 0;   // number of branches we took
    int mispred = 0;    // number of times we screwed up
    int jumpCounter = 0;
    int curBP = 0;
    for(int i = 0; i < instrCount; i++){
        cout << state.instrMem[i] << endl;
    }
    cout << endl;
    state.pc = 0;   // initialize the PC and registers to 0
    for(int i = 0; i < NUMREGS; i++){
        state.reg[i] = 0;
    }
    for(int i = 0; i < 8; i++){
        state.myRenameTable.valid[i] = 0; // VALID for the rename table is a zero
        state.myRenameTable.physical[i] = i; // initialize to 0-7
        state.myRenameTable.robIndex[i] = 0; // only matters for invalid entries
    }
    state.renameCur = 8; // next physical register will be 8
    struct stateStruct newState;
    state.renameCur = 0; // current num in rename table %32
    state.fetch1.valid = 0; // we're free to fetch an instruction
    state.fetch2.valid = 0; // we're free to fetch another instruction
    state.myExecute.multLast = 3; // the multiplication unit is free to grab a value
    for(int i = 0; i < 3; i++){ // clear out the table
        state.myBTB.pc[i] = -1;
        state.myBTB.jump[i] = -1;
    }
    for(int i = 0; i < 64; i++){ // clear out the branch predictor as well
        state.myBP.pc[i] = -1;
        state.myBP.state[i] = -1;
    }
    newState = state;
    int resCount[3]; // count the number of things in each station
    // ls, anb, mult
    bool nerdFlag = false; // print a bunch of fun garbage
    newState.renameCur = 8;
    int branchFlag = 0; // is we gonna branch?
    bool firstJump = false;
	
    while (1) {
        firstJump = false;
        branchFlag = 0;
        newState.myRenameTable.valid[0] = 0;
        newState.reg[0] = 0; // always ensure register 0 has a value of 0
        printState(&state); // print the state
        // Fetch instructions from instruction memory if possible
        
        int cur = 0;
        
        
        // COMMIT
        // two oldest entries in ROB may be retired. oldest must retire first
        // branch fixing occurs at retiring by clearing rob, res station, and rename tables
        int robotCounter = 0;
        while(robotCounter < 2){
            if(newState.myROB.destReg.size() > 0){ // make sure we got stuff in our ROBOT
                if(newState.myROB.destValid[0] == 1){ // we valid
                    // cout << "Time to commit an instruction" << endl;
					retired++;
                    if(newState.myROB.op[0] == HALT){
                        std::cout << "PEACE OUT NERDS\n";
						for(int i = 0; i < 8; i++){
							cout << "reg[" << i << "]= " << newState.reg[i] << endl;
						}
						cout << "Final PC: " << newState.pc << endl;
						cout << "Branches: " << branches << endl;
						cout << "Fetched: " << fetched << endl;
						cout << "Mispredictions: " << mispred << endl;
						cout << "Retired: " << retired << endl;
						cout << "Cycle count: " << newState.cycles + 1 << endl;
                        return;
                    }
                    else if(newState.myROB.op[0] == NOOP){
                        // DO NOTHIN
                    }
                    else if(newState.myROB.op[0] != LW && newState.myROB.op[0] != SW && newState.myROB.op[0] != BEQ && newState.myROB.op[0] != NOOP && newState.myROB.op[0] != HALT){ // no need to recommit since these do at execute
                        cout << "Attempting to update after commit for phys: " << newState.myROB.destP[0] << endl;
                        for(int i = 0; i < 8; i++){
                            if(newState.myRenameTable.physical[i] == newState.myROB.destP[0]){
                                newState.myRenameTable.valid[i] = 0;
                            }
                        }
                        newState.reg[newState.myROB.destReg[0]] = newState.myROB.destVal[0]; // update the register value
                    }
                    bool nukeItAll = false;
                    int occ = newState.myROB.op[0];
                    cout << "commiting: " << newState.myROB.destVal[0] <<
                    newState.myROB.destReg[0] << " " << opcoder(newState.myROB.op[0] )<<  endl;
                    if(newState.myROB.op[0] == BEQ){
                        bool misPred = false; // we haven't mispredicted
                        cout << "committing a BEQ  " << endl; // note that we are committing a BEQ
                        int origPC = newState.myROB.origPC[0]; // grab the PC
                        cout << "jump to " << newState.myROB.destVal[0] << endl; // where are we jumping?
                        for(int i = 0; i < 3; i++){ // check our /btb
                            if(newState.myBTB.pc[i] == origPC){ // find our index
                                if(newState.myBTB.jump[i] != newState.myROB.destVal[0]){ // they aint match
                                    cout << "ruh roh, our BTB was wrong, fixing the pc address for next time\n";
                                    mispred++;
                                    nukeItAll = true; // so we needa nuke
                                    newState.myBTB.jump[i] = newState.myROB.destVal[i]; // update
                                }
                                break;
                            }
                        }
                        bool foundBTB = false;
                        for(int i = 0; i < 3; i++){
                            if(newState.myBTB.pc[i] == newState.pc){ // we is found the PC
                                // weesa found da pc
                                cout << "Entry is already in BTB\n";
                                foundBTB = true;
                                newState.pc = newState.myBTB.jump[i]; // set the pc to the jump pc
                                if(i > 0){ // weesa needa do tha swap
                                    
                                    if(i == 1){
                                        // weesa needa switch tha first and second
                                        swap(newState.myBTB.jump[0], newState.myBTB.jump[1]);
                                        swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                                    }
                                    if(i == 2){ // shift them PUPPERS
                                        swap(newState.myBTB.jump[0], newState.myBTB.jump[2]);
                                        swap(newState.myBTB.pc[0], newState.myBTB.pc[2]);
                                        swap(newState.myBTB.jump[1], newState.myBTB.jump[2]);
                                        swap(newState.myBTB.pc[1], newState.myBTB.pc[2]);
                                    }
                                    
                                    
                                    
                                    
                                }
                                
                                
                                break; // NOW BREAK OUTTA HERE
                            }
                        }
                        if(!foundBTB){
                            cout << "looks like we weren't in the BTB, better get in!\n";
                            // oh no! your princess is in another castle
                            newState.myBTB.jump[2] = newState.myBTB.jump[1]; // move the middle right one
                            newState.myBTB.pc[2] = newState.myBTB.pc[1];
                            swap(newState.myBTB.jump[0], newState.myBTB.jump[1]); // move the first guy right one
                            swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                            newState.myBTB.jump[0] = newState.myROB.destVal[0];
                            newState.myBTB.pc[0] = newState.myROB.origPC[0]; // we is finished
                        }
                        
                        
                        for(int i = 0; i < 64; i++){ // check against our branch predictor
                            if(newState.myBP.pc[i] == origPC){ // find the one that is indexed
                                if(newState.myROB.destReg[0] == 1){ // shouldve branched
                                    if(newState.myBP.state[i] < 2){ // we decided not taken
                                        cout << "Our BP was wrong, should have branched\n";
                                        mispred++;
                                        nukeItAll = true; // flag the nuke
                                        newState.myBP.state[i]++; // increment the state
                                    }
                                    else{
                                        if(newState.myBP.state[i] == 2){ // if we're weakly taken, get supah taken
                                            newState.myBP.state[i]++;
                                            cout << "Current BP state is weakly taken, get super taken\n";
                                        }
                                    }
                                }
                                else{ // we shouldn't have branched
                                    if(newState.myBP.state[i] > 1){ // we did branch
                                        cout << "Our BP predicted taken, but we should've used PC+1\n";
                                        mispred++;
                                        nukeItAll = true;
                                        newState.myBP.state[i]--; // decrement to a weaker state
                                    }
                                    else{
                                        if(newState.myBP.state[i] == 1){ // we are weakly not taken
                                            cout << "Change to strongly not taken\n";
                                            newState.myBP.state[i]--; // goto not taken
                                        }
                                    }
                                }
                                
                                break;
                            }
                            
                            
                            
                        }
                        if(nukeItAll){
                            cout << "BOOOOOOOOOOOOOOOOOOOM\n";
                            newState.pc = newState.myROB.destVal[0]; // set our PC to the proper value
                            for(int i = 0; i < 8; i++){
                                newState.myRenameTable.valid[i] = 0; // VALID for the rename table is a zero
                                newState.myRenameTable.physical[i] = i; // initialize to 0-7
                                newState.myRenameTable.robIndex[i] = 0; // only matters for invalid entries
                            }
                            newState.renameCur = 8;
                            newState.myROB.destP.clear();
                            newState.myROB.destVal.clear();
                            newState.myROB.destReg.clear();
                            newState.myROB.destValid.clear();
                            newState.myROB.origPC.clear();
                            newState.myROB.op.clear();
                            newState.myExecute.anbCounter = 0;
                            newState.myExecute.lsCounter = 0;
                            newState.myExecute.m1Counter = 0;
                            newState.myExecute.m2Counter = 0;
                            newState.myExecute.m3Counter = 0;
                            newState.myExecute.multLast = 3;
                            newState.myExecute.anbEntry.isUsed = 0;
                            newState.myExecute.lsEntry.isUsed = 0;
                            newState.myExecute.multEntryA.isUsed = 0;
                            newState.myExecute.multEntryB.isUsed = 0;
                            newState.myExecute.multEntryC.isUsed = 0;
                            robotCounter = 999;
                            
                            
                            
                            
                            
                        }
                        
                        
                    }
                    
                    
                    
                    if(!nukeItAll){
                        
                        newState.myROB.destVal.erase(newState.myROB.destVal.begin()); // nuke the first item
                        newState.myROB.destValid.erase(newState.myROB.destValid.begin());
                        newState.myROB.destReg.erase(newState.myROB.destReg.begin());
                        newState.myROB.op.erase(newState.myROB.op.begin());
                        newState.myROB.destP.erase(newState.myROB.destP.begin());
                        newState.myROB.origPC.erase(newState.myROB.origPC.begin());
                        robotCounter++;
                        for(int i = 0; i < 3; i++){
                            for(int j = 0; j < newState.myStations[i].size(); j++){
                                newState.myStations[i][j].rIndex--; // decrement the index
                            }
                        }
                        newState.myExecute.lsEntry.rIndex--; // decrement as well
                        newState.myExecute.anbEntry.rIndex--; // note
                        newState.myExecute.multEntryA.rIndex--; // to
                        newState.myExecute.multEntryB.rIndex--; // self
                        newState.myExecute.multEntryC.rIndex--; // next time use a circular queue :)
                    }
                    
                    // MAYBE I SHOUDLVE USED A CIRCULAR QUEUE LMAO
                    
                    
                    
                    
                    
                    
                    
                    
                }
                else{
                    robotCounter = 42;
                    break;
                }
                
            }
            else{
                robotCounter = 42;
                break;
            }
        }
        
        
        
        
        
        
        // ADD NAND BEQ may be dealt with within the cycle
        while(cur < newState.myStations[1].size()){ // worry about the ADD, NAND, BEQ stuff first
            //  if(newState.myStations[1][cur].isUsed){ // see if cur is used
            //   cout << "for instr " << newState.myStations[1][cur].instr << " "
            // << newState.myStations[1][cur].src1P << " " << newState.myStations[1][cur].src2P << endl;
            if(newState.myStations[1][cur].src1Valid == 1 && newState.myStations[1][cur].src2Valid == 1){
                // cout << newState.myStations[1][cur].src1P << " " << newState.myStations[1][cur].src2P;
                //   cout << "found instruction for add/beq/nand: " << newState.myStations[1][cur].instr << endl;
                resEntryStruct temp = newState.myStations[1][cur];
                newState.myStations[1].erase(newState.myStations[1].begin() + cur);
                int ret = 0;
                branchFlag = 0;
                if(temp.OP == ADD){
                    ret = temp.src1V + temp.src2V;
                    cout << "adding " << temp.src1V << " + " << temp.src2V << " = " << ret << endl;
                }
                else if(temp.OP == NAND){
                    ret = ~(temp.src1V & temp.src2V);
                }
                else if (temp.OP == BEQ){
                    cout << "comparing registers " << temp.src1P << " " << temp.src2P <<
                    " with values " << temp.src1V << " " << temp.src2V << endl;
                    if(temp.src1V == temp.src2V){
                        branchFlag = 1;
                        
                        ret = temp.pc1 + rOffset(temp.instr);
                        cout << temp.pc1 << " for branch and " << rOffset(temp.instr) << " for offset" << endl;
                    }
                    else{
                        cout << "no branch, just: " << temp.pc1 << endl;
                        ret = temp.pc1;
                    }
                }
                cur = 3; // break out
                
                if(temp.OP != BEQ){
                    int rInd = temp.rIndex; // index into the ROB
                    newState.myROB.destVal[rInd] = ret; // write the value
                    newState.myROB.destValid[rInd] = 1; // flag as done
                    int phyInd = temp.destP;
                    for(int i = 0; i < newState.myStations[0].size(); i++){
                        if(newState.myStations[0][i].src1P == phyInd){
                            newState.myStations[0][i].src1V = ret;
                            newState.myStations[0][i].src1Valid = 1;
                        }
                        if(newState.myStations[0][i].src2P == phyInd){
                            newState.myStations[0][i].src2V = ret;
                            newState.myStations[0][i].src2Valid = 1;
                        }
                    }
                    for(int i = 0; i < newState.myStations[1].size(); i++){
                        if(newState.myStations[1][i].src1P == phyInd){
                            newState.myStations[1][i].src1V = ret;
                            newState.myStations[1][i].src1Valid = 1;
                        }
                        if(newState.myStations[1][i].src2P == phyInd){
                            newState.myStations[1][i].src2V = ret;
                            newState.myStations[1][i].src2Valid = 1;
                        }
                    }
                    for(int i = 0; i < newState.myStations[2].size(); i++){
                        if(newState.myStations[2][i].src1P == phyInd){
                            newState.myStations[2][i].src1V = ret;
                            newState.myStations[2][i].src1Valid = 1;
                        }
                        if(newState.myStations[2][i].src2P == phyInd){
                            newState.myStations[2][i].src2V = ret;
                            newState.myStations[2][i].src2Valid = 1;
                        }
                    }
                }
                
                if(temp.OP == BEQ){
                    int rInd = temp.rIndex; // index into the ROB
                    newState.myROB.destVal[rInd] = ret; // write the value
                    newState.myROB.destValid[rInd] = 1; // flag as done
                    newState.myROB.destReg[rInd] = branchFlag; // we should have branched
                    newState.myROB.origPC[rInd] = temp.pc1 - 1;
                }
                
                cur = 11;
            }
            //   }
            cur++;
        }
        
        cur = 0; // l o l
        
        if(newState.myExecute.lsCounter > 0){ // we in the middle of an execution at the moment!!!
            newState.myExecute.lsCounter++; // increment the counter
            //  cout << "Incrementing ls counter: " << newState.myExecute.lsCounter << endl;
            if(newState.myExecute.lsCounter == 4 ){ // we done did reached the cycle number count
                int temp = newState.dataMem[newState.myExecute.lsEntry.src1V + rOffset(newState.myExecute.lsEntry.instr)];
                if(newState.myExecute.lsEntry.OP == LW){ // we is doin a load word so we gotta load the word from the memory
                    newState.reg[rRegB(newState.myExecute.lsEntry.instr)] = temp;
                }
                else{ // we is doin a store word so store the value from the register into that there data memory
                    newState.dataMem[newState.myExecute.lsEntry.src1V + rOffset(newState.myExecute.lsEntry.instr)] = newState.reg[rRegB(newState.myExecute.lsEntry.instr)];
                    temp = newState.reg[rRegB(newState.myExecute.lsEntry.instr)];
                }
                
                int phyInd = newState.myExecute.lsEntry.destP;
                // cout << "Checking phys index " << phyInd << " against ";
                for(int i = 0; i < 8; i++){
                    // cout << newState.myRenameTable.physical[i] << " ";
                    if(newState.myRenameTable.physical[i] == phyInd){
                        newState.myRenameTable.valid[i] = 0;
                    }
                }
                //cout << endl;
                for(int i = 0; i < newState.myStations[0].size(); i++){
                    if(newState.myStations[0][i].src1P == phyInd){
                        newState.myStations[0][i].src1V = temp;
                        newState.myStations[0][i].src1Valid = 1;
                    }
                    if(newState.myStations[0][i].src2P == phyInd){
                        newState.myStations[0][i].src2V = temp;
                        newState.myStations[0][i].src2Valid = 1;
                    }
                }
                for(int i = 0; i < newState.myStations[1].size(); i++){
                    if(newState.myStations[1][i].src1P == phyInd){
                        newState.myStations[1][i].src1V = temp;
                        newState.myStations[1][i].src1Valid = 1;
                    }
                    if(newState.myStations[1][i].src2P == phyInd){
                        newState.myStations[1][i].src2V = temp;
                        newState.myStations[1][i].src2Valid = 1;
                    }
                }
                for(int i = 0; i < newState.myStations[2].size(); i++){
                    if(newState.myStations[2][i].src1P == phyInd){
                        newState.myStations[2][i].src1V = temp;
                        newState.myStations[2][i].src1Valid = 1;
                    }
                    if(newState.myStations[2][i].src2P == phyInd){
                        newState.myStations[2][i].src2V = temp;
                        newState.myStations[2][i].src2Valid = 1;
                    }
                }
                
                int rInd = newState.myExecute.lsEntry.rIndex;
                newState.myROB.destVal[rInd] = temp; // write the value
                newState.myROB.destValid[rInd] = 1; // flag as done
                newState.myExecute.lsEntry.isUsed = 0;
                newState.myExecute.lsCounter = 0; // we is done doin stuff with our instruction so we can reset that counter
                if(nerdFlag){
                    cout << "RETIRED A LOAD STORE INSTRUCTION\n";
                }
            }
        }
        else{ // we is tryna find a load word or a store word
            cur = 0; // set cur equal to 0 again just to be safe this line of code is 100000% pointless as is this comment
            while(cur < newState.myStations[0].size()){ // we still got more stations to check for stuff
                if(newState.myStations[0][cur].isUsed){ // we found a thingy that is an entry
                    if((newState.myStations[0][cur].src1Valid == 1 && newState.myStations[0][cur].src2Valid == 1)
                       || (newState.myStations[0][cur].src1Valid == 1 && opcoder(newState.myStations[0][cur].instr) == LW))
                    { // we found something we can do stuff to
                        //   cout << "Found a l/s instruction to work on\n";
                        newState.myExecute.lsCounter++; // increment dat counter
                        resEntryStruct temp = newState.myStations[0][cur]; // put the entry into the execute from the station
                        newState.myExecute.lsEntry = temp;
                        newState.myStations[0].erase(newState.myStations[0].begin() + cur); // now erase the entry from the res station
                        cur = 10; // exit the loop through the power of code
                    }
                }
                cur++;
            }
            
        }
        cur  = 0; // setting the variable to 0 because why not, can you tell im just typing comments instead of actually programming?
        
        
        
        
        
        
        
        // work through the current multiplication units
        if(newState.myExecute.m1Counter > 0){ // the current mult pipeline thing is in the middle of executing
            newState.myExecute.m1Counter++; // increment its counter
            cout << "MultPipeline1 incremented to " << newState.myExecute.m1Counter << endl;
            if(newState.myExecute.m1Counter > 7 ){ // check if it do be do be dooo ba peeeeeerrrrryyy the platypus
                int ret = 0; // using an integer to store the value cause i aint implemented the ROB yet
                ret = newState.myExecute.multEntryA.src1V * newState.myExecute.multEntryA.src2V; // multiply them numbers together
                newState.myExecute.multEntryA.isUsed = 0; // we is done with this functional unit thing so flag it as such
                newState.myExecute.m1Counter = 0;
                
                int phyInd = newState.myExecute.multEntryA.destP;
                int rInd = newState.myExecute.multEntryA.rIndex;
                for(int j = 0; j < 3; j++){
                    for(int i = 0; i < newState.myStations[j].size(); i++){
                        if(newState.myStations[j][i].src1P == phyInd){
                            newState.myStations[j][i].src1V = ret;
                            newState.myStations[j][i].src1Valid = 1;
                        }
                        if(newState.myStations[j][i].src2P == phyInd){
                            newState.myStations[j][i].src2V = ret;
                            newState.myStations[j][i].src2Valid = 1;
                        }
                    }
                }
                newState.myROB.destVal[rInd] = ret; // write the value
                newState.myROB.destValid[rInd] = 1; // flag as done
                
                
            }
        }
        
        if(newState.myExecute.m2Counter > 0){
            newState.myExecute.m2Counter++;
            if(newState.myExecute.m2Counter > 7 ){
                int ret = 0;
                ret = newState.myExecute.multEntryB.src1V * newState.myExecute.multEntryB.src2V;
                newState.myExecute.multEntryB.isUsed = 0;
                newState.myExecute.m2Counter = 0;
                int phyInd = newState.myExecute.multEntryB.destP;
                int rInd = newState.myExecute.multEntryB.rIndex;
                for(int j = 0; j < 3; j++){
                    for(int i = 0; i < newState.myStations[j].size(); i++){
                        if(newState.myStations[j][i].src1P == phyInd){
                            newState.myStations[j][i].src1V = ret;
                            newState.myStations[j][i].src1Valid = 1;
                        }
                        if(newState.myStations[j][i].src2P == phyInd){
                            newState.myStations[j][i].src2V = ret;
                            newState.myStations[j][i].src2Valid = 1;
                        }
                    }
                }
                newState.myROB.destVal[rInd] = ret; // write the value
                newState.myROB.destValid[rInd] = 1; // flag as done
            }
        }
        
        if(newState.myExecute.m3Counter > 0){
            newState.myExecute.m3Counter++;
            if(newState.myExecute.m3Counter > 7 ){
                int ret = 0;
                ret = newState.myExecute.multEntryC.src1V * newState.myExecute.multEntryC.src2V;
                newState.myExecute.multEntryC.isUsed = 0;
                newState.myExecute.m3Counter = 0;
                int phyInd = newState.myExecute.multEntryC.destP;
                int rInd = newState.myExecute.multEntryC.rIndex;
                for(int j = 0; j < 3; j++){
                    for(int i = 0; i < newState.myStations[j].size(); i++){
                        if(newState.myStations[j][i].src1P == phyInd){
                            newState.myStations[j][i].src1V = ret;
                            newState.myStations[j][i].src1Valid = 1;
                        }
                        if(newState.myStations[j][i].src2P == phyInd){
                            newState.myStations[j][i].src2V = ret;
                            newState.myStations[j][i].src2Valid = 1;
                        }
                    }
                }
                newState.myROB.destVal[rInd] = ret; // write the value
                newState.myROB.destValid[rInd] = 1; // flag as done
            }
        }
        
        // WE IS DONE WORKING ON THE MULTIPLICATION UNITS NOW SO WE CAN SHIFT THEM IF WE NEEDA SHIFT EM
        /*
         if(!(newState.myExecute.multEntryA.isUsed) && newState.myExecute.multEntryB.isUsed){ // shift that first one if we gotta
         newState.myExecute.multEntryA = newState.myExecute.multEntryB;
         newState.myExecute.m1Counter = newState.myExecute.m2Counter;
         newState.myExecute.multEntryB.isUsed = 0;
         newState.myExecute.m2Counter = 0;
         }
         if(!(newState.myExecute.multEntryB.isUsed) && newState.myExecute.multEntryC.isUsed){ // now shift the second one too if we gotta
         newState.myExecute.multEntryB = newState.myExecute.multEntryC;
         newState.myExecute.m2Counter = newState.myExecute.m3Counter;
         newState.myExecute.multEntryC.isUsed = 0;
         newState.myExecute.m3Counter = 0;
         }
         */
        // WE CANT SHIFT THE THIRD CAUSE THERE AINT NO FOURTH
        newState.myExecute.multLast++;
        if(newState.myExecute.multLast > 2){ // we got to a point where can pipeline dat stuff so lets check if we can chuck anything at the exeggutor
            
            
            while(cur < newState.myStations[2].size()){ // check our multiplication reservations
                if(newState.myStations[2][cur].isUsed){ // we found a thingy that is an entry
                    if(newState.myStations[2][cur].src1Valid == 1 && newState.myStations[2][cur].src2Valid == 1){
                        //    cout << "Found a multiply to commit\n";
                        resEntryStruct * mrPointerMan;
                        if(newState.myExecute.multEntryA.isUsed == 0){ // we can push to the first multiplication thing
                            mrPointerMan = &(newState.myExecute.multEntryA);
                            newState.myExecute.m1Counter++;
                            newState.myExecute.multEntryA.isUsed = 1;
                        }
                        else if(newState.myExecute.multEntryB.isUsed == 0){ // we can push to the second multiplication thing
                            mrPointerMan = &(newState.myExecute.multEntryB);
                            newState.myExecute.m2Counter++;
                            newState.myExecute.multEntryB.isUsed = 1;
                        }
                        else if ( newState.myExecute.multEntryC.isUsed == 0){ // we can push to the last lame multiplication thing
                            mrPointerMan = &(newState.myExecute.multEntryC);
                            newState.myExecute.m3Counter++;
                            newState.myExecute.multEntryC.isUsed = 1;
                        }
                        cur = 0;
                        if(mrPointerMan == NULL){
                            
                        }
                        resEntryStruct temp = newState.myStations[2][cur];
                        //   cout << "phys dest for found mult: " << newState.myStations[2][cur].destP << endl;
                        *mrPointerMan = temp;
                        newState.myExecute.multLast = 0;
                        newState.myStations[2].erase(newState.myStations[2].begin() + cur);
                        cur = 8494; // math
                        break;
                    }
                }
                cur++;
            }
            
        }
        
        
        
        if(newState.fetch1.valid == 0 && newState.pc < instrCount){ // no entry from previous for first instruction
            newState.fetch1.valid = 1; // flag the instruction as stored
            newState.fetch1.instr = state.instrMem[newState.pc]; // store the instruction
            newState.fetch1.curPC = newState.pc; // log the current PC
            
            
            bool isBranch = false; // we aren't a branch yet
            if(opcoder(newState.fetch1.instr) == BEQ){
				branches++;
                isBranch = true;
                int index = 0;
                trackJump.emplace_back(newState.pc); // store current pc in this gizmo
                bool foundBP = false;
                int i;
                for(i  = 0; i < 64; i++){ // index into our BP
                    if(newState.myBP.pc[i] == newState.pc){ // we found the current PC
                        cout << "--- Branch Predictor Hit ---\n";
                        index = i;
                        foundBP = true; // we got a hit
                        break;
                    }
                    if(newState.myBP.pc[i] == -1){ // we found an unused entry
                        cout << "---indexed out of current BP slots---\n";
                        break;
                    }
                }
                if(!foundBP){ // we werent in the BP, so allocate into it
                    // we isn't found so we need to allocate an entry
                    cout << "No entry found, creating one at index " << curBP << " for BP\n";
                    newState.myBP.pc[curBP] = newState.pc; // intialize to the current PC
                    newState.myBP.state[curBP] = WNTAKEN; // and to weakly not taken
                    index = curBP; // keep track of the index
                    curBP = (curBP + 1) % 64; // using fifo
                }
                
                // are we taking the branch or not?
                if(newState.myBP.state[index] > 1){ // we is takin da branch
                    cout << "BP reports take the branch\n";
                    firstJump = true;
                    bool foundBTB = false; // look for a BTB entry
                    for(int i = 0; i < 3; i++){
                        if(newState.myBTB.pc[i] == newState.pc){ // we is found the PC
                            // weesa found da pc
                            foundBTB = true;
                            isBranch = true;
                            newState.pc = newState.myBTB.jump[i]; // set the pc to the jump pc
                            if(i > 0){ // weesa needa do tha swap
                                if(i == 1){
                                    // weesa needa switch tha first and second
                                    swap(newState.myBTB.jump[0], newState.myBTB.jump[1]);
                                    swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                                }
                                if(i == 2){ // shift them PUPPERS
                                    swap(newState.myBTB.jump[0], newState.myBTB.jump[2]);
                                    swap(newState.myBTB.pc[0], newState.myBTB.pc[2]);
                                    swap(newState.myBTB.jump[1], newState.myBTB.jump[2]);
                                    swap(newState.myBTB.pc[1], newState.myBTB.pc[2]);
                                }
                            }
                            
                            
                            break; // NOW BREAK OUTTA HERE
                        }
                        
                    }
                    if(!foundBTB){
                        // oh no! your princess is in another castle
                        cout << "Current PC not in BTB, add it to it and fetch speculatively from PC+1\n";
                        newState.myBTB.jump[2] = newState.myBTB.jump[1]; // move the middle right one
                        newState.myBTB.pc[2] = newState.myBTB.pc[1];
                        swap(newState.myBTB.jump[0], newState.myBTB.jump[1]); // move the first guy right one
                        swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                        newState.myBTB.jump[0] = newState.pc + 1;
                        newState.myBTB.pc[0] = newState.pc; // we is finished
                        newState.pc += 1;
                    }
                    
                    
                    
                    
                }
                else{
                    isBranch = false; // no branching, just fetch from pc+1
                }
                
                
                
                
                
            }
            
            
            
            
            
            if(!isBranch){
                newState.pc++;
            }
            /*
             if(nerdFlag){
             cout << "\nfetched " << newState.fetch1.instr << endl;
             cout << "\nCurrent PC  " << newState.pc << endl;
             cout << "Reg Renamed Validity: ";
             
             for(int i = 0; i < 8; i++){
             cout << newState.myRenameTable.valid[i] << "|";
             }
             cout << endl;
             for(int i = 0; i < 8; i++){
             cout << newState.myRenameTable.physical[i] << "|";
             }
             }
             */
            fetched++;
            
        }
        
        // *** NEED TO IMPLEMENT BEQ CHECK FOR THE SECOND INSTRUCTION ***
        if(newState.fetch2.valid == 0 && opcoder(newState.fetch1.instr) != BEQ && newState.pc < instrCount && !firstJump){ // no entry from previous stored in second fetch
            newState.fetch2.valid = 1; // flag the instruction as stored
            newState.fetch2.instr = state.instrMem[newState.pc]; // store the instruction
            newState.fetch2.curPC = newState.pc;
            bool isBranch = false;
            
            if(opcoder(newState.fetch2.instr) == BEQ){
				branches++;
                isBranch = true;
                int index = 0;
                bool foundBP = false;
                int i;
                for(i  = 0; i < 64; i++){ // index into our BP
                    if(newState.myBP.pc[i] == newState.pc){
                        foundBP = true; // we got a hit
                        cout << "Found a BP hit\n";
                        index = i;
                        break;
                    }
                    if(newState.myBP.pc[i] == -1){
                        cout << "Reached end of BP\n";
                        break;
                    }
                }
                if(!foundBP){
                    // we isn't found so we need to allocate an entry
                    cout << "Not allocated in BP, allocating at " << curBP << "\n";
                    newState.myBP.pc[curBP] = newState.pc; // intialize to the current PC
                    newState.myBP.state[curBP] = WNTAKEN; // and to weakly not taken
                    index = curBP;
                    curBP = (curBP + 1) % 64; // using fifo
                }
                
                // are we taking the branch or not?
                if(newState.myBP.state[index] > 1){ // we is takin da branch
                    cout << "Branch Predicted to be taken...\n";
                    bool foundBTB = false;
                    for(int i = 0; i < 3; i++){
                        if(newState.myBTB.pc[i] == newState.pc){ // we is found the PC
                            // weesa found da pc
                            cout << "Entry is already in BTB\n";
                            foundBTB = true;
                            newState.pc = newState.myBTB.jump[i]; // set the pc to the jump pc
                            if(i > 0){ // weesa needa do tha swap
                                
                                if(i == 1){
                                    // weesa needa switch tha first and second
                                    swap(newState.myBTB.jump[0], newState.myBTB.jump[1]);
                                    swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                                }
                                if(i == 2){ // shift them PUPPERS
                                    swap(newState.myBTB.jump[0], newState.myBTB.jump[2]);
                                    swap(newState.myBTB.pc[0], newState.myBTB.pc[2]);
                                    swap(newState.myBTB.jump[1], newState.myBTB.jump[2]);
                                    swap(newState.myBTB.pc[1], newState.myBTB.pc[2]);
                                }
                                
                                
                                
                                
                            }
                            
                            
                            break; // NOW BREAK OUTTA HERE
                        }
                        
                    }
                    if(!foundBTB){
                        cout << "Oh no dawg, your princess is in another castle - Adding entry to BTB\n";
                        // oh no! your princess is in another castle
                        newState.myBTB.jump[2] = newState.myBTB.jump[1]; // move the middle right one
                        newState.myBTB.pc[2] = newState.myBTB.pc[1];
                        swap(newState.myBTB.jump[0], newState.myBTB.jump[1]); // move the first guy right one
                        swap(newState.myBTB.pc[0], newState.myBTB.pc[1]);
                        newState.myBTB.jump[0] = newState.pc + 1;
                        newState.myBTB.pc[0] = newState.pc; // we is finished
                        newState.pc += 1;
                    }
                    
                    
                    
                    
                }
                else{
                    isBranch = false;
                }
                
                
                
                
                
            }
            
            
            
            
            if(!isBranch){
                newState.pc++;
            }
            fetched++;
            /*
             if(nerdFlag){
             cout << "\nfetched " << newState.fetch2.instr << endl;
             cout << "\nCurrent PC  " << newState.pc << endl;
             cout << "Reg Renamed Validity: ";
             
             for(int i = 0; i < 8; i++){
             cout << newState.myRenameTable.valid[i] << "|";
             }
             cout << endl;
             for(int i = 0; i < 8; i++){
             cout << newState.myRenameTable.physical[i] << "|";
             }
             
             cout << endl;
             }
             */
        }
        
        for(int i = 0; i < 3; i++){ // set our counters to 0 for each res station.. get the entries in each
            resCount[i] = newState.myStations[i].size(); // 0 for ls, 1 for anb, 2 for mult
        }
        
        
        // no stations are full, so we may proceed with RENAMING
        if(resCount[0] < 3 && resCount[1] < 3 && resCount[2] < 3 && newState.myROB.destReg.size() < 16){
            if(newState.fetch1.valid == 1){ // we got an instruction to fetch???
                
                if(nerdFlag){
                    cout << "\nrenaming  " << newState.fetch1.instr << " as P" << newState.renameCur << endl;
                }
                int ind; // which station we needa goto
                // put the first fetched instruction into the rename table
                int oc = opcoder(newState.fetch1.instr); // get the opcode of the instruction
                
                if(oc == LW || oc == SW){
                    ind = 0;
                }
                else if(oc == ADD || oc == NAND || oc == BEQ){
                    ind = 1;
                }
                else{
                    ind = 2;
                }
                int d; // where we goin ???
                if(oc != HALT && oc != NOOP){
                    resEntryStruct temp;
                    newState.myStations[ind].emplace_back(temp);
                    newState.myStations[ind][resCount[ind]].src1P = newState.myRenameTable.physical[rRegA(newState.fetch1.instr)];
                    newState.myStations[ind][resCount[ind]].src2P = newState.myRenameTable.physical[rRegB(newState.fetch1.instr)];
                    newState.myStations[ind][resCount[ind]].src1Valid = !(newState.myRenameTable.valid[rRegA(newState.fetch1.instr)]);
                    newState.myStations[ind][resCount[ind]].src2Valid = !(newState.myRenameTable.valid[rRegB(newState.fetch1.instr)]);
                    if(newState.myStations[ind][resCount[ind]].src1Valid == 1){
                        newState.myStations[ind][resCount[ind]].src1V = newState.reg[rRegA(newState.fetch1.instr)];
                    }
                    if(newState.myStations[ind][resCount[ind]].src2Valid == 1){
                        newState.myStations[ind][resCount[ind]].src2V = newState.reg[rRegB(newState.fetch1.instr)];
                    }
                }
                int safety = 0;
					for(int i = 0; i < 8; i++){
						if(newState.myRenameTable.physical[i] == newState.renameCur && i != newState.renameCur){
							safety = 1;
						}
					}
                
                if(opcoder(newState.fetch1.instr) != LW && opcoder(newState.fetch1.instr) != NOOP && opcoder(newState.fetch1.instr) != HALT && opcoder(newState.fetch1.instr) != BEQ){
                    d = rDestReg(newState.fetch1.instr); // get the destination register
                    newState.myRenameTable.physical[d] = newState.renameCur + safety; // physical register
                    cout << "Renamed reg " << d << " to " << newState.myRenameTable.physical[d] << endl;
					
                    newState.renameCur = (newState.renameCur  + safety) % 32; // modularize that stuff because math
                    //newState.myRenameTable.valid[d] = 1; // set the rename entry for our architectural register and flag it as invalid
                    newState.myRenameTable.robIndex[d] = newState.myROB.destVal.size();
                }
                else if(opcoder(newState.fetch1.instr) != NOOP && opcoder(newState.fetch1.instr) != HALT && opcoder(newState.fetch1.instr) != BEQ){
                    d = rRegB(newState.fetch1.instr);
                    newState.myRenameTable.physical[d] = newState.renameCur + safety;
                    newState.renameCur = (newState.renameCur  + safety) % 32;
                    // newState.myRenameTable.valid[d] = 1; // flag it as invalid, thanks for making 1 be false dr west :|
                    newState.myRenameTable.robIndex[d] = newState.myROB.destVal.size();
                }
                else{
                    d = 0; // get the destination register
                    // newState.myRenameTable.physical[d] = newState.renameCur; // physical register
                    //  newState.renameCur = (newState.renameCur + 1) % 32; // modularize that stuff because math
                    //newState.myRenameTable.valid[d] = 1; // set the rename entry for our architectural register and flag it as invalid
                    //  newState.myRenameTable.robIndex[d] = newState.myROB.destVal.size();
                }
                if(oc != HALT && oc != NOOP){
                    
                    newState.myStations[ind][resCount[ind]].rIndex = newState.myROB.destVal.size();
                    
                    newState.myStations[ind][resCount[ind]].pc1 = newState.fetch1.curPC + 1;
                    newState.myStations[ind][resCount[ind]].instr = newState.fetch1.instr;
                    
                    newState.myStations[ind][resCount[ind]].destP = newState.myRenameTable.physical[rDestReg(newState.fetch1.instr)];
                    if(oc == LW || oc == SW){
                        newState.myStations[ind][resCount[ind]].destP = newState.myRenameTable.physical[rRegB(newState.fetch1.instr)];
                    }
                    newState.myStations[ind][resCount[ind]].OP = opcoder(newState.fetch1.instr);
                    
                    newState.myRenameTable.valid[d] = 1;
                    newState.myStations[ind][resCount[ind]].isUsed = true;
                    int stupid = newState.myROB.destReg.size();
                    newState.myROB.destReg.emplace_back(0);
                    newState.myROB.destVal.emplace_back(0);
                    newState.myROB.destValid.emplace_back(0);
                    newState.myROB.origPC.emplace_back(0);
                    newState.myROB.destP.emplace_back(0);
                    newState.myROB.op.emplace_back(0);
                    newState.myROB.destReg[stupid] = d;
                    newState.myROB.destVal[stupid] = newState.myRenameTable.physical[d];
                    newState.myROB.destValid[stupid] = 0;
                    newState.myROB.op[stupid] = oc;
                    newState.myROB.destP[stupid] = newState.myStations[ind][resCount[ind]].destP;
                    newState.myROB.origPC[stupid] = newState.myStations[ind][resCount[ind]].pc1 - 1;
                }
                else{
                    int stupid = newState.myROB.destReg.size();
                    newState.myROB.destReg.emplace_back(0);
                    newState.myROB.destVal.emplace_back(0);
                    newState.myROB.destValid.emplace_back(0);
                    newState.myROB.destP.emplace_back(0);
                    newState.myROB.op.emplace_back(0);
                    newState.myROB.origPC.emplace_back(0);
                    newState.myROB.destReg[stupid] = 0;
                    newState.myROB.destVal[stupid] = 0;
                    newState.myROB.destValid[stupid] = 1;
                    newState.myROB.op[stupid] = oc; // we're a noop or halt
                    newState.myROB.destP[stupid] = newState.myRenameTable.physical[d];
                    newState.myROB.origPC[stupid] = 9;
                }
                newState.fetch1.valid = 0;
                newState.fetch1.instr = NOOPINSTRUCTION;
            }
            for(int i = 0; i < 3; i++){ // set our counters to 0 for each res station
                resCount[i] = newState.myStations[i].size(); // 0 for ls, 1 for anb, 2 for mult
            }
            
			
			
			
			// add one to size check to account for the commit
            if(opcoder(newState.fetch1.instr) != BEQ && resCount[0] < 3 & resCount[1] < 3 && resCount[2] < 3 && newState.myROB.destReg.size()  < 16){
                if(newState.fetch2.valid == 1){
                    
                    if(nerdFlag){
                        cout << "\nrenaming  " << newState.fetch2.instr << " as P" << newState.renameCur << endl;;
                    }
                    int d;
                    int ind2;
                    // put the second fetched instruction into the rename table
                    int oc = opcoder(newState.fetch2.instr);
                    
                    if(oc == LW || oc == SW){
                        ind2 = 0;
                    }
                    else if(oc == ADD || oc == NAND || oc == BEQ){
                        ind2 = 1;
                    }
                    else if(oc == MULT){
                        ind2 = 2;
                    }
                    if(oc != HALT && oc != NOOP){
                        resEntryStruct temp;
                        newState.myStations[ind2].emplace_back(temp);
                        newState.myStations[ind2][resCount[ind2]].src1P = newState.myRenameTable.physical[rRegA(newState.fetch2.instr)];
                        newState.myStations[ind2][resCount[ind2]].src2P = newState.myRenameTable.physical[rRegB(newState.fetch2.instr)];
                        newState.myStations[ind2][resCount[ind2]].src1Valid = !(newState.myRenameTable.valid[rRegA(newState.fetch2.instr)]);
                        newState.myStations[ind2][resCount[ind2]].src2Valid = !(newState.myRenameTable.valid[rRegB(newState.fetch2.instr)]);
                        if(newState.myStations[ind2][resCount[ind2]].src1Valid == 1){
                            newState.myStations[ind2][resCount[ind2]].src1V = newState.reg[rRegA(newState.fetch2.instr)];
                        }
                        if(newState.myStations[ind2][resCount[ind2]].src2Valid == 1){
                            newState.myStations[ind2][resCount[ind2]].src2V = newState.reg[rRegB(newState.fetch2.instr)];
                        }
                    }
					int safety = 0;
					for(int i = 0; i < 8; i++){
						if(newState.myRenameTable.physical[i] == newState.renameCur && newState.renameCur != i){
							safety = 1;
						}
					}
                    if(opcoder(newState.fetch2.instr) != LW && opcoder(newState.fetch2.instr) != NOOP && opcoder(newState.fetch2.instr) != HALT && opcoder(newState.fetch2.instr) != BEQ){
                        d = rDestReg(newState.fetch2.instr);
                        newState.myRenameTable.physical[d] = newState.renameCur +safety;
                        newState.renameCur = (newState.renameCur  + safety) % 32;
                        //     newState.myRenameTable.valid[d] = 1;
                        newState.myRenameTable.robIndex[d] = newState.myROB.destVal.size();
                    }
                    else if(opcoder(newState.fetch2.instr) != NOOP && opcoder(newState.fetch2.instr) != HALT && opcoder(newState.fetch2.instr) != BEQ){
                        d = rRegB(newState.fetch2.instr);
                        newState.myRenameTable.physical[d] = newState.renameCur + safety;
                        newState.renameCur = (newState.renameCur  + safety) % 32;
                        //    newState.myRenameTable.valid[d] = 1;
                        newState.myRenameTable.robIndex[d] = newState.myROB.destVal.size();
                    }
                    else{
                        d = 0;
                    }
                    
                    if(oc != HALT && oc != NOOP){
                        
                        newState.myStations[ind2][resCount[ind2]].rIndex = newState.myROB.destVal.size();
                        
                        newState.myStations[ind2][resCount[ind2]].pc1 = newState.fetch2.curPC + 1;
                        newState.myStations[ind2][resCount[ind2]].instr = newState.fetch2.instr;
                        newState.myStations[ind2][resCount[ind2]].destP = newState.myRenameTable.physical[rDestReg(newState.fetch2.instr)];
                        if(oc == LW || oc == SW){
                            newState.myStations[ind2][resCount[ind2]].destP = newState.myRenameTable.physical[rRegB(newState.fetch2
                                                                                                                    .instr)];
                        }
                        newState.myStations[ind2][resCount[ind2]].OP = opcoder(newState.fetch2.instr);
                        
                        newState.myStations[ind2][resCount[ind2]].isUsed = true;
                        newState.myRenameTable.valid[d] = 1;
                        int stupid = newState.myROB.destReg.size();
                        newState.myROB.destReg.emplace_back(d);
                        newState.myROB.destVal.emplace_back(newState.myRenameTable.physical[d]);
                        newState.myROB.destValid.emplace_back(0);
                        newState.myROB.op.emplace_back(oc);
                        newState.myROB.destP.emplace_back(newState.myStations[ind2][resCount[ind2]].destP);
                        newState.myROB.origPC.emplace_back(newState.myStations[ind2][resCount[ind2]].pc1 - 1);
                    }
                    else{
                        int stupid = newState.myROB.destReg.size();
                        newState.myROB.destReg.emplace_back(0);
                        newState.myROB.destVal.emplace_back(0);
                        newState.myROB.destValid.emplace_back(0);
                        newState.myROB.origPC.emplace_back(0);
                        newState.myROB.op.emplace_back(0);
                        newState.myROB.destP.emplace_back(0);
                        newState.myROB.destReg[stupid] = 0;
                        newState.myROB.destReg[stupid] = 0;
                        newState.myROB.destValid[stupid] = 1;
                        newState.myROB.op[stupid] = oc;
                        newState.myROB.destP[stupid] = 0;
                        newState.myROB.origPC[stupid] = newState.myStations[ind2][resCount[ind2]].pc1 - 1;
                        
                    }
                    newState.fetch2.valid = 0;
                }
                
            }
            
            if(newState.fetch2.valid == 1){ // we couldn't shove the other entry in
                cout << "Swapping second instruction to first slot\n";
                newState.fetch1 = newState.fetch2;
                newState.fetch2.valid = 0;
            }
            
            
            
        }
        
        
        
        
        cur = 0; // counter for a res station
        
        // commit BEQ before ADD or NAND
        
        for(int i  = 0; i < newState.myStations[1].size(); i++){
            if( newState.myStations[1][i].OP != BEQ && newState.myStations[1][i+1].OP == BEQ){ // THE NEXT ENTRY IS A BEQ SO SWAP IT
                resEntryStruct temp = newState.myStations[1][i];
                newState.myStations[1][i] = newState.myStations[1][i+1];
                newState.myStations[1][i+1] = temp;
            }
        }
        
        
        
        
        newState.cycles++;
        state = newState;
		
         
        cout << "---PRINT ROB---\n";
        for(int i  = 0; i < newState.myROB.destReg.size(); i++){
            cout << i << " valid: " << newState.myROB.destValid[i];
            cout << " val: " << newState.myROB.destVal[i];
            cout << " reg " << newState.myROB.destReg[i] << endl;
        }
        cout << "-----------\nRENAME_TABLE\n";
        for(int i = 0; i < 8; i++){
            cout << newState.myRenameTable.physical[i] << " " << newState.myRenameTable.valid[i] << "|";
        }
        cout << endl;
        
        
    }
    
    
    return;
}

void printState(stateType *statePtr)
{
    int i;
    printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
    printf("\tpc %d\n", statePtr->pc);
    printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
}

int field0(int instruction)
{
    return( (instruction>>19) & 0x7);
}

int field1(int instruction)
{
    return( (instruction>>16) & 0x7);
}

int field2(int instruction)
{
    return(instruction & 0xFFFF);
}

void printInstruction(int instr)
{
    char opcodeString[10];
    if (opcoder(instr) == ADD) {
        strcpy(opcodeString, "add");
    } else if (opcoder(instr) == NAND) {
        strcpy(opcodeString, "nand");
    } else if (opcoder(instr) == LW) {
        strcpy(opcodeString, "lw");
    } else if (opcoder(instr) == SW) {
        strcpy(opcodeString, "sw");
    } else if (opcoder(instr) == BEQ) {
        strcpy(opcodeString, "beq");
    }
    
    else if (opcoder(instr) == HALT) {
        strcpy(opcodeString, "halt");
    } else if (opcoder(instr) == NOOP) {
        strcpy(opcodeString, "noop");
    } else {
        strcpy(opcodeString, "data");
    }
    
    printf("%s %d %d %d\n", opcodeString, field0(instr), field1(instr),
           field2(instr));
}
/* End code for simulator */





/*

                                                                            
                                                                            
 ________  ______  __    __  ______   ______   __    __  ________  _______  
/        |/      |/  \  /  |/      | /      \ /  |  /  |/        |/       \ 
$$$$$$$$/ $$$$$$/ $$  \ $$ |$$$$$$/ /$$$$$$  |$$ |  $$ |$$$$$$$$/ $$$$$$$  |
$$ |__      $$ |  $$$  \$$ |  $$ |  $$ \__$$/ $$ |__$$ |$$ |__    $$ |  $$ |
$$    |     $$ |  $$$$  $$ |  $$ |  $$      \ $$    $$ |$$    |   $$ |  $$ |
$$$$$/      $$ |  $$ $$ $$ |  $$ |   $$$$$$  |$$$$$$$$ |$$$$$/    $$ |  $$ |
$$ |       _$$ |_ $$ |$$$$ | _$$ |_ /  \__$$ |$$ |  $$ |$$ |_____ $$ |__$$ |
$$ |      / $$   |$$ | $$$ |/ $$   |$$    $$/ $$ |  $$ |$$       |$$    $$/ 
$$/       $$$$$$/ $$/   $$/ $$$$$$/  $$$$$$/  $$/   $$/ $$$$$$$$/ $$$$$$$/  
                                                                            
                                                                            
*/

