#pragma warning (disable : 4996)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "register.h"
#include "instruction.h"
#include "pipe_register.h"

// register & memory global variables
unsigned int Memory[0x4000000] = { 0, };
unsigned int Register[32] = { 0, };
unsigned int PC = 0;

IFID ifid[2];
IDEX idex[2];
EXMEM exmem[2];
MEMWB memwb[2];

// instruction count variables
int cycle_count = 0;
int R_count = 0;
int I_count = 0;
int J_count = 0;
int MemAcc_count = 0;
int branch_count = 0;

// function
void init();
void read_mem(FILE* file);

int IF(FILE* file);
int ID();
int EX();
int MEM();
int WB();

int Control_Signal(int opcode);
int sign_extend(int imm);
int jump_Addr();
int branch_Addr();
void Branch_Prediction(int opcode, int val1, int val2);

void R_type(int func, int val1, int val2);
void I_type(int opcode, int val1, int val2);
void J_type(int opcode);

int main() {
    FILE* file;
    int ret = 0;

    char* filename;
    filename = "input01.bin";

    file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: There is no file");
        return 0;
    }

    read_mem(file);
    fclose(file);

    // initiallize
    init();

    // Run program
    while (1) {
        ret = IF(file);
        if (ret <= 0) break;
        printf("\n");

        ret = ID();
        if (ret <= 0) break;
        printf("\n");

        printf("[EX]----------------------------------------------------------------------------\n");
        ret = EX();
        if (ret <= 0) break;
        printf("\n");

        printf("[MEM]---------------------------------------------------------------------------\n");
        ret = MEM();
        if (ret <= 0) break;
        printf("\n");

        printf("[WB]----------------------------------------------------------------------------\n");
        ret = WB();
        if (ret <= 0) break;
        printf("\n");

        cycle_count++;
        printf("\n\n");
    }

    printf("================================================================================\n");
    printf("Return value (r2)                    : %d\n", Register[v0]);
    printf("Total cycle                          : %d\n", cycle_count);
    printf("Executed 'R' instruction             : %d\n", R_count);
    printf("Executed 'I' instruction             : %d\n", I_count);
    printf("Executed 'J' instruction             : %d\n", J_count);
    printf("Number of branch taken               : %d\n", branch_count);
    printf("Number of memory access instructions : %d\n", MemAcc_count);
    printf("=================================================================================\n");

    return 0;
}

void init() {
    Register[ra] = 0xFFFFFFFF;
    Register[sp] = 0x10000000;
}

void read_mem(FILE* file) {
    unsigned int ret = 0;
    int i = 0;
    int memVal;

    while (true) {
        int data = 0;                                    // 파일을 읽어오는 용도로 사용할 변수
        ret = fread(&data, 1, sizeof(int), file);        // data 변수에 파일로부터 4바이트 만큼 데이터를 읽어옴. ret 변수에는 fread 함수의 읽은 바이트 수가 저장됨
        memVal = ((data & 0xFF000000) >> 24) |
            ((data & 0x00FF0000) >> 8) |
            ((data & 0x0000FF00) << 8) |
            ((data & 0x000000FF) << 24);
        Memory[i++] = memVal;                             // Memory에 값을 저장


        if (ret != 4) break;                              // 4바이트를 읽지 못했다면 루프 탈출
    }
}

int IF(FILE* file) {
    int ret = 1;
    if (PC == 0xFFFFFFFF) return 0;                         // PC가 0xFFFFFFFF이면 종료
    ifid[0].inst = Memory[PC / 4];
    ifid[0].PC = PC;

    printf("Cycle[%03d]======================================================================\n\n", cycle_count + 1);
    printf("[IF]----------------------------------------------------------------------------\n");
    
    if (ifid[0].inst == 0) {
        PC += 4;
        cycle_count++;
        printf("Bubble\n");
        return IF(file);  // nop 명령어일 경우 다시 fetch 호출
    }

    PC += 4;
    
    printf("  [IM] PC : 0x%X -> 0x%08X\n", ifid[0].PC, ifid[0].inst);
    printf("  [PC UPDATE] PC <- 0x%08X = 0x%08X + 4\n", PC, ifid[0].PC);

    return ret;
}

int Control_Signal(int opcode) {
    int ret = 1;

    // MUX
    if (opcode == 0x0) {
        idex[0].RegDst = 1;
    }
    else {
        idex[0].RegDst = 0;
    }

    if ((opcode == J) || (opcode == JAL) || (opcode == BEQ) || (opcode == BNE)) {
        idex[0].Branch = 1;
    }
    else {
        idex[0].Branch = 0;
    }

    if (opcode == LW) {
        idex[0].MemtoReg = 1;
        idex[0].MemRead = 1;
    }
    else {
        idex[0].MemtoReg = 0;
        idex[0].MemRead = 0;
    }

    if (opcode == SW) {
        idex[0].MemWrite = 1;
    }
    else {
        idex[0].MemWrite = 0;
    }

    if ((opcode != 0) && (opcode != BEQ) && (opcode != BNE)) {
        idex[0].ALUSrc = 1;
    }
    else {
        idex[0].ALUSrc = 0;
    }

    if ((opcode != SW) && (opcode != BEQ) && (opcode != BNE) && (opcode != JR) && (opcode != J) && (opcode != JAL)) {
        idex[0].RegWrite = 1;
    }
    else {
        idex[0].RegWrite = 0;
    }

    return ret;
}

int sign_extend(int imm) {
    int sign_imm;
    int sign = (imm & 0x0000FFFF) >> 15;
    if (sign == 1) {
        sign_imm = 0xFFFF0000 | imm;
    }
    else {
        sign_imm = 0x0000FFFF & imm;
    }
    return sign_imm;
}

int ID() {
    int ret = 1;
    int opcode;

    ifid[1] = ifid[0];

    idex[0].opcode = (ifid[1].inst & 0xFC000000);
    idex[0].opcode = (idex[0].opcode >> 26) & 0x3F;
    opcode = idex[0].opcode;

    idex[0].rs = (ifid[1].inst & 0x03E00000);
    idex[0].rs = (idex[0].rs >> 21) & 0x1F;

    idex[0].rt = (ifid[1].inst & 0x001F0000);
    idex[0].rt = (idex[0].rt >> 16) & 0x1F;

    idex[0].rd = (ifid[1].inst & 0x0000F800);
    idex[0].rd = (idex[0].rd >> 11) & 0x1F;

    idex[0].shamt = (ifid[1].inst & 0x000007C0);
    idex[0].shamt = idex[0].shamt >> 6;

    idex[0].func = (ifid[1].inst & 0x0000003F);

    idex[0].imm = (ifid[1].inst & 0x0000FFFF);

    idex[0].addr = (ifid[1].inst & 0x03FFFFFF);

    printf("[ID]----------------------------------------------------------------------------\n");

    switch (opcode) {
    case 0:
        printf("  [TYPE : R]    OPCODE : 0x%X, RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), RD : 0x%X (R[%d] = 0x%X), SHAMT : 0x%X, FUNCT : 0x%X\n", opcode, idex[0].rs, idex[0].rs, Register[idex[0].rs], idex[0].rt, idex[0].rt, Register[idex[0].rt], idex[0].rd, idex[0].rd, Register[idex[0].rd], idex[0].shamt, idex[0].func);
        
        R_count++;
        break;

    case J:
        printf("  [TYPE : J]    OPCODE : 0x%X, BRANCH : 0x%X\n", opcode, idex[0].addr);

        J_count++;
        break;

    case JAL:
        printf("  [TYPE : J]    OPCODE : 0x%X, BRANCH : 0x%X\n", opcode, idex[0].addr);

        J_count++;
        break;

    default:
        printf("  [TYPE : I]    OPCODE : 0x%X, RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), IMM : 0x%X\n", opcode, idex[0].rs, idex[0].rs, Register[idex[0].rs], idex[0].rt, idex[0].rt, Register[idex[0].rt], idex[0].imm);

        I_count++;
        break;
    }

    int imm = ifid[1].inst;
    idex[0].SignExtImm = sign_extend(imm);

    idex[0].inst = ifid[1].inst;
    idex[0].PC = ifid[1].PC;

    J_type(opcode);
    Control_Signal(opcode);

    return ret;
}


int jump_Addr() {
    idex[0].JumpAddr = (ifid[1].PC & 0xF0000000) | (idex[0].addr << 2);
    int j = idex[0].JumpAddr;
    return j;
}

int branch_Addr() {
    idex[1].BranchAddr = idex[1].SignExtImm << 2;
    int b = idex[1].BranchAddr;
    return b;
}

void R_type(int func, int val1, int val2) {
    switch (idex[1].func) {
    case ADD:
        exmem[0].ALUResult = val1 + val2;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", exmem[0].rs, exmem[0].rs, val1, exmem[0].rt, exmem[0].rt, val2, exmem[0].ALUResult);
        break;

    case ADDU:
        exmem[0].ALUResult = val1 + val2;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", exmem[0].rs, exmem[0].rs, val1, exmem[0].rt, exmem[0].rt, val2, exmem[0].ALUResult);
        break;

    case AND:
        exmem[0].ALUResult = val1 & val2;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", exmem[0].rs, exmem[0].rs, val1, exmem[0].rt, exmem[0].rt, val2, exmem[0].ALUResult);
        break;

    case JR:
        PC = val1;
        memset(&ifid[0], 0, sizeof(IFID));
        memset(&idex[0], 0, sizeof(IDEX));
        break;

    case JALR:
        PC = val1;
        memset(&ifid[0], 0, sizeof(IFID));
        memset(&idex[0], 0, sizeof(IDEX));
        break;

    case NOR:
        exmem[0].ALUResult = !(val1 | val2);
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, idex[1].ALUResult);

        break;

    case OR:
        exmem[0].ALUResult = (val1 | val2);
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SLT:
        exmem[0].ALUResult = ((val1 < val2) ? 1 : 0);
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SLTU:
        exmem[0].ALUResult = ((val1 < val2) ? 1 : 0);
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SLL:
        exmem[0].ALUResult = val2 << idex[1].shamt;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SRL:
        exmem[0].ALUResult = val2 >> idex[1].shamt;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SUB:
        exmem[0].ALUResult = val1 - val2;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    case SUBU:
        exmem[0].ALUResult = val1 - val2;
        printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, exmem[0].ALUResult);

        break;

    default:
        break;
    }
}

void I_type(int opcode, int val1, int val2) {
    int temp;
    switch (opcode) {
    case ADDI:
        exmem[0].ALUResult = val1 + idex[1].SignExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case ADDIU:
        exmem[0].ALUResult = val1 + idex[1].SignExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case ANDI:
        exmem[0].ALUResult = val1 & idex[1].ZeroExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case LBU:
        exmem[0].ALUResult = idex[1].rs + idex[1].SignExtImm & 0x000000FF;
        exmem[0].ALUResult = (Memory[exmem[0].ALUResult / 4]) & 0x000000FF;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case LHU:
        exmem[0].ALUResult = idex[1].rs + idex[1].SignExtImm & 0x000000FF;
        exmem[0].ALUResult = (Memory[exmem[0].ALUResult / 4]) & 0x0000FFFF;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case LL:
        exmem[0].rt = Memory[idex[1].rs + idex[1].SignExtImm / 4];
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case LUI:
        exmem[0].ALUResult = idex[1].imm << 16;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case LW:
        exmem[0].temp = val1 + idex[1].SignExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case ORI:
        exmem[0].ALUResult = val1 | idex[1].ZeroExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SLTI:
        exmem[0].ALUResult = (val1 < (idex[1].SignExtImm) ? 1 : 0);
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SLTIU:
        exmem[0].ALUResult = ((val1 < idex[1].SignExtImm) ? 1 : 0);
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SB:
        exmem[0].ALUResult = idex[1].rs + idex[1].SignExtImm;
        temp = Memory[exmem[0].ALUResult / 4];
        temp = temp & 0xFFFFFF00;
        temp = (idex[1].rt & 0x000000FF) | temp;
        Memory[exmem[0].ALUResult / 4] = temp;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SC:
        exmem[0].ALUResult = idex[1].SignExtImm;
        temp = idex[1].rs + exmem[0].ALUResult;
        exmem[0].ALUResult = idex[1].rt;
        Memory[temp / 4] = exmem[0].ALUResult;
        exmem[0].rt = 1;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SH:
        exmem[0].ALUResult = idex[1].rs + idex[1].SignExtImm;
        temp = Memory[exmem[0].ALUResult / 4];
        temp = temp & 0xFFFF0000;
        temp = (idex[1].rt & 0x0000FFFF) | temp;
        Memory[exmem[0].ALUResult / 4] = temp;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    case SW:
        exmem[0].ALUResult = val2;
        exmem[0].temp = val1 + idex[1].SignExtImm;
        printf("  RS : 0x%X (R[%d] = 0x%X), IMM : 0x%X, ALU_RESULT : %d\n", idex[1].rs, idex[1].rs, val1, idex[1].SignExtImm, exmem[0].ALUResult);

        break;

    default:
        break;
    }
}

void J_type(int opcode) {
    switch (opcode) {
    case J :
        PC = jump_Addr();

        printf("  Jump PC = 0x%X\n", PC);
        break;

    case JAL : 
        Register[ra] = idex[0].PC + 8;
        PC = jump_Addr();

        printf("  Jump PC = 0x%X\n", PC);
        break;
    }
}

void Branch_Prediction(int opcode, int val1, int val2) {
    switch (opcode) {
    case BEQ:
        if (val1 == val2) {
            int b = branch_Addr();
            PC = idex[1].PC + b + 4;
            //flush
            memset(&ifid[0], 0, sizeof(IFID));
            memset(&idex[0], 0, sizeof(IDEX));

            branch_count++;    
            printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), PC = 0x%X\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, PC);
        }
        break;

    case BNE:
        if (val1 != val2) {
            int b = branch_Addr();
            PC = idex[1].PC + b + 4;
            //flush
            memset(&ifid[0], 0, sizeof(IFID));
            memset(&idex[0], 0, sizeof(IDEX));

            branch_count++;
            printf("  RS : 0x%X (R[%d] = 0x%X), RT : 0x%X (R[%d] = 0x%X), PC = 0x%X\n", idex[1].rs, idex[1].rs, val1, idex[1].rt, idex[1].rt, val2, PC);
        }
        break;

    default:
        break;
    }
}

int EX() {
    int ret = 1;
    int val1, val2;

    idex[0].val1 = Register[idex[0].rs];
    idex[0].val2 = Register[idex[0].rt];

    idex[1] = idex[0];

    idex[1].ZeroExtImm = (idex[1].inst & 0x0000FFFF);

    // data forwarding
    if ((idex[1].rs != 0) && (idex[1].rs == exmem[0].WriteReg) && (exmem[0].RegWrite)) {
        idex[1].val1 = exmem[0].ALUResult;
    }
    else if ((idex[1].rs != 0) && (idex[1].rs == memwb[0].WriteReg) && (memwb[0].RegWrite)) {
        if (memwb[0].MemtoReg == 1) {
            idex[1].val1 = memwb[0].ReadData;
        }
        else {
            idex[1].val1 = memwb[0].ALUResult;
        }
    }
    if ((idex[1].rt != 0) && (idex[1].rt == exmem[0].WriteReg) && (exmem[0].RegWrite)) {
        idex[1].val2 = exmem[0].ALUResult;
    }
    else if ((idex[1].rt != 0) && (idex[1].rt == memwb[0].WriteReg) && (memwb[0].RegWrite)) {
        if (memwb[1].MemtoReg == 1) {
            idex[1].val2 = memwb[0].ReadData;
        }
        else {
            idex[1].val2 = memwb[0].ALUResult;
        }
    }

    val1 = idex[1].val1;
    val2 = idex[1].val2;

    if (idex[1].RegDst == 1) {
        R_type(idex[1].func, val1, val2);
        R_count++;
    }
    else if (idex[1].ALUSrc == 1) {
        I_type(idex[1].opcode, val1, val2);
    }
    else if (idex[1].Branch == 1) {
        Branch_Prediction(idex[1].opcode, val1, val2);
    }

    if (idex[1].RegDst == 1) { // R-type
        exmem[0].WriteReg = idex[1].rd;
    }
    else {
        exmem[0].WriteReg = idex[1].rt;
    }

    // Update Latch : exmem[0] = idex[1]
    exmem[0].opcode = idex[1].opcode;
    exmem[0].func = idex[1].func;
    exmem[0].inst = idex[1].inst;
    exmem[0].PC = idex[1].PC;
    exmem[0].Branch = idex[1].Branch;
    exmem[0].MemRead = idex[1].MemRead;
    exmem[0].MemtoReg = idex[1].MemtoReg;
    exmem[0].MemWrite = idex[1].MemWrite;
    exmem[0].RegWrite = idex[1].RegWrite;
    exmem[0].val1 = val1;
    exmem[0].val2 = val2;

    return ret;
}

int MEM() {
    int ret = 1;

    exmem[1] = exmem[0];

    if (exmem[1].MemRead == 1) { // LW
        exmem[1].ReadData = Memory[exmem[1].temp / 4];

        printf("  R[%d] <- 0x%X\n", exmem[1].rs, exmem[1].ReadData);

        MemAcc_count++;
    }
    else if (exmem[1].MemWrite == 1) { // SW
        Memory[exmem[1].temp / 4] = exmem[1].val2;

        printf("  R[%d] -> 0x%X\n", exmem[1].rs, exmem[1].val2);

        MemAcc_count++;
    }

    // Update Latch : memwb[0] = exmem[1]
    memwb[0].opcode = exmem[1].opcode;
    memwb[0].func = exmem[1].func;
    memwb[0].inst = exmem[1].inst;
    memwb[0].MemtoReg = exmem[1].MemtoReg;
    memwb[0].RegWrite = exmem[1].RegWrite;
    memwb[0].val1 = exmem[1].val1;
    memwb[0].val2 = exmem[1].val2;
    memwb[0].ALUResult = exmem[1].ALUResult;
    memwb[0].WriteReg = exmem[1].WriteReg;
    memwb[0].ReadData = exmem[1].ReadData;

    return ret;
}

int WB() {
    int ret = 1;
    
    memwb[1] = memwb[0];

    if (memwb[1].RegWrite == 1) {
        if (memwb[1].MemtoReg == 1) {
            Register[memwb[1].WriteReg] = memwb[1].ReadData;
        }
        else {
            if (memwb[1].WriteReg != 0) {
                Register[memwb[1].WriteReg] = memwb[1].ALUResult;
            }
            else {
                Register[memwb[1].WriteReg] = 0;
            }
        }
    }

    printf("  R[%d] <- 0x%X\n", memwb[1].WriteReg, Register[memwb[1].WriteReg]);

    return ret;
}