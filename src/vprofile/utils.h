#ifndef __UTILS_H__
#define __UTILS_H__
#include "dr_api.h"

// TODO: search for dynamorio interface to obtain element witdh of a SIMD operation
// As the dynamorio did not provide function to obtain SIMD operation width of each element, Zerospy mannually implements with a lookup table
#ifdef ARM_CCTLIB

#ifdef ARM

uint32_t
FloatOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        // ignore those non-vectorized instructions (not start with OP_v*)
        return size;
    }
    int opc = instr_get_opcode(instr);

    switch (opc) {

    /* convert instructions */
    case OP_vcvt_f32_f16:
    case OP_vcvtb_f32_f16:
    case OP_vcvtb_f64_f16:
    case OP_vcvtt_f32_f16:
    case OP_vcvtt_f64_f16:
    /* end of convert instructions */
    case OP_vrev16_16:  // ?
    case OP_vrev32_16:  // ?
    case OP_vrev64_16:  // ?
    case OP_vtrn_16:  // ?
    case OP_vtst_16:  // ?
    case OP_vuzp_16:  // ?
    case OP_vzip_16:  // ?
        return 2;

    case OP_vabs_f32:
    case OP_vacge_f32:
    case OP_vacgt_f32:
    case OP_vadd_f32:
    case OP_vceq_f32:
    case OP_vcge_f32:
    case OP_vcgt_f32:
    case OP_vcle_f32:
    case OP_vclt_f32:
    case OP_vcmp_f32:
    case OP_vcmpe_f32:
    /* convert instructions */
    case OP_vcvt_f16_f32:
    case OP_vcvt_f64_f32:
    case OP_vcvt_s16_f32:
    case OP_vcvt_s32_f32:
    case OP_vcvt_u16_f32:
    case OP_vcvt_u32_f32:
    case OP_vcvta_s32_f32:
    case OP_vcvta_u32_f32:
    case OP_vcvtb_f16_f32:
    case OP_vcvtm_s32_f32:
    case OP_vcvtm_u32_f32:
    case OP_vcvtn_s32_f32:
    case OP_vcvtn_u32_f32:
    case OP_vcvtp_s32_f32:
    case OP_vcvtp_u32_f32:
    case OP_vcvtr_s32_f32:
    case OP_vcvtr_u32_f32:
    case OP_vcvtt_f16_f32:
    /* end of convert instructions */
    case OP_vdiv_f32:
    case OP_vfma_f32:
    case OP_vfms_f32:
    case OP_vfnma_f32:
    case OP_vfnms_f32:
    case OP_vmax_f32:
    case OP_vmaxnm_f32:
    case OP_vmin_f32:
    case OP_vminnm_f32:
    case OP_vmla_f32:
    case OP_vmls_f32:
    case OP_vmov_f32:
    case OP_vmul_f32:
    case OP_vneg_f32:
    case OP_vnmla_f32:
    case OP_vnmls_f32:
    case OP_vnmul_f32:
    case OP_vpadd_f32:
    case OP_vpmax_f32:
    case OP_vpmin_f32:
    case OP_vrecpe_f32:
    case OP_vrecps_f32:
    case OP_vrev32_32:  // ?
    case OP_vrev64_32:  // ?
    case OP_vrinta_f32_f32:  // ?
    case OP_vrintm_f32_f32:  // ?
    case OP_vrintn_f32_f32:  // ?
    case OP_vrintp_f32_f32:  // ?
    case OP_vrintr_f32:  // ?
    case OP_vrintx_f32:  // ?
    case OP_vrintx_f32_f32:  // ?
    case OP_vrintz_f32:  // ?
    case OP_vrintz_f32_f32:  // ?
    case OP_vrsqrte_f32:
    case OP_vrsqrts_f32:
    case OP_vsel_eq_f32:
    case OP_vsel_ge_f32:
    case OP_vsel_gt_f32:
    case OP_vsel_vs_f32:
    case OP_vsqrt_f32:
    case OP_vsub_f32:
    case OP_vtrn_32:  // ?
    case OP_vtst_32:  // ?
    case OP_vuzp_32:  // ?
    case OP_vzip_32:  // ?
        return 4;

    case OP_vabs_f64:
    case OP_vadd_f64:
    case OP_vcmp_f64:
    case OP_vcmpe_f64:
    /* convert instructions */
    case OP_vcvt_f32_f64:
    case OP_vcvt_s16_f64:
    case OP_vcvt_s32_f64:
    case OP_vcvt_u16_f64:
    case OP_vcvt_u32_f64:
    case OP_vcvta_s32_f64:
    case OP_vcvta_u32_f64:
    case OP_vcvtb_f16_f64:
    case OP_vcvtm_s32_f64:
    case OP_vcvtm_u32_f64:
    case OP_vcvtn_s32_f64:
    case OP_vcvtn_u32_f64:
    case OP_vcvtp_s32_f64:
    case OP_vcvtp_u32_f64:
    case OP_vcvtr_s32_f64:
    case OP_vcvtr_u32_f64:
    case OP_vcvtt_f16_f64:
    /* end of convert instructions */
    case OP_vdiv_f64:
    case OP_vfma_f64:
    case OP_vfms_f64:
    case OP_vfnma_f64:
    case OP_vfnms_f64:
    case OP_vmaxnm_f64:
    case OP_vminnm_f64:
    case OP_vmla_f64:
    case OP_vmls_f64:
    case OP_vmov_f64:
    case OP_vmul_f64:
    case OP_vneg_f64:
    case OP_vnmla_f64:
    case OP_vnmls_f64:
    case OP_vnmul_f64:
    case OP_vrinta_f64_f64:  // ?
    case OP_vrintm_f64_f64:  // ?
    case OP_vrintn_f64_f64:  // ?
    case OP_vrintp_f64_f64:  // ?
    case OP_vrintr_f64:  // ?
    case OP_vrintx_f64:  // ?
    case OP_vrintz_f64:  // ?
    case OP_vsel_eq_f64:
    case OP_vsel_ge_f64:
    case OP_vsel_gt_f64:
    case OP_vsel_vs_f64:
    case OP_vsqrt_f64:
    case OP_vsub_f64:
        return 8;

    default: return 0;
    }
}

uint32_t
IntegerOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        return size;
    }

    int opc = instr_get_opcode(instr);

    switch (opc) {
        default: {
            return 0;
        }
    }
}

#else

uint32_t
FloatOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        // ignore those non-vectorized instructions (not start with OP_v*)
        return size;
    }

    opnd_t width = instr_get_src(instr, instr_num_srcs(instr) - 1);
    if (!opnd_is_immed_int(width)) {
        int opc = instr_get_opcode(instr);
        switch (opc) {
            // actual esize is decided by instr encode, should be ignore?
            case OP_dup:
            case OP_movi:
            case OP_stp:
            case OP_addp:
            case OP_cmeq:
            case OP_ld1:
            case OP_orr:
            case OP_and:
            case OP_str:
            case OP_stur:
            case OP_ldp:
            case OP_ldr:
            case OP_ldur:
            case OP_ld2:
            case OP_uminv:
                    return 8;
            default: return 0;
        }
    }

    switch (opnd_get_immed_int(width)) {
        case VECTOR_ELEM_WIDTH_HALF:
            return 2;
        case VECTOR_ELEM_WIDTH_SINGLE:
            return 4;
        case VECTOR_ELEM_WIDTH_DOUBLE:
            return 8;
        default: {
            int opc = instr_get_opcode(instr);
            switch (opc) {
                case OP_dup:
                case OP_movi:
                case OP_stp:
                case OP_addp:
                case OP_cmeq:
                case OP_ld1:
                case OP_orr:
                case OP_and:
                case OP_str:
                case OP_stur:
                case OP_ldp:
                case OP_ldr:
                case OP_ldur:
                case OP_ld2:
                case OP_uminv:
                        return 8;
                default: return 0;
            }
        }
    }
}

uint32_t
IntegerOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        return size;
    }

    int opc = instr_get_opcode(instr);

    switch (opc) {
        case OP_dup:
        case OP_movi:
        case OP_stp:
        case OP_addp:
        case OP_cmeq:
        case OP_ld1:
        case OP_orr:
        case OP_and:
        case OP_str:
        case OP_stur:
        case OP_ldp:
        case OP_ldr:
        case OP_ldur:
        case OP_ld2:
                return 8;
        default: {
            return 0;
        }
    }
}

#endif

#else

bool instr_is_reg_copy(instr_t* instr) {
    if(instr_reads_memory(instr) || instr_writes_memory(instr)) {
        return false;
    }
    if(instr_is_mov(instr)) return true;
    int opcode = instr_get_opcode(instr);
    switch(opcode) {
        case OP_movdqu:
        case OP_movdqa:
        case OP_movsd:
        case OP_movss:
        case OP_vmovss:
        case OP_vmovsd:
        case OP_vmovd:
        case OP_vmovq:
        case OP_movd:
        case OP_movq:
        case OP_movapd:
        case OP_movaps:
        case OP_vmovapd:
        case OP_vmovaps:
        case OP_movupd:
        case OP_movups:
        case OP_vmovupd:
        case OP_vmovups:
            return true;
    }
    return false;
}

uint32_t
FloatOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        // ignore those non-vectorized instructions (not start with OP_v*)
        return size;
    }
    int opc = instr_get_opcode(instr);

    switch (opc) {

    // TODO: packed 128-bit floating-point, treat them as two double floating points
    case OP_vinsertf128:
    case OP_vextractf128:
    case OP_vbroadcastf128:
    case OP_vperm2f128:
        return 8;
        //return 16;

    case OP_movss:
    case OP_movups:
    case OP_movaps:

    case OP_vmovss:
    case OP_vmovups:
    case OP_vmovlps:
    case OP_vmovsldup:
    case OP_vmovhps:
    case OP_vmovshdup:
    case OP_vmovaps:
    case OP_vmovntps:
    case OP_unpcklps:
    case OP_unpckhps:
    case OP_vunpcklps:
    case OP_vunpckhps:
    case OP_extractps:
    case OP_insertps:
    case OP_vextractps:
    case OP_vinsertps:
    case OP_vbroadcastss:
    case OP_vpermilps:
    case OP_vmaskmovps:
    case OP_vshufps:
        return 4;

    case OP_subpd:
    case OP_divpd:
    case OP_movupd:
    case OP_movsd:
    case OP_andpd:
    case OP_shufpd:
    case OP_xorpd:
    case OP_orpd:
    case OP_andnpd:

    case OP_vmovsd:
    case OP_vmovupd:
    case OP_vmovlpd:
    case OP_vmovddup:
    case OP_vmovhpd:
    case OP_movapd:
    case OP_vmovapd:
    case OP_vmovntpd:
    case OP_unpcklpd:
    case OP_unpckhpd:
    case OP_vunpcklpd:
    case OP_vunpckhpd:
    case OP_vbroadcastsd:
    case OP_vpermilpd:
    case OP_vmaskmovpd:
    case OP_vshufpd:
        return 8;

    /* Ignore Convert instructions */

    /* SSE3/3D-Now!/SSE4 */
    case OP_haddps:
    case OP_hsubps:
    case OP_addsubps:
    case OP_femms:
    case OP_movntss:
    case OP_blendvps:
    case OP_roundps:
    case OP_roundss:
    case OP_blendps:
    case OP_dpps:
        return 4;

    case OP_haddpd:
    case OP_hsubpd:
    case OP_addsubpd:
    case OP_movntsd:
    case OP_blendvpd:
    case OP_roundpd:
    case OP_roundsd:
    case OP_blendpd:
    case OP_dppd:
        return 8;

    case OP_xorps:
    case OP_andps:
    case OP_orps:
    case OP_cvtpd2ps:
    case OP_cvttpd2dq://?
    case OP_rcpps:

    /* AVX */
    case OP_vcvtsd2ss://?
    
    case OP_vucomiss:
    case OP_vcomiss:
    case OP_vmovmskps:
    case OP_vsqrtps:
    case OP_vsqrtss:
    case OP_vrsqrtps:
    case OP_vrsqrtss:
    case OP_vrcpps:
    case OP_vrcpss:
    case OP_vandps:
    case OP_vandnps:
    case OP_vorps:
    case OP_vxorps:
    case OP_vaddps:
    case OP_vaddss:
    case OP_vmulps:
    case OP_vmulss:
    case OP_vsubss:
    case OP_vsubps:
    case OP_vminps:
    case OP_vminss:
    case OP_vdivps:
    case OP_vdivss:
    case OP_vmaxps:
    case OP_vmaxss:
    case OP_vcmpps:
    case OP_vcmpss:
    case OP_vhaddps:
    case OP_vhsubps:
    case OP_vaddsubps:
    case OP_vblendvps:
    case OP_vroundps:
    case OP_vroundss:
    case OP_vblendps:
    case OP_vdpps:
    case OP_vtestps:
        return 4;

    case OP_cvtdq2pd://?
    case OP_vcvtsi2sd://?
    case OP_vcvtss2sd://?

    case OP_sqrtpd:
    case OP_maxpd:
    case OP_cmppd:
    case OP_cvtps2pd:

    case OP_vucomisd:
    case OP_vcomisd:
    case OP_vmovmskpd:
    case OP_vsqrtpd:
    case OP_vsqrtsd:
    case OP_vandpd:
    case OP_vandnpd:
    case OP_vorpd:
    case OP_vxorpd:
    case OP_vaddpd:
    case OP_vaddsd:
    case OP_vmulpd:
    case OP_vmulsd:
    case OP_vsubpd:
    case OP_vsubsd:
    case OP_vminpd:
    case OP_vminsd:
    case OP_vdivpd:
    case OP_vdivsd:
    case OP_vmaxpd:
    case OP_vmaxsd:
    case OP_vcmppd:
    case OP_vcmpsd:
    case OP_vhaddpd:
    case OP_vhsubpd:
    case OP_vaddsubpd:
    case OP_vblendvpd:
    case OP_vroundpd:
    case OP_vroundsd:
    case OP_vblendpd:
    case OP_vdppd:
    case OP_vtestpd:
        return 8;

    /* SSE packed instruction */
    case OP_addpd:
    case OP_mulpd:
        return 8;

    case OP_addps:
    case OP_mulps:
        return 4;

    /* FMA */
    case OP_vfmadd132ps:
    case OP_vfmadd213ps:
    case OP_vfmadd231ps:
    case OP_vfmadd132ss:
    case OP_vfmadd213ss:
    case OP_vfmadd231ss:
    case OP_vfmaddsub132ps:
    case OP_vfmaddsub213ps:
    case OP_vfmaddsub231ps:
    case OP_vfmsubadd132ps:
    case OP_vfmsubadd213ps:
    case OP_vfmsubadd231ps:
    case OP_vfmsub132ps:
    case OP_vfmsub213ps:
    case OP_vfmsub231ps:
    case OP_vfmsub132ss:
    case OP_vfmsub213ss:
    case OP_vfnmadd132ps:
    case OP_vfnmadd213ps:
    case OP_vfnmadd231ps:
    case OP_vfnmadd132ss:
    case OP_vfnmadd213ss:
    case OP_vfnmadd231ss:
    case OP_vfnmsub213ps:
    case OP_vfnmsub132ss:
    case OP_vfnmsub213ss:
    case OP_vfnmsub231ss:
        return 4;

    case OP_vfmadd132pd:
    case OP_vfmadd213pd:
    case OP_vfmadd231pd:
    case OP_vfmadd132sd:
    case OP_vfmadd213sd:
    case OP_vfmadd231sd:
    case OP_vfmaddsub132pd:
    case OP_vfmaddsub213pd:
    case OP_vfmaddsub231pd:
    case OP_vfmsubadd132pd:
    case OP_vfmsubadd213pd:
    case OP_vfmsubadd231pd:
    case OP_vfmsub132pd:
    case OP_vfmsub213pd:
    case OP_vfmsub231pd:
    case OP_vfmsub132sd:
    case OP_vfmsub213sd:
    case OP_vfmsub231ss:
    case OP_vfmsub231sd:
    case OP_vfnmadd132pd:
    case OP_vfnmadd213pd:
    case OP_vfnmadd231pd:
    case OP_vfnmadd132sd:
    case OP_vfnmadd213sd:
    case OP_vfnmadd231sd:
    case OP_vfnmsub132ps:
    case OP_vfnmsub132pd:
    case OP_vfnmsub213pd:
    case OP_vfnmsub231ps:
    case OP_vfnmsub231pd:
    case OP_vfnmsub132sd:
    case OP_vfnmsub213sd:
    case OP_vfnmsub231sd:
        return 8;

    default: return 0;
    }
}

uint32_t
IntegerOperandSizeTable(instr_t *instr, opnd_t opnd)
{
    uint size = opnd_size_in_bytes(opnd_get_size(opnd));
    if(size!=16 && size!=32 && size!=64) {
        return size;
    }

    int opc = instr_get_opcode(instr);

    switch (opc) {
        case OP_vpalignr:
        case OP_vpbroadcastb:
        case OP_vpcmpeqb:
        case OP_vpminub:
        case OP_vpcmpgtb:
        case OP_vpsubb:

        case OP_pmovmskb:
        case OP_pcmpeqb:
	    case OP_paddb:
        case OP_psubb:
        case OP_pminub:
        case OP_pshufb:
            return 1;

        case OP_punpcklwd:
        case OP_punpcklbw:
            return 2;

        case OP_vmovd:
        case OP_vpandn:

        case OP_movd:

        case OP_psubd:
        case OP_pmuludq:
        case OP_pcmpgtd:
        case OP_pslld:
        case OP_psrld:
        case OP_psrad:
        case OP_pcmpeqd:
        case OP_punpckldq:
        case OP_pshufd:
        case OP_paddd:
            return 4;

        case OP_vmovq:

        case OP_movq:

        case OP_psrlq:
        case OP_psllq:
        case OP_pand:
        case OP_pandn:
        case OP_pxor:
        case OP_por:
        case OP_punpcklqdq:
        case OP_punpckhqdq:
        case OP_paddq:
        case OP_psubq:
            return 8;

        case OP_vpslldq:

        case OP_pslldq:
        case OP_psrldq:
        case OP_movdqa:
        case OP_movdqu:
            return 16;

        case OP_vmovdqu:
        case OP_vmovdqa:
        case OP_vpor:
        case OP_vpxor:
        case OP_vpand:
            return size;
        default: {
            return 0;
        }
    }
}

#endif

#endif

#if defined(ARM) || defined(AARCH64)
bool instr_is_floating_self(instr_t* instr) {
    int num_srcs = instr_num_srcs(instr);
    for(int i=0; i<num_srcs; ++i) {
        opnd_t opnd = instr_get_src(instr, i);
        if(!opnd_is_memory_reference(opnd)) {
            for(int j=0; j<opnd_num_regs_used(opnd); ++j) {
                reg_id_t reg_used = opnd_get_reg_used(opnd, j);
                // check for whether the register is floating point
                if(reg_used>=DR_REG_Q0 && reg_used<=DR_REG_B31) {
                    return true;
                }
            }
        }
    }
    int num_dsts = instr_num_dsts(instr);
    for(int i=0; i<num_dsts; ++i) {
        opnd_t opnd = instr_get_dst(instr, i);
        if(!opnd_is_memory_reference(opnd)) {
            for(int j=0; j<opnd_num_regs_used(opnd); ++j) {
                reg_id_t reg_used = opnd_get_reg_used(opnd, j);
                // check for whether the register is floating point
                if(reg_used>=DR_REG_Q0 && reg_used<=DR_REG_B31) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool instr_is_floating(instr_t* instr) {
    bool is_float = instr_is_floating_self(instr);
    // if it is memory load operation, check for the defined register usage
    if(!is_float && instr_reads_memory(instr)) {
        // extract defined register
        int num_dsts = instr_num_dsts(instr);
        for(int i=0; i<num_dsts; ++i) {
            opnd_t opnd = instr_get_dst(instr, i);
            // we ignore the non-register dst opnd
            if(opnd_is_reg(opnd)) {
                reg_id_t reg_def = opnd_get_reg(opnd);
                for (instr_t* ins = instr; ins != NULL; ins = instr_get_next(ins)) {
                    // Here, we aggressively suppose that if reg_def is used in a
                    // instruction and it is a floating point instruction, this
                    // instruction should be parsed as floating point reads.
                    int num_srcs = instr_num_srcs(ins);
                    for(int j=0; j<num_srcs; ++j) {
                        opnd_t opnd = instr_get_src(ins, j);
                        if (opnd_is_reg(opnd) && opnd_get_reg(opnd) == reg_def) {
                            return instr_is_floating_self(ins);
                        }
                    }
                }
            } else {
#ifdef DEBUG
                dr_fprintf(STDERR, "??? Non register dst operand!\n");
#endif
            }
        }
    }
    return is_float;
}
#endif

// Some instr may result in floating point instruction, 
// while opnd may be integer (e.g., convert instruction)
bool opnd_is_floating(instr_t* instr, opnd_t opnd) {
    // currently, we just simply return the result of the instruction type
    // TODO: make a more accurate table for X86/ARM
    return instr_is_floating(instr);
}
