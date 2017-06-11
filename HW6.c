#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdlib.h>

#define OPCODE(instr)  (instr >> 12 & 0x000F)
#define DSTREG(instr)  (instr >> 9 & 0x0007) 
#define SRCREG(instr)  (instr >> 6 & 0x0007)
#define BASEREG(instr)  (instr >> 6 & 0x0007)
#define SRCREG2(instr)  (instr & 0x0007)
#define IMMBIT(instr)  ((instr & 0x0020) >> 5) 
#define NBIT(instr)  ((instr & 0x0800) >> 11) 
#define ZBIT(instr)  ((instr & 0x0400) >> 10) 
#define PBIT(instr)  ((instr & 0x0200) >> 9) 
#define IMMVAL(instr)  ((instr << 27 ) >> 27) 
#define OFFSET6(instr) ((instr << 26 ) >> 26)
#define PCOFFSET9(instr) ((instr << 23 ) >> 23) 
#define PCOFFSET11(instr) ((instr << 21) >> 21)
#define TRAPVECT8(instr) ((instr << 24) >> 24)

int16_t  memory[65536];
int16_t regs[8];
int16_t pc, ir;

struct {
    int junk:13;
    unsigned int p:1;
    unsigned int n:1;
    unsigned int z:1;
} psr;   // process status register



void setCC(int16_t dest_reg) {
    if (regs[dest_reg] == 0) {
        psr.z = 1;
        psr.n = 0;
        psr.p = 0;
    } else if (regs[dest_reg] < 0) {
        psr.z = 0;
        psr.n = 1;
        psr.p = 0;                       
    } else {   // else if > 0
        psr.z = 0;
        psr.n = 0;
        psr.p = 1;                      
    }                  
                  
}


int main(int argc, char **argv) {

       // ADD  R4, R5, 7
       // 0001  100  101  1   00111
       // 0001  1001  0110 0111
       //   1     9     6    7
       
       // ADD  R4, R5, -8
       // 0001  100  101  1   11000
       // 0001  1001  0111 1000
       //   1     9     7    8
   
   int p;
    for (p = 1; p < argc; p++) {
    char *fileName;
       fileName = argv[p];

   
   // how big is the input file?
   struct stat st;
   stat(fileName, &st);
   int size_in_bytes = st.st_size;
         
   FILE *infile = fopen(fileName,"r");

   if (!infile) {
       printf("Couldn't open input file!\n");
       exit(2);
   }
   
   int16_t load_start_addr;
   
   // read in first two bytes to find out starting address of machine code
   int words_read = fread(&load_start_addr,sizeof(int16_t), 1, infile);
   char *cptr = (char *)&load_start_addr;
   char temp;
   
   // switch order of bytes
   temp = *cptr;
   *cptr = *(cptr+1);
   *(cptr+1) = temp;
   pc = load_start_addr;
   
   
   // now read in the remaining bytes of the object file
   int instrs_to_load = (size_in_bytes-2)/2;
   words_read = fread(&memory[load_start_addr], sizeof(int16_t),instrs_to_load, infile);
   
   // again switch the bytes 
   int i;
   cptr = (char *)&memory[load_start_addr]; 
   for (i = 0; i < instrs_to_load; i++) {
       temp = *cptr;
       *cptr = *(cptr+1);
       *(cptr+1) = temp;
       cptr += 2;  // next pair       
   }
   }     
      
   // main loop for fetching and executing instructions
   // for now, we do this until we run into the instruction with opcode 13
   
   psr.p = 1;  // need a valid psr      
   int halt = 0;
   uint16_t DSR, DDR;
   char c;
   int pls = 0 ;
   memory[0xFE04] = 0x8000;
   
   while (!halt) {   // one instruction executed on each rep.
        ir = memory[pc]; //fetched the instruction
        int16_t opcode = OPCODE(ir);
        //printf("pc = %04hx opcode = %02hx\n", pc, opcode);       
        pc++; 
           
        int16_t dest_reg, src_reg, src_reg2, base_reg,
                imm_bit, imm_val, pcoffset9, offset6,
                nbit, zbit, pbit, pcoffset11, trapvect8;
  
        //DSR = 0x8000;
        memory[65028] = 32768;
        

        switch(opcode) {
        
            case 0:
                  
                  nbit = NBIT(ir);
                  zbit = ZBIT(ir);
                  pbit = PBIT(ir);                                    
                  pcoffset9 = PCOFFSET9(ir);
                  
                  if (psr.n && nbit || psr.z && zbit || psr.p && pbit) {
                      pc = pc + pcoffset9;
                  }
                  
                  break;
                  
            case 9:  // NOT
                  dest_reg = DSTREG(ir);
                  src_reg = SRCREG(ir); 
                  regs[dest_reg] = ~regs[src_reg];                  
                  setCC(dest_reg);
                  break;
                  
            case 1:  // add instruction
            case 5:  // and 
                  dest_reg = DSTREG(ir);
                  src_reg = SRCREG(ir);  
                  imm_bit = IMMBIT(ir); 
                  
                  if (imm_bit) {
                      imm_val = IMMVAL(ir);
                      if (opcode == 1) 
                          regs[dest_reg] = regs[src_reg] + imm_val;
                      else 
                          regs[dest_reg] = regs[src_reg] & imm_val;

                  } else {
                      src_reg2 = SRCREG2(ir);    
                      if (opcode == 1)                  
                          regs[dest_reg] = regs[src_reg] + regs[src_reg2];
                      else 
                          regs[dest_reg] = regs[src_reg] & regs[src_reg2];
                  }
                  setCC(dest_reg); 
                  // end of add instruction
                  break;
                   
            case 2:  
            case 10: 
                      // LD
                      //  0010   110         000001010
                      //  opcode dest reg     PCoffset9

                  dest_reg = DSTREG(ir);
                  pcoffset9 = PCOFFSET9(ir);  
                  if (opcode == 2)  {
                                     
                      regs[dest_reg] = memory[pc+pcoffset9];   // LD
                      
                  } else {
                      regs[dest_reg] = memory[(unsigned short)memory[pc+pcoffset9]]; // LDI
                      //halt = 1;
                  }
                  setCC(dest_reg);
                  break;
           
            case 6:
                  dest_reg = DSTREG(ir);
                  base_reg = BASEREG(ir);     
                  offset6 = OFFSET6(ir);
                  regs[dest_reg] = memory[regs[base_reg]+offset6];  // LDR
                  setCC(dest_reg);
                  break;
                  
                  
            case 3:
            case 11:
                  src_reg = DSTREG(ir);
                  pcoffset9 = PCOFFSET9(ir);  
                  if (opcode == 3)  {                                
                      memory[pc+pcoffset9] = regs[src_reg];   // ST
                      if (memory[pc+pcoffset9] == memory[0xFE06])  {
                          c = memory[0xFE06];                          
                          printf("%c", c);
                      } 
                  } else {
                      memory[(unsigned short)memory[pc+pcoffset9]] = regs[src_reg]; // STI                      
                      if (memory[(unsigned short)memory[pc+pcoffset9]] == memory[0xFE06])  {
                          c = memory[0xFE06];                        
                          printf("%c", c);
                     }    
                  }
                  break;            
            
            case 7:  // STR
                  printf("STR\n");
                  src_reg = DSTREG(ir);
                  base_reg = BASEREG(ir);     
                  offset6 = OFFSET6(ir);
                  
                  memory[regs[base_reg]+offset6] = regs[src_reg];  // STR      
                  if (memory[regs[base_reg]+offset6] == memory[0xFE06])  {
                          c = memory[0xFE06];               
                          printf("%c", c);
                  }                      
                  break;
                  
            case 13:  // In HW5, we temporarily treat this op code like a HALT
                  halt = 1;
                  break; 
                  
            case 14: //LEA
                  dest_reg = DSTREG(ir);
                  pcoffset9 = PCOFFSET9(ir);  
                  regs[dest_reg] = pc + pcoffset9;
                  break;
                  
            case 4: //JSR
                 nbit = NBIT(ir);
                 pcoffset11 = PCOFFSET11(ir);
                 regs[7] = pc + 1;
                 pc = pc + pcoffset11;    
                 break;
                 
            case 12: //RET
                 pc = regs[7];
                 break;
                 
            case 15: //TRAP 
                pls++;
                trapvect8 = TRAPVECT8(ir);
                regs[7] = pc;
                pc = memory[trapvect8];

                break;
                  
                  
                  
        
        } // switch ends        
    }
    
    
         
      
    
}