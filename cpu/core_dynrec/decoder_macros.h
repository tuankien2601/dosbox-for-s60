/*
 *  Copyright (C) 2002-2008  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/*
	This file provides macros used by decoder.h, decoder_basic.h, decoder_opcodes.h, dyn_fpu.h
*/

#ifdef DRC_USE_SEGS_ADDR

#define MOV_SEG_VAL_TO_HOST_REG(host_reg, seg_index) gen_mov_seg16_to_reg(host_reg,(Bitu)(DRCD_SEG_VAL(seg_index)) - (Bitu)(&Segs))

#define MOV_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_mov_seg32_to_reg(host_reg,(Bitu)(DRCD_SEG_PHYS(seg_index)) - (Bitu)(&Segs))
#define ADD_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_add_seg32_to_reg(host_reg,(Bitu)(DRCD_SEG_PHYS(seg_index)) - (Bitu)(&Segs))

#else

#define MOV_SEG_VAL_TO_HOST_REG(host_reg, seg_index) gen_mov_word_to_reg(host_reg,DRCD_SEG_VAL(seg_index),false)

#define MOV_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_mov_word_to_reg(host_reg,DRCD_SEG_PHYS(seg_index),true)
#define ADD_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_add(host_reg,DRCD_SEG_PHYS(seg_index))

#endif


#ifdef DRC_USE_REGS_ADDR

#define MOV_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_mov_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_VAL(reg_index)) - (Bitu)(&cpu_regs))
#define ADD_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_add_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_VAL(reg_index)) - (Bitu)(&cpu_regs))

#define MOV_REG_WORD16_TO_HOST_REG(host_reg, reg_index) gen_mov_regval16_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,false)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD32_TO_HOST_REG(host_reg, reg_index) gen_mov_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,true)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD_TO_HOST_REG(host_reg, reg_index, dword) gen_mov_regword_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,dword)) - (Bitu)(&cpu_regs), dword)

#define MOV_REG_WORD16_FROM_HOST_REG(host_reg, reg_index) gen_mov_regval16_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,false)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD32_FROM_HOST_REG(host_reg, reg_index) gen_mov_regval32_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,true)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD_FROM_HOST_REG(host_reg, reg_index, dword) gen_mov_regword_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,dword)) - (Bitu)(&cpu_regs), dword)

#define MOV_REG_BYTE_TO_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_regbyte_to_reg_low(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))
#define MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(host_reg, reg_index, high_byte) gen_mov_regbyte_to_reg_low_canuseword(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))
#define MOV_REG_BYTE_FROM_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_regbyte_from_reg_low(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))

#else

#define MOV_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_VAL(reg_index),true)
#define ADD_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_add(host_reg,DRCD_REG_VAL(reg_index))

#define MOV_REG_WORD16_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,false),false)
#define MOV_REG_WORD32_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,true),true)
#define MOV_REG_WORD_TO_HOST_REG(host_reg, reg_index, dword) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,dword),dword)

#define MOV_REG_WORD16_FROM_HOST_REG(host_reg, reg_index) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,false),false)
#define MOV_REG_WORD32_FROM_HOST_REG(host_reg, reg_index) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,true),true)
#define MOV_REG_WORD_FROM_HOST_REG(host_reg, reg_index, dword) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,dword),dword)

#define MOV_REG_BYTE_TO_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_byte_to_reg_low(host_reg,DRCD_REG_BYTE(reg_index,high_byte))
#define MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(host_reg, reg_index, high_byte) gen_mov_byte_to_reg_low_canuseword(host_reg,DRCD_REG_BYTE(reg_index,high_byte))
#define MOV_REG_BYTE_FROM_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_byte_from_reg_low(host_reg,DRCD_REG_BYTE(reg_index,high_byte))

#endif


#define DYN_LEA_MEM_MEM(ea_reg, op1, op2, scale, imm) dyn_lea_mem_mem(ea_reg,op1,op2,scale,imm)

#if defined(DRC_USE_REGS_ADDR) && defined(DRC_USE_SEGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_segphys_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_regval_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,op1,op2_index,scale,imm)

#elif defined(DRC_USE_REGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,DRCD_SEG_PHYS(op1_index),op2_index,scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_regval_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,op1,op2_index,scale,imm)

#elif defined(DRC_USE_SEGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_segphys_mem(ea_reg,op1_index,DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_REG_VAL(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,op1,DRCD_REG_VAL(op2_index),scale,imm)

#else

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_SEG_PHYS(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_REG_VAL(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,op1,DRCD_REG_VAL(op2_index),scale,imm)

#endif
