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
#include "keywords.h"

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
    assert(PL_comppad_name_fill == 0);

    U16 old_in_my = PL_parser->in_my;
    PL_parser->in_my = KEY_our;
    PADOFFSET padix = allocmy("%HAS", 4, 0);
    PL_parser->in_my = old_in_my;

    SV *isa_sv = newSVpvn(HvNAME(PL_curstash), HvNAMELEN(PL_curstash));
    sv_catpv(isa_sv, "::ISA");

    GV *const isa_gv = gv_fetchsv(isa_sv, GV_ADD | GV_ADDMULTI, SVt_PVAV);
    AV *const isa = GvAVn(isa_gv);
    av_push(isa, newSVpvn("CLASS", 5));
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

void
Perl_class_extends(pTHX_ OP *parent_op)
{
    SV *parent;
    SV *isa_sv;

    if (parent_op->op_type != OP_CONST)
        Perl_croak(aTHX_ "Module name must be constant");

    parent = newSVsv(cSVOPx(parent_op)->op_sv);
    isa_sv = newSVpvn(HvNAME(PL_curstash), HvNAMELEN(PL_curstash));
    sv_catpv(isa_sv, "::ISA");

    GV *const isa_gv = gv_fetchsv(isa_sv, GV_ADD | GV_ADDMULTI, SVt_PVAV);
    AV *const isa = GvAVn(isa_gv);
    av_push(isa, parent);
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
Perl_class_op_field(pTHX_ SV *name)
{
    OP *o;
    OP *name_op = newSVOP(OP_CONST, 0, SvREFCNT_inc(name));
    warn("ss: %s\n", SvPV_nolen(name));

    PADOFFSET tmp = pad_findmy_pvn("$self", 5, 0);
    warn("pi: %ld\n", tmp);

    o = newOP(OP_PADSV, 0);
    o->op_targ = tmp;

    o = doref(newHVREF(o), OP_RV2HV, TRUE);

    o = newBINOP(OP_HELEM, 0, o, name_op);

    return o;
}

OP *
Perl_class_op_define_field(pTHX_ OP *name_op, OP *attrs)
{
    PADNAME *pn = PadnamelistARRAY(PL_comppad_name)[name_op->op_targ];
    SV *name = newSVpvn(PadnamePV(pn)+1, PadnameLEN(pn)-1);

    ENTER;
    SAVEVPTR(PL_curcop);
    lex_start(NULL, NULL, LEX_START_SAME_FILTER);
    I32 floor = start_subparse(FALSE, 0);

    OP *pack = newSVOP(OP_CONST, 0, newSVpvs("CLASS"));
    OP *modname = newSVOP(OP_CONST, 0, newSVpvs("CLASS.pm"));
    SV *meth = newSVpvs_share("_DEFINE_FIELD");
    SV *pkg = newSVhek(HvNAME_HEK(PL_curstash));
    SV *new_name = newSVpvs("");

    OP *arg = newSVOP(OP_CONST, 0, pkg);
    arg = op_append_elem(OP_LIST, arg, newSVOP(OP_CONST, 1, newSVsv(name)));
    arg = op_append_elem(OP_LIST, arg, newSVOP(OP_CONST, 1, newRV(new_name)));

    OP *imop = op_convert_list(OP_ENTERSUB, OPf_STACKED,
		   op_append_elem(OP_LIST,
			       arg,
			       newMETHOP_named(OP_METHOD_NAMED, 0, meth)
		   ));

    newATTRSUB(floor,
        newSVOP(OP_CONST, 0, newSVpvs_share("BEGIN")),
        NULL,
        NULL,
        op_append_elem(OP_LINESEQ,
            op_append_elem(OP_LINESEQ,
                newSTATEOP(0, NULL, newUNOP(OP_REQUIRE, 0, modname)),
                newSTATEOP(0, NULL, NULL)),
            newSTATEOP(0, NULL, imop) ));

    LEAVE;
    warn("de: %s\n", SvPV_nolen(new_name));
    warn("de: %d\n", __LINE__);

    return NULL;
}

OP *
Perl_class_op_accessor_get(pTHX_ SV *name)
{
    GV *gv;
    OP *o;
    I32 startsub = start_subparse(FALSE, 0);
    I32 save_ix = block_start(TRUE);

    OP *name_op = newSVOP(OP_CONST, 0, SvREFCNT_inc(name));

    SV *fname = newSVpvn(HvNAME(PL_curstash), HvNAMELEN(PL_curstash));
    sv_catpv(fname, "::");
    sv_catsv(fname, name);

    GV *name_gv = gv_fetchsv(fname, GV_ADDMULTI | GV_NOINIT, SVt_PVCV);
    gv_init_sv(name_gv, PL_curstash, name, 0);

    init_named_cv(PL_compcv, name_op);
    PL_parser->in_my = 0;
    PL_parser->in_my_stash = NULL;

    class_setup_method(PL_compcv);

    // pad_findmy_pvn was returning -1, so right now we know self_po is 1
    PADOFFSET self_po = 1;
    o = newSVOP(OP_FIELDSV, 0, name);
    o->op_targ = self_po;

    o = newSTATEOP(0, NULL, o);

    o = block_end(save_ix, o);
    o = class_wrap_method_body(o);
    SAVEFREESV(PL_compcv);

    SvREFCNT_inc_simple_void(PL_compcv);
    CV *cv = newATTRSUB_x(startsub, (OP *)name_gv, NULL, NULL, o, TRUE);
    {
        STRLEN namelen;
        const char *n = SvPV_const(name, namelen);
        U32 hash;
        PERL_HASH(hash, n, namelen);

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

PP(pp_fieldsv)
{
    dSP;

    EXTEND(SP, 1);
    HE* he;
    SV **svp;
    const U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    const U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    const bool localizing = PL_op->op_private & OPpLVAL_INTRO;
    bool preeminent = TRUE;

    SV *sv;
    SV *self = PAD_SVl(PL_op->op_targ);

    if (!self || !SvROK(self)) { warn("About to die\n"); }
    HV *self_hv = (HV *)SvRV(self);
    HV *hv = self_hv;

    SV * const field = cSVOP_sv;

    if (localizing) {
        MAGIC *mg;
        HV *stash;

        /* If we can determine whether the element exists,
         * Try to preserve the existenceness of a tied hash
         * element by using EXISTS and DELETE if possible.
         * Fallback to FETCH and STORE otherwise. */
        if (SvCANEXISTDELETE(hv))
            preeminent = hv_exists_ent(hv, field, 0);
    }

    he = hv_fetch_ent(hv, field, lval && !defer, 0);
    svp = he ? &HeVAL(he) : NULL;
    if (lval) {
        if (!svp || !*svp || *svp == &PL_sv_undef) {
            SV* lv;
            SV* key2;
            if (!defer) {
                DIE(aTHX_ PL_no_helem_sv, SVfARG(field));
            }
            lv = newSV_type_mortal(SVt_PVLV);
            LvTYPE(lv) = 'y';
            sv_magic(lv, key2 = newSVsv(field), PERL_MAGIC_defelem, NULL, 0);
            SvREFCNT_dec_NN(key2);      /* sv_magic() increments refcount */
            LvTARG(lv) = SvREFCNT_inc_simple_NN(hv);
            LvTARGLEN(lv) = 1;
            PUSHs(lv);
            RETURN;
        }
        if (localizing) {
            if (HvNAME_get(hv) && isGV_or_RVCV(*svp))
                save_gp(MUTABLE_GV(*svp), !(PL_op->op_flags & OPf_SPECIAL));
            else if (preeminent)
                save_helem_flags(hv, field, svp,
                     (PL_op->op_flags & OPf_SPECIAL) ? 0 : SAVEf_SETMAGIC);
            else
                SAVEHDELETE(hv, field);
        }
        else if (PL_op->op_private & OPpDEREF) {
            PUSHs(vivify_ref(*svp, PL_op->op_private & OPpDEREF));
            RETURN;
        }
    }
    sv = (svp && *svp ? *svp : &PL_sv_undef);
    /* Originally this did a conditional C<sv = sv_mortalcopy(sv)>; this
     * was to make C<local $tied{foo} = $tied{foo}> possible.
     * However, it seems no longer to be needed for that purpose, and
     * introduced a new bug: stuff like C<while ($hash{taintedval} =~ /.../g>
     * would loop endlessly since the pos magic is getting set on the
     * mortal copy and lost. However, the copy has the effect of
     * triggering the get magic, and losing it altogether made things like
     * c<$tied{foo};> in void context no longer do get magic, which some
     * code relied on. Also, delayed triggering of magic on @+ and friends
     * meant the original regex may be out of scope by now. So as a
     * compromise, do the get magic here. (The MGf_GSKIP flag will stop it
     * being called too many times). */
    if (!lval && SvRMAGICAL(hv) && SvGMAGICAL(sv))
        mg_get(sv);
    PUSHs(sv);
    RETURN;
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
