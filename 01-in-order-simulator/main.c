/* Assembler for LC */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

typedef int bool;   // the epitome of laziness
#define true 1
#define false 0

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

// simulator structs
typedef struct IFIDStruct {
    int instr;
    int pcPlus1;
} IFIDType;

typedef struct IDEXStruct {
    int instr;
    int pcPlus1;
    int readRegA;
    int readRegB;
    int offset;
} IDEXType;

typedef struct EXMEMStruct {
    int instr;
    int branchTarget;
    int aluResult;
    int readRegB;
} EXMEMType;

typedef struct MEMWBStruct {
    int instr;
    int writeData;
} MEMWBType;

typedef struct WBENDStruct {
    int instr;
    int writeData;
} WBENDType;

typedef struct stateStruct {
    int pc;
    int instrMem[NUMMEMORY];
    int dataMem[NUMMEMORY];
    int reg[NUMREGS];
    int numMemory;
    IFIDType IFID;
    IDEXType IDEX;
    EXMEMType EXMEM;
    MEMWBType MEMWB;
    WBENDType WBEND;
    int cycles; /* number of cycles run so far */
} stateType;

typedef struct bpStruct {
    int pc[4];  // branch instruction program counter
    int state[4]; // state of the current branch
} bpType;

typedef struct btbStruct {
    int pc[4]; // source address
    int jump[4]; // where to jump
} btbType;

struct stateStruct state; // global so we can feed in the instructions
int instrCount = 0;		 // num of instructions

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
    
	/*
    for (i=0; i<numLabels; i++) {
        printf("%s = %d\n", labelArray[i], labelAddress[i]);    // label addresses are indexed from 0
    }
    */
	
    /* now do second pass (print machine code, with symbols filled in as
     addresses) */
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
        }
       // printf("opcode:%d\nregA:%d\nregB:%d\nregDest:%d\nOffset:%d\n\n", opcoder(num),rRegA(num),rRegB(num), rDestReg(num),rOffset(num));
       // printf("%d\n", num);
		state.instrMem[instrCount] = num;
		state.dataMem[instrCount] = num;
		instrCount++;
    }
    
    run(); // assembler stuff is done so lets run the simulator
    
    exit(0);
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
    
    
    int fetched = 0;
    int retired = 0;
    int branches = 0;
    int mispred = 0;
    state.pc = 0;   // initialize the PC and registers to 0
    for(int i = 0; i < NUMREGS; i++){
        state.reg[i] = 0;
    }
    state.IFID.instr = NOOPINSTRUCTION; // initialize pipeline instructions with NOOP
    state.IDEX.instr = NOOPINSTRUCTION;
    state.EXMEM.instr = NOOPINSTRUCTION;
    state.MEMWB.instr = NOOPINSTRUCTION;
    state.WBEND.instr = NOOPINSTRUCTION;

    
    struct stateStruct newState;
    struct bpStruct myBP;
    struct btbStruct myBTB;
    newState = state;
    int branchFlag = 0; // simulate signaling a branch
    int jumpFlag = 0;
    for(int i = 0; i < 4; i++){ // clear out the table
        myBP.pc[i] = -1;
        myBP.state[i] = -1;
        myBTB.pc[i] = -1;
        myBTB.jump[i] = -1;
    }
    int tableCounter = 0;
    int btbCounter = 0;
    int jumpCounter = 0;
    int jumpPC = -1;
    
    while (1) {
        newState.reg[0] = 0;
		state.reg[0] = 0;
        /* check for halt */
        if (opcoder(state.MEMWB.instr) == HALT) {
            printf("machine halted\n");
            printf("total of %d cycles executed\n", state.cycles);
            printf("total of %d instructions fetched\n", fetched);
            printf("total of %d instructions completed\n", retired);
            printf("total of %d branches executed\n", branches);
            printf("total of %d branches wrong\n", mispred);
            exit(0);
        }
        
        //if(newState.cycles > 20){return;}
        printState(&state);

        // FETCH COMPLETE
        /* --------------------- IF stage --------------------- */
        newState.IFID.instr = newState.instrMem[state.pc];        // load instruction from instruction memory to IFID
        fetched++;
        if (opcoder(newState.IFID.instr) == BEQ){                 // check if branch was issued
            bool tableSuccess = false;
            for(int i = 0; i < 4; i++){
              if(myBP.pc[i] == state.pc){
                    tableSuccess = true;
                  if(tableCounter == i || (tableCounter - 1) % 4 == i){
                      tableCounter = (tableCounter + 1) % 4;
                  }
                    if(myBP.pc[i] < 2){
                        jumpPC = -1;
                        break;      // don't take
                    }
                    else{   // need to confer with the branch target buffer now
                        bool bufferSuccess = false;
                        for(int j = 0; j < 4; j++){
                            if(myBTB.jump[j] != -1 && myBTB.pc[j] == state.pc){
                                bufferSuccess = true;
                                jumpFlag = 1;
                                jumpPC = myBTB.jump[j];
                                if(jumpCounter == j || ((jumpCounter - 1)) % 4 == j){
                                    jumpCounter = (jumpCounter + 1) % 4;
                                }
                                break;
                            }
                        }
                        if(bufferSuccess == false){
                            myBTB.pc[jumpCounter] = state.pc;
                            jumpCounter = (jumpCounter + 1) % 4;
                        }
                    }
                    break;
                }
                
            }
            if(tableSuccess == false){
                myBP.pc[tableCounter] = state.pc;
                myBP.state[tableCounter] = WNTAKEN;
                tableCounter = (tableCounter + 1) % 4; // FIFO IT WITH THE POWER OF MODULAR ARITHMETIC
            }
        }
        
        
        newState.IFID.pcPlus1 = state.pc + 1;                    // store pc+1 in the IF/ID
        
        
        // DECODE
        /* --------------------- ID stage --------------------- */
        
        newState.IDEX.instr = state.IFID.instr;                             // pass the instruction from IFID to IDEX
        newState.IDEX.pcPlus1 = state.IFID.pcPlus1;                        // pass pc+1 from IFID to IDEX
        newState.IDEX.readRegA = newState.reg[rRegA(state.IFID.instr)];   //  read the proper register A and pass to IDEX
        newState.IDEX.readRegB = newState.reg[rRegB(state.IFID.instr)];  //  ditto for B
        
        // EXECUTE
        /* --------------------- EX stage --------------------- */
        
        newState.EXMEM.branchTarget = state.IDEX.pcPlus1 + rOffset(state.IDEX.instr); // branch target is PC+1 + offset
        newState.EXMEM.instr = state.IDEX.instr;                                     // forward the instruction
        
        
        /* --------------------- Data Forwarding --------------------- */
        int srcA = rRegA(state.IDEX.instr);  // source of regA
        int srcB = rRegB(state.IDEX.instr); // source of regB
        int valA = state.IDEX.readRegA;    // initialize to regA
        int valB = state.IDEX.readRegB;   // initialize to regB
        
        int destR = rDestReg(state.EXMEM.instr);    // destReg of EXMEM
        int oc = opcoder(state.EXMEM.instr);    // decipher instruction type
        if((srcA == destR) && (oc == 0 || oc == 1 || oc == 5)){ // check if the ALU just calculated our target register
            valA = state.EXMEM.aluResult;                      // if so set our value to it
        }
        else{
            destR = rDestReg(state.MEMWB.instr);                // else, check MEMWB
            oc = opcoder(state.MEMWB.instr);
            if((srcA == destR) && (oc == 0 || oc == 1|| oc == 5)){
                    valA = state.MEMWB.writeData;
            }
            else if(srcA == rRegB(state.MEMWB.instr) && oc == 2){   // also need to check for a LW at this stage
                valA =  state.MEMWB.writeData;                     // since B would be the destination register
            }
            else{
                destR = rDestReg(state.WBEND.instr);    // worst case, check WBEND
                oc = opcoder(state.WBEND.instr);
                if((srcA == destR) && (oc == 0 || oc == 1 ||oc == 5)){
                        valA = state.WBEND.writeData;
                }
                else if (srcA == rRegB(state.WBEND.instr) && oc == 2){
                    valA = state.WBEND.writeData;
                }
            }
        }
        destR = rDestReg(state.EXMEM.instr);
        oc = opcoder(state.EXMEM.instr);
        if((srcB == destR) && (oc == 0 || oc == 1 || oc == 5)){ // now repeat for B :)
            valB = state.EXMEM.aluResult;
        }
        else{
            destR = rDestReg(state.MEMWB.instr);
            oc = opcoder(state.MEMWB.instr);
            if((srcB == destR) && (oc == 0 || oc == 1 || oc == 5)){
                valB = state.MEMWB.writeData;
            }
            else if(srcB == rRegB(state.MEMWB.instr) && oc == 2){   // also need to check for a LW at this stage
                valB =  state.MEMWB.writeData;                     // since B would be the destination register
            }
            else{
                destR = rDestReg(state.WBEND.instr);
                oc = opcoder(state.WBEND.instr);
                if((srcB == destR) && (oc == 0 || oc == 1 || oc == 5)){
                    valB = state.WBEND.writeData;
                }
                else if (srcB == rRegB(state.WBEND.instr) && oc == 2){
                    valB = state.WBEND.writeData;
                }
            }
        }
        
        newState.EXMEM.readRegB = valB; // forward regB contents from IDEX pipeline
                                        // forward updated value
        
        /* --------------------- End Data Forwarding --------------------- */
        
        if(opcoder(state.IDEX.instr) == 0){ // perform addition, add regA + regB, store result in destReg
            newState.EXMEM.aluResult = (valA + valB);
        }
        else if(opcoder(state.IDEX.instr) == 1){    // nand the contents of regA and regB
            newState.EXMEM.aluResult = (~(valA & valB));
        }
        else if(opcoder(state.IDEX.instr) == 2){    // calculate the memory offset to load into regB from instr + regA
            newState.EXMEM.aluResult = valA + rOffset(state.IDEX.instr);
        }
        else if(opcoder(state.IDEX.instr) == 3){    // calculate the memory offset to load from regB from instr + regA
            newState.EXMEM.aluResult = valA + rOffset(state.IDEX.instr);
        }
        else if(opcoder(state.IDEX.instr) == 4){
            if(valA == valB){
                newState.EXMEM.aluResult = state.IDEX.pcPlus1 + rOffset(state.IDEX.instr);   // where to branch
                branches++;
                printf("\n---Branching, regA == regB for instruction %d goto: %d ---\n\n\n ",state.IDEX.instr, newState.EXMEM.aluResult);
                branchFlag = 1;                 // since we are checking this flag anyways
            }
            else{
                printf("\n---Not Branching, regA != regB for instruction %d---\n\n\n",state.IDEX.instr);
                for(int i = 0; i < 4; i++){
                    if(myBP.pc[i] == state.IDEX.pcPlus1 - 1){
                        if(myBP.state[i] > 0){
                            if(myBP.state[i] > 1){  // we branched when we shouldn't have
                                printf("\n---Uh Oh! We shouldn't have branched, Branch Predictor was wrong. Squash!---\n\n\n");
                                mispred++;
                                newState.IDEX.instr = NOOPINSTRUCTION; // nope out
                                newState.IFID.instr = NOOPINSTRUCTION; // nope out
                                newState.pc = state.IDEX.pcPlus1; // fetch PC + 1 next cycle
                                jumpFlag = -1;
                            }
                             myBP.state[i] -= 1;
                        }
                    }
                }
                newState.EXMEM.aluResult = state.IDEX.pcPlus1 + rOffset(state.IDEX.instr);
            }
        }
        else if(opcoder(state.IDEX.instr) == 5){ // mult regA * regB
            newState.EXMEM.aluResult = (valA * valB);
        }
        else if(opcoder(state.IDEX.instr) == 6){    // HALT
            newState.EXMEM.aluResult = 0;
        }
        else if(opcoder(state.IDEX.instr) == 7){    // NOOP
            newState.EXMEM.aluResult = 0;
        }

        // MEMORY OPERATION
        /* --------------------- MEM stage --------------------- */
        
        newState.MEMWB.instr = state.EXMEM.instr;
        newState.MEMWB.writeData = state.EXMEM.aluResult;
        if(opcoder(state.EXMEM.instr) == 2){    // load from address
            newState.MEMWB.writeData = state.dataMem[state.EXMEM.aluResult];
        }
        else if(opcoder(state.EXMEM.instr) == 3){   // store to memory
            newState.dataMem[state.EXMEM.aluResult] = state.EXMEM.readRegB;
        }
        /* --------------------- WB stage --------------------- */
        oc = opcoder(state.MEMWB.instr);
        if(oc == 2){
            newState.reg[rRegB(state.MEMWB.instr)] = state.MEMWB.writeData;
        }
        else{
            if(oc == 0 || oc == 1 || oc == 5){
                newState.reg[rDestReg(state.MEMWB.instr)] = state.MEMWB.writeData;
            }
        }
        retired++;
        newState.WBEND.instr = state.MEMWB.instr;
        newState.WBEND.writeData = state.MEMWB.writeData;
        if(branchFlag == 1){    // we were signalled to branch by BEQ in the previous instruction
            newState.pc = newState.EXMEM.branchTarget;
            for(int i = 0; i < 4; i++){
                if(myBP.pc[i] == state.IDEX.pcPlus1 - 1){
                    if(myBP.state[i] < 3){
                        if(myBP.state[i] < 2){ // woops, we should've branched
                            printf("\n--- Uh oh! We should've branched! Branched predictor was wrong, squashing... %d\n", newState.EXMEM.aluResult);
                            mispred++;
                            newState.IFID.instr = NOOPINSTRUCTION;
                            newState.IDEX.instr = NOOPINSTRUCTION;
                            newState.pc = newState.EXMEM.aluResult; // load from calculated value
                            jumpFlag = -1;
                        }
                        myBP.state[i] += 1; // taken, so increment by 1
                    }
                }
                if(myBTB.pc[i] == state.IDEX.pcPlus1){
                    if(myBTB.jump[i] != newState.EXMEM.branchTarget){ // we guessed the wrong place to jump :)
                        printf("\n--- Oh Noooooo! We guessed the wrong place to jump with our BTB, squash and fix!\n");
                        mispred++;
                        newState.IFID.instr = NOOPINSTRUCTION; // noop our way out
                        newState.IDEX.instr = NOOPINSTRUCTION;
                        newState.pc = newState.EXMEM.aluResult;
                        jumpFlag = -1;
                    }
                }
            }
        }
        if(jumpFlag == 1 && branchFlag != 1){
            newState.pc = jumpPC;
        }
        
        else if(branchFlag != 1){
            newState.pc = newState.IFID.pcPlus1;
        }
        
        newState.reg[0] = 0; // set to 0 again to be safea
        jumpFlag = 0;
        branchFlag = 0;
        newState.cycles++;
        state = newState; /* this is the last statement before end of the loop.
                           It marks the end of the cycle and updates the
                           current state with the values calculated in this
                           cycle */
    }
    return;
}

void printState(stateType *statePtr)
{
    int i;
    printf("\n@@@\nstate before cycle %d starts\n", statePtr->cycles);
    printf("\tpc %d\n", statePtr->pc);
    /*
    printf("\tdata memory:\n");
    for (i=0; i<statePtr->numMemory; i++) {
        printf("\t\tdataMem[ %d ] %d\n", i, statePtr->dataMem[i]);
    }*/
    printf("\tregisters:\n");
    for (i=0; i<NUMREGS; i++) {
        printf("\t\treg[ %d ] %d\n", i, statePtr->reg[i]);
    }
    printf("\tIFID:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IFID.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IFID.pcPlus1);
    printf("\tIDEX:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->IDEX.instr);
    printf("\t\tpcPlus1 %d\n", statePtr->IDEX.pcPlus1);
    printf("\t\treadRegA %d\n", statePtr->IDEX.readRegA);
    printf("\t\treadRegB %d\n", statePtr->IDEX.readRegB);
    printf("\t\toffset %d\n", statePtr->IDEX.offset);
    printf("\tEXMEM:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->EXMEM.instr);
    printf("\t\tbranchTarget %d\n", statePtr->EXMEM.branchTarget);
    printf("\t\taluResult %d\n", statePtr->EXMEM.aluResult);
    printf("\t\treadRegB %d\n", statePtr->EXMEM.readRegB);
    printf("\tMEMWB:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->MEMWB.instr);
    printf("\t\twriteData %d\n", statePtr->MEMWB.writeData);
    printf("\tWBEND:\n");
    printf("\t\tinstruction ");
    printInstruction(statePtr->WBEND.instr);
    printf("\t\twriteData %d\n", statePtr->WBEND.writeData);
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
    
    /*
     else if (opcode(instr) == JALR) {
     strcpy(opcodeString, "jalr");
     } // this isn't in our requirements doc */
    
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
