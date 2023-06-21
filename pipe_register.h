#pragma once

typedef struct ifid {
	int inst;
	int PC;
}IFID;

typedef struct idex {
	unsigned int opcode, rs, rd, rt, imm, addr, shamt, func, inst;
	int PC;
	int JumpAddr, BranchAddr;
	int ZeroExtImm, SignExtImm;
	int RegDst, Branch, MemRead, MemtoReg, MemWrite, ALUSrc, RegWrite;
	int val1, val2;
	int temp;
	int ALUResult;
}IDEX;

typedef struct exmem {
	unsigned int opcode, rs, rd, rt, imm, addr, shamt, func, inst;
	int PC;
	int JumpAddr, BranchAddr;
	int ZeroExtImm, SignExtImm;
	int RegDst, Branch, MemRead, MemtoReg, MemWrite, ALUSrc, RegWrite;
	int val1, val2;
	int temp;
	int WriteReg, ALUResult, ReadData;
}EXMEM;

typedef struct memwb {
	unsigned int opcode, rs, rd, rt, imm, addr, shamt, func, inst;
	int PC;
	int JumpAddr, BranchAddr;
	int ZeroExtImm, SignExtImm;
	int RegDst, Branch, MemRead, MemtoReg, MemWrite, ALUSrc, RegWrite;
	int val1, val2;
	int temp;
	int WriteReg, ALUResult, ReadData;
}MEMWB;