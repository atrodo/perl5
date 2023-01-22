/*    class.c
 *
 *    Copyright (C) 2022 by Paul Evans and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This file contains the code that implements perl's new `use feature 'class'`
 * object model
 */

#include "EXTERN.h"
#define PERL_IN_CLASS_C
#include "perl.h"

#include "XSUB.h"

enum {
    PADIX_SELF   = 1,
};

void
Perl_croak_kw_unless_class(pTHX_ const char *kw)
{
    PERL_ARGS_ASSERT_CROAK_KW_UNLESS_CLASS;

/* TODO jgentle */
    if(!kw)
        croak("Cannot '' outside of a 'class'");
}

void
Perl_class_setup(pTHX_ HV *stash)
{
}

void
Perl_class_setup_method(pTHX_ CV *cv)
{
    assert(PL_comppad_name_fill == 0);

    PADOFFSET padix = allocmy("$self", 5, 0);

   assert(padix == PADIX_SELF);
   warn("p: %ld\n", padix);

   PERL_UNUSED_VAR(padix);
}

OP *
Perl_class_wrap_method_body(pTHX_ OP *o)
{
    PERL_ARGS_ASSERT_CLASS_WRAP_METHOD_BODY;

    if(!o)
        return o;

    /* If this is an empty method body then o will be an OP_STUB and not a
     * list. This will confuse op_sibling_splice() */
    if(o->op_type != OP_LINESEQ)
        o = newLISTOP(OP_LINESEQ, 0, o, NULL);

    op_sibling_splice(o, NULL, 0, newUNOP_AUX(OP_METHSTART, 0, NULL, NULL));

    return o;
}

OP *
Perl_class_op_accessor_get(pTHX_ SV *name)
{
    GV *gv;
    OP *o;
    I32 startsub = start_subparse(FALSE, 0);
/*
    STRLEN namlen = 0;
    const char * const name = o ? SvPV_const(cSVOPo->op_sv, namlen) : NULL;
*/
I32 save_ix = block_start(TRUE);

    //OP * body = newUNOP_AUX(OP_MULTIDEREF, 0, NULL, arg_buf);
    warn("l: %d\n", __LINE__);
    //gv = gv_fetchsv(name, GV_NOADD_NOINIT, SVt_PVCV);
    //gv = gv_fetchpvs("_", GV_NOADD_NOINIT, SVt_PV);
    OP *name_op = newSVOP(OP_CONST, 0, SvREFCNT_inc(name));
    warn("l: %d\n", __LINE__);

    SV *fname = newSVpvn(HvNAME(PL_curstash), HvNAMELEN(PL_curstash));
    sv_catpv(fname, "::");
    sv_catsv(fname, name);
    STRLEN name_len;
    const char *name_cc = SvPV_const(name, name_len);
    //SV **name_gv = hv_fetch(PL_curstash, name_cc, name_len, TRUE);
    GV *name_gv = gv_fetchsv(fname, GV_ADDMULTI | GV_NOINIT, SVt_PVCV);
    gv_init_pvn(name_gv, PL_curstash, name_cc, name_len, 0);

    warn("l: %d\n", (int)name_gv);
    warn("l: %d\n", isGV(name_gv));
    warn("s: %s\n", HvNAME(PL_curstash));
    warn("s: %s\n", SvPV_nolen(fname));
    warn("l: %d\n", __LINE__);

    SV *l = newSV(0);
    gv_fullname3(l, name_gv, NULL);
    warn("s: %s\n", SvPV_nolen(name));

    init_named_cv(PL_compcv, name_op);
    PL_parser->in_my = 0;
    PL_parser->in_my_stash = NULL;
    warn("l: %d\n", __LINE__);

    warn("l: %d\n", __LINE__);
    /*
    o = newUNOP(OP_RV2AV, OPf_REF,
        newGVOP(OP_GV, 0, PL_defgv));
        */

    o = doref(newAVREF(newGVOP(OP_GV, 0, PL_defgv)),OP_RV2AV, TRUE);

    warn("l: %d\n", __LINE__);
    //o = newBINOP(OP_AELEM, 0, o, newSVOP(OP_CONST, 0, newSViv(0)));
    o = doref(newBINOP(OP_AELEM, 0, o, newSVOP(OP_CONST, 0, newSViv(0))), OP_RV2HV, TRUE);

    warn("l: %d\n", __LINE__);
    o = newUNOP(OP_RV2HV, OPf_REF, o);

    warn("l: %d\n", __LINE__);
    warn("s: %s\n", SvPV_nolen(name));
    //o = newBINOP(OP_HELEM, 0, o, newSVOP(OP_CONST, 0, name));

    /*
    PADNAME *pn = PadnamelistARRAY(PL_comppad_name)[name_op->op_targ];
    warn("l: %d\n", __LINE__);
    SV *name = newSVpvn(PadnamePV(pn), PadnameLEN(pn));
    warn("l: %d\n", __LINE__);
    */

    SvREFCNT_inc(name);
    o = newBINOP(OP_HELEM, 0, o, name_op);
    warn("l: %d\n", __LINE__);
    //sigops = op_prepend_elem(OP_LINESEQ, check, sigops);

    o = newSTATEOP(0, NULL, o);
    warn("l: %d\n", __LINE__);

o = block_end(save_ix, o);
    SAVEFREESV(PL_compcv);
    warn("l: %d\n", __LINE__);

    warn("l: %d\n", __LINE__);
    SvREFCNT_inc_simple_void(PL_compcv);
    warn("l: %d\n", __LINE__);
    warn("s: %s\n", SvPV_nolen(name));
    CV *cv = newATTRSUB_x(startsub, (OP *)name_gv, NULL, NULL, o, TRUE);
    warn("l: %d\n", __LINE__);
    warn("s: %s\n", SvPV_nolen(name));
    warn("s: %s\n", HvNAME(PL_curstash));
    //warn("s: %s\n", SvPV_nolen(GvSV(CvGV(cv))));
    /*
    intro_my();
    warn("l: %d\n", __LINE__);
    PL_parser->parsed_sub = 1;
    warn("l: %d\n", __LINE__);
    */
{
STRLEN namelen;
      const char *n = SvPV_const(name, namelen);
      U32 hash;
      PERL_HASH(hash, n, namelen);
      //ASSUME(!CvNAME_HEK(o));
 
      CvNAME_HEK_set(cv, share_hek(n, namelen, hash));
}

    return NULL;
}

/* TODO: People would probably expect to find this in pp.c  ;) */
PP(pp_methstart)
{
    SV *self = av_shift(GvAV(PL_defgv));
    SV *rv = NULL;

    /* pp_methstart happens before the first OP_NEXTSTATE of the method body,
     * meaning PL_curcop still points at the callsite. This is useful for
     * croak() messages. However, it means we have to find our current stash
     * via a different technique.
     */
    CV *curcv;
    if(LIKELY(CxTYPE(CX_CUR()) == CXt_SUB))
        curcv = CX_CUR()->blk_sub.cv;
    else
        curcv = find_runcv(NULL);

    if(!SvROK(self) ||
        !SvOBJECT((rv = SvRV(self))))
        /* TODO: check it's in an appropriate class */
        croak("Cannot invoke method on a non-instance");

    /* TODO: When we implement inheritence we'll have to do something fancier here */

    save_clearsv(&PAD_SVl(PADIX_SELF));
    sv_setsv(PAD_SVl(PADIX_SELF), self);

    return NORMAL;
}

XS(XS_CLASS_new);
XS(XS_CLASS_new)
{
    dXSARGS;

    SV *self = NULL;
    croak("new was called, how dare you\n");

    EXTEND(SP, 1);
    ST(0) = self;
    XSRETURN(1);
}

struct xsub_details {
    const char *name;
    XSUBADDR_t xsub;
};

static const struct xsub_details these_details[] = {
    {"CLASS::new", XS_CLASS_new },
};

void
Perl_boot_core_CLASS(pTHX)
{
    static const char file[] = __FILE__;
    const struct xsub_details *xsub = these_details;
    const struct xsub_details *end = C_ARRAY_END(these_details);

    do {
        CV *cv = newXS_flags(xsub->name, xsub->xsub, file, NULL, 0);
    } while (++xsub < end);
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
