#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h> //for malloc
#include <string.h> //for memset

#include "compiler.h"

#include "stack.h"

/*
    This function basically takes strings of the same type of insn and changes it into one instruction

    >><><<>>> = OP_ADD_CELL_POINTER 3
    ++-+-+++- = OP_ADD_CELL_VALUE -1 (but turns into an unsigned number)
*/
void scan_for_operand(const char *code, size_t len, unsigned int index, char add, char sub, CELL_TYPE *operand, size_t *consumed) {

    *operand = 0;
    *consumed = 0;

    for(; index < len; index++) {
        char c = code[index];

        //this switch will allow us to ignore non-bf characters
        switch (c) {
        case '>':
        case '<':
        case '+':
        case '-':
        case '.':
        case ',':
        case '[':
        case ']':
            if (c == add)
                (*operand)++;
            else if (c == sub)
                (*operand)--;
            else
                return;
        }

        (*consumed)++;
    }
}


#define index_equ(check_index, character) (check_index < len && code[check_index] == character)

Instruction_t next_insn(const char *code, size_t len, unsigned int index, bool optimize, size_t *consumed) {
    Instruction_t insn = {0, 0};

    unsigned int loops = 0;

    *consumed = 1;

loop:
    switch (code[index]) {
    case '>':
    case '<':
        insn.opcode = OP_ADD_CELL_POINTER;

        if (optimize)
            scan_for_operand(code, len, index, '>', '<', &insn.operand, consumed);
        else
            insn.operand = code[index] == '>' ? 1 : -1;

        break;
    case '+':
    case '-':
        insn.opcode = OP_ADD_CELL_VALUE;

        if(optimize)
            scan_for_operand(code, len, index, '+', '-', &insn.operand, consumed);
        else
            insn.operand = code[index] == '+' ? 1 : -1;

        break;
    case '.':
        insn.opcode = OP_PRINT_CELL;
        break;
    case ',':
        insn.opcode = OP_INPUT_CELL;
        break;
    case '[':
        insn.opcode = OP_OPEN_BRACKET;

        if (index_equ(index + 1, '-') && index_equ(index + 2, ']')) {
            insn.opcode = OP_SET_ZERO;
            *consumed = 3;
        }

        break;
    case ']':
        insn.opcode = OP_CLOSE_BRACKET;
        break;
    default:
        index++;
        loops++;
        goto loop;
    }

    *consumed += loops;

    return insn;
}

#define MAX_INSN 45644
uint8_t native_insn_mem[MAX_INSN]; //because malloc() can't allocate this much apparently

void op(Compiler_t *c, uint8_t opcode) { 
    if (c->pc >= MAX_INSN) {
        c->error = E_OUT_OF_MEMORY;
        return; 
    }

    c->code.native[c->pc++] = opcode; 
}

void op_2_bytes_big(Compiler_t *c, unsigned int integer) {
    op(c, ((unsigned int)(integer) >> 8) & 0xFF); //high byte of integer
    op(c, ((unsigned int)(integer) >> 0) & 0xFF); //low byte of integer
}

void op_3_bytes_big(Compiler_t *c, unsigned int integer) {
    op(c, ((unsigned int)(integer) >> 16) & 0xFF);//highest byte of integer
    op(c, ((unsigned int)(integer) >> 8) & 0xFF); //middle byte of integer
    op(c, ((unsigned int)(integer) >> 0) & 0xFF); //lowest byte of integer
} 

void op_2_bytes_little(Compiler_t *c, unsigned int integer) {
    op(c, ((unsigned int)(integer) >> 0) & 0xFF); //low byte of integer
    op(c, ((unsigned int)(integer) >> 8) & 0xFF); //high byte of integer
}

void op_3_bytes_little(Compiler_t *c, unsigned int integer) {
    op(c, ((unsigned int)(integer) >> 0) & 0xFF); //lowest byte of integer
    op(c, ((unsigned int)(integer) >> 8) & 0xFF); //middle byte of integer
    op(c, ((unsigned int)(integer) >> 16) & 0xFF);//highest byte of integer
}

//I would make the following functions inline but our toolchain compiler doesn't support that...

/*jp z, *** */
void op_jp_z(Compiler_t *c, unsigned int jump_address) {
    op(c, 0xCA);
    op_3_bytes_little(c, jump_address);
}

/*jp nz, *** */
void op_jp_nz(Compiler_t *c, unsigned int jump_address) {
    op(c, 0xC2);
    op_3_bytes_little(c, jump_address);
}

/* ld hl, (***) */
void op_load_hl_address(Compiler_t *c, unsigned int integer) {
    op(c, 0x2A); 
    op_3_bytes_little(c, integer);
}

/*ld hl, *** */
void op_load_hl(Compiler_t *c, unsigned int integer) {
    op(c, 0x21);
    op_3_bytes_little(c, integer);
}

/*ld (***), hl*/
void op_write_hl_to_address(Compiler_t *c, unsigned int address) {
    op(c, 0x22);
    op_3_bytes_little(c, address);
}

/* ld de, *** */
void op_load_de(Compiler_t *c, unsigned int integer) {
    op(c, 0x11);
    op_3_bytes_little(c, integer);
}

/*We can do optimizations on this, but the toolchain compiler does not support a line that long...*/
/* ld bc, *** */
void op_load_bc(Compiler_t *c, unsigned int integer) {
    op(c, 0x01);
    op_3_bytes_little(c, integer);
}

void op_inc_hl(Compiler_t *c) { op(c, 0x23); } /*inc hl*/
void op_dec_hl(Compiler_t *c) { op(c, 0x2B); } /*dec hl*/

void op_inc_de(Compiler_t *c) { op(c, 0x13); } /*inc de*/
void op_dec_de(Compiler_t *c) { op(c, 0x1B); } /*dec de*/

/*
Adds increment to hl register.
BEFORE:
    hl = starting value
AFTER:
    hl = starting value + increment
    bc = increment (unless preserve_bc)
*/
void op_add_hl(Compiler_t *c, unsigned int increment, bool preserve_bc) {
    if(increment == 0);
    //now before you start laughing at this, idk a better way to check unsigned numbers to be "negative"
    else if(increment == 1) {op_inc_hl(c);}
    else if(increment == 2) {op_inc_hl(c); op_inc_hl(c);}
    else if(increment == 3) {op_inc_hl(c); op_inc_hl(c); op_inc_hl(c);}
    else if(increment == -1) {op_dec_hl(c);}
    else if(increment == -2) {op_dec_hl(c); op_dec_hl(c);}
    else if(increment == -3) {op_dec_hl(c); op_dec_hl(c); op_dec_hl(c);}
    else {
        if(preserve_bc) op(c, 0xC5);   //push bc
        op_load_bc(c, increment);
        op(c, 0x09);                   //add hl, bc
        if(preserve_bc) op(c, 0xC1);   //pop bc
    }
}

/*
Adds increment to de register.
BEFORE:
    de = starting value
AFTER:
    de = starting value + increment
    bc = increment (unless preserve_bc)
*/
void op_add_de(Compiler_t *c, unsigned int increment, bool preserve_bc) {
    if(increment == 0);
    //now before you start laughing at this, idk a better way to check unsigned numbers to be "negative"
    else if(increment == 1) {op_inc_de(c);}
    else if(increment == 2) {op_inc_de(c); op_inc_de(c);}
    else if(increment == 3) {op_inc_de(c); op_inc_de(c); op_inc_de(c);}
    else if(increment == -1) {op_dec_de(c);}
    else if(increment == -2) {op_dec_de(c); op_dec_de(c);}
    else if(increment == -3) {op_dec_de(c); op_dec_de(c); op_dec_de(c);}
    else {
        op(c, 0xEB); //ex de, hl
        op_add_hl(c, increment, preserve_bc);
        op(c, 0xEB); //ex de, hl
    }
}

/*
Sets z flag if hl is zero
BEFORE:
    hl = value to check
*/
void op_check_hl_zero(Compiler_t *c) {
    op(c, 0x19);				//add hl, de
    op(c, 0xB7);				//or a
    op_2_bytes_big(c, 0xED52);	//sbc hl, de
}

/*
Sets z flag if de is zero
BEFORE:
    de = value to check
*/
void op_check_de_zero(Compiler_t *c) {
    op(c, 0xEB); //ex de, hl
    op_check_hl_zero(c);
    op(c, 0xEB); //ex de, hl
}

/*
Loads the current cell address into hl
AFTER:
    hl = current cell address
*/
void op_load_cell_address_hl(Compiler_t *c, struct Memory *mem) {
    if(c->hl != CELL_PTR) {
        op_load_hl_address(c, (unsigned int)&mem->cell_ptr);
        c->hl = CELL_PTR;
    }
}

/*
Loads the value of the cell pointed to in hl into de
BEFORE:
    hl = current cell address
AFTER:
    de = current cell value
*/
void op_load_cell_value_de(Compiler_t *c) {
    if(c->de != CELL_VALUE) {
        op_2_bytes_big(c, 0xED17); //ld de, (hl)
        c->de = CELL_VALUE;
    }
}

/*
Loads the value of the cell pointed to in hl into de
BEFORE:
    hl = current cell address
AFTER:
    hl = current cell value
*/
void op_load_cell_value_hl(Compiler_t *c) {
    if(c->hl != CELL_VALUE) {
        op_2_bytes_big(c, 0xED27); //ld hl, (hl)
        c->hl = CELL_VALUE;
    }
}

/*
Saves de into (hl)
BEFORE:
    hl = current cell address
    de = cell value
*/
void op_save_de_to_hl_address(Compiler_t *c) {
    op_2_bytes_big(c, 0xED1F); //ld (hl), de
}


void mem_Create(struct Memory *mem) {
    memset(mem->cells, 0, sizeof(mem->cells));
    mem->cell_ptr = mem->cells;
}

void comp_Create(Compiler_t *c, const char *program, size_t program_length) {
    c->program = program;
    c->program_length = program_length;
    
    c->code_length = 0;
    c->error = E_SUCCESS;

    c->pc = 0;

    c->hl = JUNK;
    c->de = JUNK;
    c->bc = 0;

    stack_Create(&c->stack);
}

void comp_CompileBytecode(Compiler_t *c, bool optimize) {
    unsigned int i;

    if (c->program == NULL) {
        c->error = E_GENERIC_COMPILE;
        return;
    }

    //calculate the amount of instructions
    i = 0;
    while (i < c->program_length) {
        size_t consumed;

        next_insn(c->program, c->program_length, i, optimize, &consumed);

        c->code_length++;

        i += consumed;
    }

    c->code.bytecode = malloc(c->code_length * sizeof(Instruction_t));

    i = 0;
    while (i < c->program_length) {
        
        size_t consumed;

        //have to split these two lines up due to a compiler error lol
        //P3: Internal Error(0x83BAF1): \ Please contact Technical Support \ make: *** [obj/compiler.obj] Error -1
        Instruction_t insn;
        insn = next_insn(c->program, c->program_length, i, optimize, &consumed);

        switch (insn.opcode) {
        case OP_OPEN_BRACKET:

            if (c->stack.top >= MAX_STACK_SIZE) {
                c->error = E_STACK_OVERFLOW;
                return;
            }

            stack_Push(&c->stack, c->pc);
            break;
        case OP_CLOSE_BRACKET:

            if (c->stack.top <= 0) {
                c->error = E_STACK_UNDERFLOW;
                return;
            }

            //set the operand of this close bracket to the index of the corresponding open bracket + 1
            insn.operand = stack_Pop(&c->stack) + 1;

            if (c->code.bytecode[insn.operand - 1].opcode != OP_OPEN_BRACKET) {
                c->error = E_GENERIC_COMPILE; //something is terribly wrong here in our code, not the bf code
                return;
            }

            //set the operand of the corresponding open bracket to the index right after this close bracket
            c->code.bytecode[insn.operand - 1].operand = c->pc + 1;
            break;
        }


        c->code.bytecode[c->pc++] = insn;
        i += consumed;
    }

}

/*
    .SIS 0x40
    .LIS 0x49
    .SIL 0x52
    .LIL 0x5B
*/
void comp_CompileNative(Compiler_t *c, struct Memory *mem, bool optimize) {
    unsigned int i;

    c->code.native = native_insn_mem;

    i = 0;
    while(i < c->program_length) {
        size_t consumed;

        //have to split these two lines up due to a compiler error lol
        //P3: Internal Error(0x83BAF1): \ Please contact Technical Support \ make: *** [obj/compiler.obj] Error -1
        Instruction_t insn;
        insn = next_insn(c->program, c->program_length, i, optimize, &consumed);

        switch (insn.opcode) {
        case OP_ADD_CELL_POINTER:

            op_load_cell_address_hl(c, mem);
            op_add_hl(c, insn.operand * sizeof(CELL_TYPE), false);
            op_write_hl_to_address(c, (unsigned int)&mem->cell_ptr);

            c->hl = CELL_PTR;
            c->de = JUNK;

            break;
        case OP_ADD_CELL_VALUE:

            if(c->hl == CELL_VALUE) {
                op_add_hl(c, insn.operand, false);
                op(c, 0xEB); //ex de, hl
                c->hl = c->de;
                op_load_cell_address_hl(c, mem);
            } else if(c->de == CELL_VALUE) {
                op_load_cell_address_hl(c, mem);
                op_add_de(c, insn.operand, false);
            } else {
                op_load_cell_address_hl(c, mem);
                op_load_cell_value_de(c);
                op_add_de(c, insn.operand, false);
            }

			op_save_de_to_hl_address(c);

            c->hl = CELL_PTR;
            c->de = CELL_VALUE;

            break;
        case OP_PRINT_CELL:

            op_load_cell_address_hl(c, mem);
            op_load_cell_value_de(c);

            op(c, 0xD5); //push de
            op(c, 0xCD); //call ***
            op_3_bytes_little(c, (unsigned int)bf_print_cell);
            op(c, 0xD1); //pop de

            c->hl = JUNK;
            c->de = CELL_VALUE;

            break;
        case OP_INPUT_CELL:

            //call the get input and store the value in de
            op(c, 0xCD); //call ***
            op_3_bytes_little(c, (unsigned int)bf_get_input);

            op(c, 0xEB); //ex de, hl

            //get the current cell ptr and store de in it
            op_load_cell_address_hl(c, mem);
			op_save_de_to_hl_address(c);
            
            c->hl = CELL_PTR;
            c->de = CELL_VALUE;

            break;
        case OP_OPEN_BRACKET:

            if(c->stack.top >= MAX_STACK_SIZE) {
				c->error = E_STACK_OVERFLOW;
                return;
            }

            if(c->de == CELL_VALUE) {
                op(c, 0xEB); //ex de, hl
            } else if(c->hl != CELL_VALUE) {
                op_load_cell_address_hl(c, mem);
                op_load_cell_value_hl(c);
            }
            
            op_check_hl_zero(c);
            op_jp_z(c, 0);

            //push this address so the corresponding ] can fix the 3 byte jump address
            //stack_Push(&stack, de);
            stack_Push(&c->stack, (unsigned int)c->code.native + c->pc);

            //these values have to remain the same as the corresponding ] for when we jump
			c->hl = CELL_VALUE;
            c->de = JUNK;

            break;
        case OP_CLOSE_BRACKET: {

            unsigned int pc_backup = 0, corresponding = 0;

            if(c->stack.top <= 0) {
				c->error = E_STACK_UNDERFLOW;
                return;
            }

            corresponding = stack_Pop(&c->stack);

            if(c->de == CELL_VALUE) {
                op(c, 0xEB); //ex de, hl
            } else if(c->hl != CELL_VALUE) {
                op_load_cell_address_hl(c, mem);
                op_load_cell_value_hl(c);
            }
            
            op_check_hl_zero(c);
            op_jp_nz(c, corresponding);

            //Go back and correct the 3 jump bytes of the corresponding [
            pc_backup = c->pc;
			c->pc = corresponding - (unsigned int)c->code.native - 3;
            op_3_bytes_little(c, (unsigned int)c->code.native + pc_backup);
			c->pc = pc_backup;

            //these values have to remain the same as the corresponding [ for when we jump
			c->hl = CELL_VALUE;
            //c->de = JUNK;

            break;
        }
        case OP_SET_ZERO:

            if(c->hl != CELL_PTR)
                op_load_hl_address(c, (unsigned int)&mem->cell_ptr);
            op_load_de(c, 0);
            op_2_bytes_big(c, 0xED1F); //ld (hl), de

			c->hl = CELL_PTR;
			c->de = CELL_VALUE;

            break;
        }

        i += consumed;
    }

    op(c, 0xC9); //ret
    c->code_length = c->pc;
}

void comp_CleanupBytecode(Compiler_t *c) {
    if(c->code.bytecode != NULL)
        free(c->code.bytecode);
}
void comp_CleanupNative(Compiler_t *c) {
    //We don't malloc() the native array so we won't free it
}







/*
void compile_native(const char *code, size_t len, bool optimize, uint8_t **native_code, size_t *native_length, struct Memory *mem, int *error) {
    unsigned int i = 0, pc = 0;

    enum reg_state hl = JUNK, de = JUNK;
    unsigned int bc;

    Stack_t stack;
    stack_Create(&stack);

    *native_code = insns;

    while(i < len) {
        size_t consumed;

        //have to split these two lines up due to a compiler error lol
        //P3: Internal Error(0x83BAF1): \ Please contact Technical Support \ make: *** [obj/compiler.obj] Error -1
        Instruction_t insn;
        insn = next_insn(code, len, i, optimize, &consumed);

        switch (insn.opcode) {
        case OP_ADD_CELL_POINTER:

            op_load_cell_address_hl();
            op_add_hl(insn.operand * sizeof(CELL_TYPE), false);
            op_write_hl_to_address(&mem->cell_ptr);

            hl = CELL_PTR;
            de = JUNK;

            break;
        case OP_ADD_CELL_VALUE:

            if(hl == CELL_VALUE) {
                op_add_hl(insn.operand, false);
                op(0xEB); //ex de, hl
                hl = de;
                op_load_cell_address_hl();
            } else if(de == CELL_VALUE) {
                op_load_cell_address_hl();
                op_add_de(insn.operand, false);
            } else {
                op_load_cell_address_hl();
                op_load_cell_value_de();
                op_add_de(insn.operand, false);
            }

            op_save_cell_value_de();

            hl = CELL_PTR;
            de = CELL_VALUE;

            break;
        case OP_PRINT_CELL:

            op_load_cell_address_hl();
            op_load_cell_value_de();

            op(0xD5); //push de
            op(0xCD); //call ***
            op_3_bytes_little(bf_print_cell);
            op(0xD1); //pop de

            hl = JUNK;
            de = CELL_VALUE;

            break;
        case OP_INPUT_CELL:

            //call the get input and store the value in de
            op(0xCD); //call ***
            op_3_bytes_little(bf_get_input);

            op(0xEB); //ex de, hl

            //get the current cell ptr and store de in it
            op_load_cell_address_hl();
            op_save_cell_value_de();
            
            hl = CELL_PTR;
            de = CELL_VALUE;

            break;
        case OP_OPEN_BRACKET:

            if(stack.top >= MAX_STACK_SIZE) {
                if(error != NULL)
                    *error = E_STACK_OVERFLOW;
                return;
            }

            if(de == CELL_VALUE) {
                op(0xEB); //ex de, hl
            } else if(hl != CELL_VALUE) {
                op_load_cell_address_hl();
                op_load_cell_value_hl();
            }
            
            op_check_hl_zero();
            op_jp_z(0);

            //push this address so the corresponding ] can fix the 3 byte jump address
            //stack_Push(&stack, de);
            stack_Push(&stack, (unsigned int)*native_code + pc);

            //these values have to remain the same as the corresponding ] for when we jump
            hl = CELL_VALUE;
            //de = JUNK;

            break;
        case OP_CLOSE_BRACKET: {

            unsigned int pc_backup = 0, corresponding = 0;

            if(stack.top <= 0) {
                if(error != NULL)
                    *error = E_STACK_UNDERFLOW;
                return;
            }

            corresponding = stack_Pop(&stack);

            if(de == CELL_VALUE) {
                op(0xEB); //ex de, hl
            } else if(hl != CELL_VALUE) {
                op_load_cell_address_hl();
                op_load_cell_value_hl();
            }
            
            op_check_hl_zero();
            op_jp_nz(corresponding);

            //Go back and correct the 3 jump bytes of the corresponding [
            pc_backup = pc;
            pc = corresponding - (unsigned int)*native_code - 3;
            op_3_bytes_little((unsigned int)*native_code + pc_backup);
            pc = pc_backup;

            //these values have to remain the same as the corresponding [ for when we jump
            hl = CELL_VALUE;
            //de = JUNK;

            break;
        }
        case OP_SET_ZERO:

            if(hl != CELL_PTR)
                op_load_hl_address(&mem->cell_ptr);
            op_load_de(0);
            op_2_bytes(0xED1F); //ld (hl), de

            hl = CELL_PTR;
            de = CELL_VALUE;

            break;
        }

        i += consumed;
    }

    op(0xC9); //ret

    *native_length = pc;
    *error = E_SUCCESS;
}
*/

#ifdef __cplusplus
}
#endif