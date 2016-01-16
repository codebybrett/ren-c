/***********************************************************************
**
**  REBOL [R3] Language Interpreter and Run-time Environment
**
**  Copyright 2012 REBOL Technologies
**  REBOL is a trademark of REBOL Technologies
**
**  Licensed under the Apache License, Version 2.0 (the "License");
**  you may not use this file except in compliance with the License.
**  You may obtain a copy of the License at
**
**  http://www.apache.org/licenses/LICENSE-2.0
**
**  Unless required by applicable law or agreed to in writing, software
**  distributed under the License is distributed on an "AS IS" BASIS,
**  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
**  See the License for the specific language governing permissions and
**  limitations under the License.
**
************************************************************************
**
**  Module:  c-function.c
**  Summary: support for functions, actions, closures and routines
**  Section: core
**  Author:  Carl Sassenrath, Shixin Zeng
**  Notes:
**
***********************************************************************/
/*
    Structure of functions:

        spec - interface spec block
        body - body code
        args - args list (see below)

    Args list is a block of word+values:

        word - word, 'word, :word, /word
        value - typeset! or none (valid datatypes)

    Args list provides:

        1. specifies arg order, arg kind (e.g. 'word)
        2. specifies valid datatypes (typesets)
        3. used for word and type in error output
        4. used for debugging tools (stack dumps)
        5. not used for MOLD (spec is used)
        6. used as a (pseudo) frame of function variables

*/

#include "sys-core.h"

//
//  List_Func_Words: C
// 
// Return a block of function words, unbound.
// Note: skips 0th entry.
//
REBARR *List_Func_Words(const REBVAL *func)
{
    REBARR *array = Make_Array(VAL_FUNC_NUM_PARAMS(func));
    REBVAL *typeset = VAL_FUNC_PARAMS_HEAD(func);

    for (; !IS_END(typeset); typeset++) {
        enum Reb_Kind kind;

        if (VAL_GET_EXT(typeset, EXT_TYPESET_HIDDEN)) {
            // "true local" (e.g. it was a SET-WORD! in the spec)
            // treat as invisible and do not expose via WORDS-OF
            continue;
        }

        if (VAL_GET_EXT(typeset, EXT_TYPESET_REFINEMENT))
            kind = REB_REFINEMENT;
        else if (VAL_GET_EXT(typeset, EXT_TYPESET_QUOTE)) {
            if (VAL_GET_EXT(typeset, EXT_TYPESET_EVALUATE))
                kind = REB_LIT_WORD;
            else
                kind = REB_GET_WORD;
        }
        else {
            // Currently there's no meaning for non-quoted non-evaluating
            // things (only 3 param types for foo:, 'foo, :foo)
            assert(VAL_GET_EXT(typeset, EXT_TYPESET_EVALUATE));
            kind = REB_WORD;
        }

        Val_Init_Word(Alloc_Tail_Array(array), kind, VAL_TYPESET_SYM(typeset));
    }

    return array;
}


//
//  List_Func_Typesets: C
// 
// Return a block of function arg typesets.
// Note: skips 0th entry.
//
REBARR *List_Func_Typesets(REBVAL *func)
{
    REBARR *array = Make_Array(VAL_FUNC_NUM_PARAMS(func));
    REBVAL *typeset = VAL_FUNC_PARAMS_HEAD(func);

    for (; !IS_END(typeset); typeset++) {
        REBVAL *value = Alloc_Tail_Array(array);
        *value = *typeset;

        // !!! It's already a typeset, but this will clear out the header
        // bits.  This may not be desirable over the long run (what if
        // a typeset wishes to encode hiddenness, protectedness, etc?)

        VAL_RESET_HEADER(value, REB_TYPESET);
    }

    return array;
}


//
//  Make_Paramlist_Managed: C
// 
// Check function spec of the form:
// 
// ["description" arg "notes" [type! type2! ...] /ref ...]
// 
// Throw an error for invalid values.
//
REBARR *Make_Paramlist_Managed(REBARR *spec, REBCNT opt_sym_last)
{
    REBVAL *item;
    REBARR *paramlist;
    REBVAL *typeset;

    // Use a temporary to hold a value being "bubbled" toward the end if there
    // was a request for a canon symbol to be moved to the end.  (Feature used
    // by definitional return.)
    //
    // !!! This could be done more efficiently as a feature of Collect_Keylist
    // when it was forming the array, but that efficiency would be at the cost
    // of burdening Collect_Keylist's interface and adding overhead for more
    // common binding operations than function spec analysis.
    //
    REBVAL bubble;
    VAL_INIT_WRITABLE_DEBUG(&bubble);
    SET_END(&bubble); // not holding a value being bubbled to end...

    // Start by reusing the code that makes keylists out of Rebol-structured
    // data.  Scan for words (BIND_ALL) and error on duplicates (BIND_NO_DUP)
    //
    paramlist = Collect_Keylist_Managed(
        NULL, ARRAY_HEAD(spec), NULL, BIND_ALL | BIND_NO_DUP
    );
    ARRAY_SET_FLAG(paramlist, OPT_SER_PARAMLIST);

    // Whatever function is being made, it must fill in the paramlist slot 0
    // with an ANY-FUNCTION! value corresponding to the function that it is
    // the paramlist of.  Use SET_TRASH so that the debug build will leave
    // an alarm if that value isn't thrown in (the GC would complain...)

    typeset = ARRAY_HEAD(paramlist);
    SET_TRASH_IF_DEBUG(typeset);

    // !!! needs more checks
    for (item = ARRAY_HEAD(spec); NOT_END(item); item++) {

        if (ANY_BINSTR(item)) {
            // A goal of the Ren-C design is that core generators like
            // MAKE FUNCTION! an MAKE OBJECT! do not know any keywords or
            // key strings.  As a consequence, the most flexible offering
            // to function generators is to allow them to let as many
            // strings or tags or otherwise be stored in the spec as
            // they might wish to.  It's up to them to take them out.
            //
            // So it's not Check_Func_Spec's job to filter out "bad" string
            // patterns.  Anything is fair game:
            //
            //      [foo [type!] {doc string :-)}]
            //      [foo {doc string :-/} [type!]]
            //      [foo {doc string1 :-/} {doc string2 :-(} [type!]]
            //
            // HELP and other clients of SPEC-OF are left with the burden of
            // sorting out the variants.  The current policy of HELP is only
            // to show strings.
            //
            // !!! Though the system isn't supposed to have a reaction to
            // strings, is there a meaning for BINARY! besides ignoring it?

            continue;
        }

        if (IS_BLOCK(item)) {
            REBVAL *attribute;

            if (typeset != ARRAY_HEAD(paramlist)) {
                //
                // Turn block into typeset for parameter at current index
                // Note: Make_Typeset leaves VAL_TYPESET_SYM as-is
                //
                Make_Typeset(VAL_ARRAY_HEAD(item), typeset, FALSE);
                continue;
            }

            // !!! Rebol2 had the ability to put a block in the first
            // slot before any parameters, in which you could put words.
            // This is deprecated in favor of the use of tags.  We permit
            // [catch] and [throw] during Rebol2 => Rebol3 migration.
            //
            // !!! Longer-term this will likely be where a typeset goes that
            // indicates the return type of the function.  The tricky part
            // of that is there's nowhere to put that typeset.  Adding it
            // as a key to the frame would add an extra VAR to the frame
            // also...which would be a possibility, perhaps with a special
            // symbol ID.  The storage space for the VAR might not need
            // to be wasted; there may be another use for a value-sized
            // spot per-invocation.
            //
            attribute = VAL_ARRAY_AT(item);
            for (; NOT_END(attribute); attribute++) {
                if (IS_WORD(attribute)) {
                    if (VAL_WORD_SYM(attribute) == SYM_CATCH)
                        continue; // ignore it;
                    if (VAL_WORD_SYM(attribute) == SYM_THROW) {
                        // !!! Basically a synonym for <transparent>, but
                        // transparent is now a manipulation done by the
                        // function generators *before* the internal spec
                        // is checked...and the flag is removed.  So
                        // simulating it here is no longer easy...hence
                        // ignore it;
                        continue;
                    }
                    // no other words supported, fall through to error
                }
                fail (Error(RE_BAD_FUNC_DEF, item));
            }
            continue;
        }

        if (!ANY_WORD(item))
            fail (Error(RE_BAD_FUNC_DEF, item));

        typeset++;

        assert(
            IS_TYPESET(typeset)
            && VAL_TYPESET_SYM(typeset) == VAL_WORD_SYM(item)
        );

        switch (VAL_TYPE(item)) {
        case REB_WORD:
            VAL_SET_EXT(typeset, EXT_TYPESET_EVALUATE);
            break;

        case REB_GET_WORD:
            VAL_SET_EXT(typeset, EXT_TYPESET_QUOTE);
            break;

        case REB_LIT_WORD:
            VAL_SET_EXT(typeset, EXT_TYPESET_QUOTE);
            // will actually only evaluate get-word!, get-path!, and group!
            VAL_SET_EXT(typeset, EXT_TYPESET_EVALUATE);
            break;

        case REB_REFINEMENT:
            VAL_SET_EXT(typeset, EXT_TYPESET_REFINEMENT);

            // Refinements can nominally be only WORD! or NONE!
            VAL_TYPESET_BITS(typeset) =
                (FLAGIT_64(REB_WORD) | FLAGIT_64(REB_NONE));
            break;

        case REB_SET_WORD:
            // "Pure locals"... these will not be visible via WORDS-OF and
            // will be skipped during argument fulfillment.  We re-use the
            // same option flag that is used to hide words other places.

            VAL_SET_EXT(typeset, EXT_TYPESET_HIDDEN);
            break;

        default:
            fail (Error(RE_BAD_FUNC_DEF, item));
        }

        if (VAL_TYPESET_CANON(typeset) == opt_sym_last) {
            //
            // If we find the canon symbol we were looking for then grab it
            // into the bubble.
            //
            assert(opt_sym_last != SYM_0 && IS_END(&bubble));
            bubble = *typeset;
        }
        else if (NOT_END(&bubble)) {
            //
            // If we already found our bubble, keep moving the typeset bits
            // back one slot to cover up each hole left.
            //
            *(typeset - 1) = *typeset;
        }
    }

    // Note the above code leaves us in the final typeset position... the loop
    // is incrementing the *spec* and bumps the typeset on demand.
    //
    assert(IS_END(typeset + 1));

    // If we were looking for something to bubble to the end, assert we've
    // found it...and place it in that final slot.  (It may have come from
    // the last slot so it's a No-Op, but no reason to check that.)
    //
    if (opt_sym_last != SYM_0) {
        assert(NOT_END(&bubble));
        *typeset = bubble;
    }

    // Make sure the parameter list does not expand.
    //
    // !!! Should more precautions be taken, at some point locking and
    // protecting the whole array?  (It will be changed more by the caller,
    // but after that.)
    //
    ARRAY_SET_FLAG(paramlist, OPT_SER_FIXED_SIZE);

    return paramlist;
}


//
//  Make_Native: C
//
void Make_Native(
    REBVAL *out,
    REBARR *spec,
    REBNAT code,
    enum Reb_Kind type,
    REBOOL frameless
) {
    //Print("Make_Native: %s spec %d", Get_Sym_Name(type+1), SERIES_LEN(spec));

    ENSURE_ARRAY_MANAGED(spec);

    VAL_RESET_HEADER(out, type);
    if (frameless)
        VAL_SET_EXT(out, EXT_FUNC_FRAMELESS);

    VAL_FUNC_CODE(out) = code;
    VAL_FUNC_SPEC(out) = spec;

    out->payload.any_function.func
        = AS_FUNC(Make_Paramlist_Managed(spec, SYM_0));

    // Save the function value in slot 0 of the paramlist so that having
    // just the paramlist REBARR can get you the full REBVAL of the function
    // that it is the paramlist for.

    *FUNC_VALUE(out->payload.any_function.func) = *out;

    // Make sure all the vars are marked read only.  This means that any
    // vars which get bound to the native's args will not be able to modify
    // them.  Such references are being experimentally allowed in the
    // debugger.
    //
    // !!! Review whether allowing such references is a good or bad idea.
    // Note also that this protection can be undone in user mode, which
    // suggests the need for another bit that PROTECT checks.
    //
    {
        REBVAL *param;
        param = VAL_FUNC_PARAMS_HEAD(out);
        for (; NOT_END(param); param++) {
            assert(IS_TYPESET(param));
            VAL_SET_EXT(param, EXT_TYPESET_LOCKED);
        }
    }

    // These native routines want to be recognized by paramlist, not by their
    // VAL_FUNC_CODE pointers.  (RETURN because the code pointer is swapped
    // out for VAL_FUNC_RETURN_FROM, and EVAL for 1 test vs. 2 in the eval loop.)
    //
    // PARSE wants to throw its value from nested code to itself, and doesn't
    // want to thread its known D_FUNC value through the call stack.
    //
    if (code == &N_return) {
        *ROOT_RETURN_NATIVE = *out;

        // Curiously, it turns out that extracting the paramlist to a global
        // once and comparing against it is about 30% faster than saving to the
        // root object and extracting VAL_FUNC_PARAMLIST(ROOT_RETURN_NATIVE)
        // each time...
        //
        PG_Return_Func = VAL_FUNC(out);

        // The definitional return code canonizes symbols to see if they are
        // return or not, but doesn't canonize SYM_RETURN.  Double-check it
        // does not have to.
        //
        // !!! Is there a better point in the bootstrap for this check, where
        // it's late enough to not fail the word table lookup?
        //
        assert(SYM_RETURN == SYMBOL_TO_CANON(SYM_RETURN));
    }
    else if (code == &N_parse)
        *ROOT_PARSE_NATIVE = *out;
    else if (code == &N_eval) {
        //
        // See above note regarding return.  A check for EVAL is done on each
        // function evaluation, so it's worth it to extract.
        //
        PG_Eval_Func = VAL_FUNC(out);
    }
    else if (code == &N_resume) {
        *ROOT_RESUME_NATIVE = *out;
    }
    else if (code == &N_quit) {
        *ROOT_QUIT_NATIVE = *out;
    }
}


//
//  Get_Maybe_Fake_Func_Body: C
// 
// The EXT_FUNC_HAS_RETURN tricks used for definitional scoping acceleration
// make it seem like a generator authored more code in the function's
// body...but the code isn't *actually* there and an optimized internal
// trick is used.
// 
// If the body is fake, it needs to be freed by the caller with
// Free_Series.  This means that the body must currently be shallow
// copied, and the splicing slot must be in the topmost series.
//
REBARR *Get_Maybe_Fake_Func_Body(REBOOL *is_fake, const REBVAL *func)
{
    REBARR *fake_body;

    assert(IS_CLOSURE(func) || IS_FUNCTION(func));

    if (!VAL_GET_EXT(func, EXT_FUNC_HAS_RETURN)) {
        *is_fake = FALSE;
        return VAL_FUNC_BODY(func);
    }

    *is_fake = TRUE;

    // See comments in sysobj.r on standard/func-body.
    fake_body = Copy_Array_Shallow(
        VAL_ARRAY(Get_System(SYS_STANDARD, STD_FUNC_BODY))
    );

    // Index 5 (or 4 in zero-based C) should be #BODY, a "real" body
    assert(IS_ISSUE(ARRAY_AT(fake_body, 4))); // #BODY
    Val_Init_Array(ARRAY_AT(fake_body, 4), REB_GROUP, VAL_FUNC_BODY(func));
    VAL_SET_OPT(ARRAY_AT(fake_body, 4), OPT_VALUE_LINE);

    // !!! This should not be necessary as there is a line break in the
    // template...look into why the line didn't make it to the body.
    //
    VAL_SET_OPT(ARRAY_AT(fake_body, 0), OPT_VALUE_LINE);

    return fake_body;
}


//
//  Make_Function: C
// 
// This is the support routine behind `MAKE FUNCTION!` (or CLOSURE!), the
// basic building block of creating functions in Rebol.
// 
// If `has_return` is passed in as TRUE, then is also the optimized native
// implementation for the function generators FUNC and CLOS.  Ren/C's
// schematic for these generators is *very* different from R3-Alpha, whose
// definition of FUNC was simply:
// 
//     make function! copy/deep reduce [spec body]
// 
// Not only does Ren/C's `make function!` already copy the spec and body,
// but FUNC and CLOS "use the internals to cheat".  They analyze and edit
// the spec, then potentially build an entity whose full "body" acts like:
// 
//     return: make function! [
//         [{Returns a value from a function.} value [opt-any-value!]]
//         [throw/name :value bind-of 'return]
//     ]
//     catch/name (body) bind-of 'return
// 
// This pattern addresses "Definitional Return" in a way that does not
// technically require building RETURN in as a language keyword in any
// specific form.  FUNC and CLOS optimize by not internally building
// or executing the equivalent body, but giving it back from BODY-OF.
// 
// NOTES:
// 
// The spec and body are copied--even for MAKE FUNCTION!--because:
// 
//    (a) It prevents tampering with the spec after it has been analyzed
//        by Make_Paramlist_Managed().  Such changes to the spec will not be
//        reflected in the actual behavior of the function.
// 
//    (b) The BLOCK! values inside the make-spec may actually be imaging
//        series at an index position besides the series head.  However,
//        the REBVAL for a FUNCTION! contains only three REBSER slots--
//        all in use, with no space for offsets.  A copy must be made
//        to truncate to the intended spec and body start (unless one
//        is willing to raise errors on non-head position series :-/)
// 
//    (c) Copying the root of the series into a series the user cannot
//        access makes it possible to "lie" about what the body "above"
//        is.  This gives FUNC and CLOS the edge to pretend to add
//        containing code and simulate its effects, while really only
//        holding onto the body the caller provided.  This trick may
//        prove useful for other optimizing generators.
// 
// While MAKE FUNCTION! has no RETURN, all functions still have EXIT as a
// non-definitional alternative.  Ren/C adds a /WITH refinement so it can
// behave equivalently to old-non-definitonal return.  There is even a way to
// identify specific points up the call stack to exit from via EXIT/FROM, so
// not having definitional return has several alternate options for generators
// that wish to use them.
// 
// This function will either successfully place a function value into
// `out` or not return...as a failed check on a function spec is
// raised as an error.
//
void Make_Function(
    REBVAL *out,
    enum Reb_Kind type,
    const REBVAL *spec,
    const REBVAL *body,
    REBOOL has_return
) {
    REBYTE func_flags = 0; // 8-bits in header, reserved type-specific flags

    if (!IS_BLOCK(spec) || !IS_BLOCK(body))
        fail (Error_Bad_Func_Def(spec, body));

    if (!has_return) {
        //
        // Simpler case: if `make function!` or `make closure!` are used
        // then the function is "effectively <transparent>".   There is no
        // definitional return automatically added.  Non-definitional EXIT
        // and EXIT/WITH will still be available.
        //
        // A small optimization will reuse the global empty array for an
        // empty spec instead of copying (as the spec need not be unique)
        //
        if (VAL_LEN_AT(spec) == 0)
            VAL_FUNC_SPEC(out) = EMPTY_ARRAY;
        else
            VAL_FUNC_SPEC(out) = Copy_Array_At_Deep_Managed(
                VAL_ARRAY(spec), VAL_INDEX(spec)
            );
    }
    else {
        // Trickier case: when the `func` or `clos` natives are used, they
        // must read the given spec the way a user-space generator might.
        // They must decide whether to add a specially handled RETURN
        // local, which will be given a tricky "native" definitional return

        REBVAL *item = VAL_ARRAY_HEAD(spec);
        REBCNT index = 0;
        REBOOL convert_local = FALSE;

        for (; NOT_END(item); index++, item++) {
            if (IS_SET_WORD(item)) {
                //
                // Note a "true local" (indicated by a set-word) is considered
                // to be tacit approval of wanting a definitional return
                // by the generator.  This helps because Red's model
                // for specifying returns uses a SET-WORD!
                //
                //     func [return: [integer!] {returns an integer}]
                //
                // In Ren/C's case it just means you want a local called
                // return, but the generator will be "initializing it
                // with a definitional return" for you.  You don't have
                // to use it if you don't want to...

                // !!! Should FUNC and CLOS be willing to move blocks after
                // a return: to the head to indicate a type check?  It
                // breaks the purity of the model.

                continue;
            }

            if (IS_TAG(item)) {
                if (
                    0 == Compare_String_Vals(item, ROOT_TRANSPARENT_TAG, TRUE)
                ) {
                    // The <transparent> tag is a way to cue FUNC and CLOS that
                    // you do not want a definitional return:
                    //
                    //     foo: func [<transparent> a] [return a]
                    //     foo 10 ;-- ERROR!
                    //
                    // This is redundant with the default for `make function!`.
                    // But having an option to use the familiar arity-2 form
                    // will probably appeal to more users.  Also, having two
                    // independent parameters can save the need for a REDUCE
                    // or COMPOSE that is generally required to composite a
                    // single block parameter that MAKE FUNCTION! requires.
                    //
                    VAL_FUNC_SPEC(out) = Copy_Array_At_Deep_Managed(
                        VAL_ARRAY(spec), VAL_INDEX(spec)
                    );
                    has_return = FALSE;

                    // We *could* remove the <transparent> tag, or check to
                    // see if there's more than one, etc.  But Check_Func_Spec
                    // is tolerant of any strings that we leave in the spec.
                    // This tolerance exists because the system is not to have
                    // any features based on recognizing specific keywords,
                    // so there's no need for tags to be "for future expansion"
                    // ... hence the mechanical cost burden of being forced
                    // to copy and remove them is a cost generators may not
                    // want to pay.

                    /*Remove_Series(VAL_FUNC_SPEC(out), index, 1);*/
                }
                else if (
                    0 == Compare_String_Vals(item, ROOT_INFIX_TAG, TRUE)
                ) {
                    // The <infix> option may or may not stick around.  The
                    // main reason not to is that it doesn't make sense for
                    // OP! to be the same interface type as FUNCTION! (or
                    // ANY-FUNCTION!).  An INFIX function generator is thus
                    // kind of tempting that returns an INFIX! (OP!), so
                    // this will remain under consideration.
                    //
                    SET_FLAG(func_flags, EXT_FUNC_INFIX);
                }
                else if (
                    0 == Compare_String_Vals(item, ROOT_LOCAL_TAG, TRUE)
                ) {
                    // While using x: and y: for pure locals is one option,
                    // it has two downsides.  One downside is that it makes
                    // the spec look too much "like everything else", so
                    // all the code kind of bleeds together.  Another is that
                    // if you nest one function within another then the outer
                    // function will wind up locals-gathering the locals of
                    // the inner function.  (It will anyway if you put the
                    // whole literal body there, but if you're adding the
                    // locals in a generator to be picked up by code that
                    // rebinds to them then it makes a difference.)
                    //
                    // Having a tag that lets you mark a run of locals is
                    // useful.  It will convert WORD! to SET-WORD! in the
                    // spec, and stop at the next refinement.
                    //
                    convert_local = TRUE;

                    // See notes about how we *could* remove ANY-STRING!s like
                    // the <local> tag from the spec, but Check_Func_Spec
                    // doesn't mind...it might be useful for HELP...and it's
                    // cheaper not to.
                }
                else
                    fail (Error(RE_BAD_FUNC_DEF, item));
            }
            else if (ANY_WORD(item)) {
                if (convert_local) {
                    if (IS_WORD(item)) {
                        //
                        // We convert words to set-words for pure local status
                        //
                        VAL_SET_TYPE_BITS(item, REB_SET_WORD);
                    }
                    else if (IS_REFINEMENT(item)) {
                        //
                        // A refinement signals us to stop doing the locals
                        // conversion.  Historically, help hides any
                        // refinements that appear behind a /local, so
                        // presumably it would do the same with <local>...
                        // but mechanically there is no way to tell
                        // Check_Func_Spec to hide a refinement.
                        //
                        convert_local = FALSE;
                    }
                    else {
                        // We've already ruled out pure locals, so this means
                        // they wrote something like:
                        //
                        //     func [a b <local> 'c #d :e]
                        //
                        // Consider that an error.
                        //
                        fail (Error(RE_BAD_FUNC_DEF, item));
                    }
                }

                if (SAME_SYM(VAL_WORD_SYM(item), SYM_RETURN)) {
                    //
                    // Although return: is explicitly tolerated,  all these
                    // would cancel a definitional return:
                    //
                    //     func [return [integer!]]
                    //     func [/value return]
                    //     func [/local return]
                    //
                    // The last one because /local is actually "just an ordinary
                    // refinement".  The choice of HELP to omit it could be
                    // a configuration setting.
                    //
                    VAL_FUNC_SPEC(out) = Copy_Array_At_Deep_Managed(
                        VAL_ARRAY(spec), VAL_INDEX(spec)
                    );
                    has_return = FALSE;
                }
            }
        }

        if (has_return) {
            //
            // No prior RETURN (or other issue) stopping definitional return!
            // Add the "true local" RETURN: to the spec.
            //
            if (index == 0) {
                //
                // If the incoming spec was [] and we are turning it to
                // [return:], then that's a relatively common pattern
                // (e.g. what DOES would manufacture).  Re-use a global
                // instance of that series as an optimization.
                //
                VAL_FUNC_SPEC(out) = VAL_ARRAY(ROOT_RETURN_BLOCK);
            }
            else {
                VAL_FUNC_SPEC(out) = Copy_Array_At_Extra_Deep_Managed(
                    VAL_ARRAY(spec), VAL_INDEX(spec), 1 // +1 capacity hint
                );
                Append_Value(VAL_FUNC_SPEC(out), ROOT_RETURN_SET_WORD);
            }
        }
    }

    // Spec checking will longjmp out with an error if the spec is bad.
    // For efficiency, we tell the paramlist what symbol we would like to
    // have located in the final slot if its symbol is found (so SYM_RETURN
    // if the function has a optimized definitional return).
    //
    out->payload.any_function.func = AS_FUNC(
        Make_Paramlist_Managed(
            VAL_FUNC_SPEC(out),
            has_return ? SYM_RETURN : SYM_0
        )
    );

    // We copy the body or do the empty body optimization to not copy and
    // use the EMPTY_ARRAY (which probably doesn't happen often...)
    //
    if (VAL_LEN_AT(body) == 0)
        VAL_FUNC_BODY(out) = EMPTY_ARRAY;
    else
        VAL_FUNC_BODY(out) = Copy_Array_At_Deep_Managed(
            VAL_ARRAY(body), VAL_INDEX(body)
        );

    // Even if `has_return` was passed in true, the FUNC or CLOS generator
    // may have seen something to turn it off and turned it false.  But if
    // it's still on, then signal we want the fancy fake return!
    //
    if (has_return) {
        //
        // Make_Paramlist above should have ensured it's in the last slot.
        //
    #if !defined(NDEBUG)
        REBVAL *param = ARRAY_LAST(AS_ARRAY(out->payload.any_function.func));
        assert(VAL_TYPESET_CANON(param) == SYM_RETURN);
        assert(VAL_GET_EXT(param, EXT_TYPESET_HIDDEN));
    #endif

        // Flag that this function has a definitional return, so Dispatch_Call
        // knows to write the "hacked" function in that final local.  (Arg
        // fulfillment should leave the hidden parameter unset)
        //
        SET_FLAG(func_flags, EXT_FUNC_HAS_RETURN);
    }

#if !defined(NDEBUG)
    //
    // Because Mezzanine functions are written to depend on the idea that when
    // they get a refinement it will be a WORD! and not a LOGIC!, we have to
    // capture the desire to get LOGIC! vs WORD! at function creation time,
    // not dispatch time.
    //
    if (LEGACY(OPTIONS_REFINEMENTS_TRUE))
        SET_FLAG(func_flags, EXT_FUNC_LEGACY);
#endif

    assert(type == REB_FUNCTION || type == REB_CLOSURE);
    VAL_RESET_HEADER(out, type); // clears value opts and exts in header...
    VAL_SET_EXTS_DATA(out, func_flags); // ...so we set this after that point

    // Now that we've created the function's fields, we pull a trick.  It
    // would be useful to be able to navigate to a full function value
    // given just its identifying series, but where to put it?  We use
    // slot 0 (a trick learned from R3-Alpha's object strategy)
    //
    *FUNC_VALUE(out->payload.any_function.func) = *out;

    // The argument and local symbols have been arranged in the function's
    // "frame" and are now in index order.  These numbers are put
    // into the binding as *negative* versions of the index, in order
    // to indicate that they are in a function and not an object frame.
    //
    // (This is done for the closure body even though each call is associated
    // with an object frame.  The reason is that this is only the "archetype"
    // body of the closure...it is copied each time and the real numbers
    // filled in.  Having the indexes already done speeds the copying.)
    //
    Bind_Relative_Deep(VAL_FUNC(out), VAL_FUNC_BODY(out));
}


//
//  Clonify_Function: C
// 
// The "Clonify" interface takes in a raw duplicate value that one
// wishes to mutate in-place into a full-fledged copy of the value
// it is a clone of.  This interface can be more efficient than a
// "source in, dest out" copy...and clarifies the dangers when the
// source and destination are the same.
//
void Clonify_Function(REBVAL *value)
{
    REBFUN *func_orig;
    REBARR *paramlist_copy;

    // !!! Conceptually the only types it currently makes sense to speak of
    // copying are functions and closures.  Though the concept is a little
    // bit "fuzzy"...the idea is that the series which are reachable from
    // their body series by a deep copy would be their "state".  Hence
    // as a function runs, its "state" can change.  One can thus define
    // a copy as snapshotting that "state".  This has been the classic
    // interpretation that Rebol has taken.

    // !!! However, in R3-Alpha a closure's "archetype" (e.g. the one made
    // by `clos [a] [print a]`) never operates on its body directly... it
    // is copied each time.  And there is no way at present to get a
    // reference to a closure "instance" (an ANY-FUNCTION value with the
    // copied body in it).  Until such time as there's a way

    // !!! This leaves only one function type that is mechanically
    // clonable at all... the FUNCTION!.  While the behavior is questionable,
    // for now we will suspend disbelief and preserve what R3-Alpha did
    // until a clear resolution.

    if (!IS_FUNCTION(value))
        return;

    // No need to modify the spec or header.  But we do need to copy the
    // identifying parameter series, so that the copied function has a
    // unique identity on the stack from the one it is copying.  Otherwise
    // two calls on the stack would be seen as recursions of the same
    // function, sharing each others "stack relative locals".

    func_orig = VAL_FUNC(value);
    paramlist_copy = Copy_Array_Shallow(FUNC_PARAMLIST(func_orig));

    ARRAY_SET_FLAG(paramlist_copy, OPT_SER_PARAMLIST);

    value->payload.any_function.func = AS_FUNC(paramlist_copy);

    VAL_FUNC_BODY(value) = Copy_Array_Deep_Managed(VAL_FUNC_BODY(value));

    // Remap references in the body from paramlist_orig to our new copied
    // word list we saved in VAL_FUNC_PARAMLIST(value)

    Rebind_Values_Relative_Deep(
        func_orig,
        value->payload.any_function.func,
        ARRAY_HEAD(VAL_FUNC_BODY(value))
    );

    // The above phrasing came from deep cloning code, while the below was
    // in the Copy_Function code.  Evaluate if there is now "dead code"
    // relating to the difference.
/*
    Bind_Relative_Deep(
        VAL_FUNC_PARAMLIST(out), VAL_FUNC_PARAMLIST(out), VAL_FUNC_BODY(out)
    );
*/

    // The first element in the paramlist is the identity of the function
    // value itself.  So we must update this value if we make a copy,
    // so the paramlist does not indicate the original.
    //
    *FUNC_VALUE(value->payload.any_function.func) = *value;

    MANAGE_ARRAY(VAL_FUNC_PARAMLIST(value));
}


//
//  Do_Native_Core: C
//
enum Reb_Call_Mode Do_Native_Core(struct Reb_Call *call_)
{
    REB_R ret;

    Eval_Natives++;

    // For all other native function pointers (for now)...ordinary dispatch.

    ret = FUNC_CODE(D_FUNC)(call_);

    switch (ret) {
    case R_OUT: // for compiler opt
    case R_OUT_IS_THROWN:
        break;
    case R_NONE:
        SET_NONE(D_OUT);
        break;
    case R_UNSET:
        SET_UNSET(D_OUT);
        break;
    case R_TRUE:
        SET_TRUE(D_OUT);
        break;
    case R_FALSE:
        SET_FALSE(D_OUT);
        break;
    case R_ARG1:
        *D_OUT = *D_ARG(1);
        break;
    case R_ARG2:
        *D_OUT = *D_ARG(2);
        break;
    case R_ARG3:
        *D_OUT = *D_ARG(3);
        break;
    default:
        assert(FALSE);
    }

    return ret == R_OUT_IS_THROWN ? CALL_MODE_THROWN : CALL_MODE_0;
}


//
//  Do_Action_Core: C
//
enum Reb_Call_Mode Do_Action_Core(struct Reb_Call *call_)
{
    REBCNT type = VAL_TYPE(D_ARG(1));
    REBACT action;
    REB_R ret;

    Eval_Natives++;

    assert(type < REB_MAX);

    // Handle special datatype test cases (eg. integer?).  Note that this
    // has a frameless implementation which is the one that typically runs
    // when a frame is not required (such as when running under trace, where
    // the values need to be inspectable)
    //
    if (FUNC_ACT(D_FUNC) < REB_MAX) {
        if (type == FUNC_ACT(D_FUNC))
            SET_TRUE(D_OUT);
        else
            SET_FALSE(D_OUT);

        return CALL_MODE_0;
    }

    action = Value_Dispatch[type];
    if (!action) fail (Error_Illegal_Action(type, FUNC_ACT(D_FUNC)));
    ret = action(call_, FUNC_ACT(D_FUNC));

    switch (ret) {
    case R_OUT: // for compiler opt
    case R_OUT_IS_THROWN:
        break;
    case R_NONE:
        SET_NONE(D_OUT);
        break;
    case R_UNSET:
        SET_UNSET(D_OUT);
        break;
    case R_TRUE:
        SET_TRUE(D_OUT);
        break;
    case R_FALSE:
        SET_FALSE(D_OUT);
        break;
    case R_ARG1:
        *D_OUT = *D_ARG(1);
        break;
    case R_ARG2:
        *D_OUT = *D_ARG(2);
        break;
    case R_ARG3:
        *D_OUT = *D_ARG(3);
        break;
    default:
        assert(FALSE);
    }

    return ret == R_OUT_IS_THROWN ? CALL_MODE_THROWN : CALL_MODE_0;
}


//
//  Do_Function_Core: C
//
enum Reb_Call_Mode Do_Function_Core(struct Reb_Call *c)
{
    Eval_Functions++;

    // !!! repeated code in Do_Closure (should disappear in unification)
    //
    if (VAL_GET_EXT(FUNC_VALUE(c->func), EXT_FUNC_HAS_RETURN)) {
        //
        // If the closure has a native-optimized version of definitional
        // return, the local for this return should so far have just been
        // ensured in last slot...and left unset by any arg filling process.
        //
        REBVAL *last_arg = &c->arg[FUNC_NUM_PARAMS(c->func)];

    #if !defined(NDEBUG)
        REBVAL *last_param = FUNC_PARAM(c->func, FUNC_NUM_PARAMS(c->func));
        assert(VAL_TYPESET_CANON(last_param) == SYM_RETURN);
        assert(VAL_GET_EXT(last_param, EXT_TYPESET_HIDDEN));

        if (VAL_GET_EXT(FUNC_VALUE(c->func), EXT_FUNC_LEGACY))
            assert(IS_NONE(last_arg));
        else
            assert(IS_UNSET(last_arg));
    #endif

        // Now fill in the var for that local with a "hacked up" native
        // Note that FUNCTION! uses its PARAMLIST as the RETURN_FROM
        //
        *last_arg = *ROOT_RETURN_NATIVE;
        VAL_FUNC_RETURN_FROM(last_arg) = FUNC_PARAMLIST(c->func);
    }

    // Functions have a body series pointer, but no VAL_INDEX, so use 0
    //
    if (Do_At_Throws(c->out, FUNC_BODY(c->func), 0))
        return CALL_MODE_THROWN;

    return CALL_MODE_0;
}


//
//  Do_Closure_Core: C
// 
// Do a closure by cloning its body and rebinding it to
// a new frame of words/values.
//
enum Reb_Call_Mode Do_Closure_Core(struct Reb_Call *c)
{
    REBARR *body;
    REBCON *context;

    Eval_Functions++;

    // The head value of a function/closure paramlist should be the value
    // of the function/closure itself that has that paramlist.
    //
#if !defined(NDEBUG)
    assert(IS_CLOSURE(FUNC_VALUE(c->func)));
    if (VAL_FUNC_PARAMLIST(FUNC_VALUE(c->func)) != FUNC_PARAMLIST(c->func)) {
        Panic_Array(VAL_FUNC_PARAMLIST(FUNC_VALUE(c->func)));
    }
#endif

    // We will extract the arglist from ownership and manual memory management
    // by the call, to be used in a GC-managed object frame by the closure.
    // It will be held alive as long as the call is in effect by the
    // Reb_Call so that the `arg` pointer will remain valid.
    //
    context = AS_CONTEXT(c->arglist.array);

    // Formerly the arglist's 0 slot had a CLOSURE! value in it, but we now
    // are going to be switching it to an OBJECT!.

    ARRAY_SET_FLAG(CONTEXT_VARLIST(context), OPT_SER_CONTEXT);
    VAL_RESET_HEADER(CONTEXT_VALUE(context), REB_OBJECT);
    INIT_VAL_CONTEXT(CONTEXT_VALUE(context), context);
    INIT_CONTEXT_KEYLIST(context, FUNC_PARAMLIST(c->func));
    CONTEXT_SPEC(context) = NULL;
    CONTEXT_BODY(context) = NULL;
    ASSERT_CONTEXT(context);

    // We do not Manage_Context, because we are reusing a word series here
    // that has already been managed.  The arglist array was managed when
    // created and kept alive by Mark_Call_Frames
    //
    ASSERT_ARRAY_MANAGED(CONTEXT_KEYLIST(context));
    ASSERT_ARRAY_MANAGED(CONTEXT_VARLIST(context));

    // Clone the body of the closure to allow us to rebind words inside
    // of it so that they point specifically to the instances for this
    // invocation.  (Costly, but that is the mechanics of words.)
    //
    body = Copy_Array_Deep_Managed(FUNC_BODY(c->func));
    Rebind_Values_Closure_Deep(c->func, context, ARRAY_HEAD(body));

    // !!! repeated code in Do_Function (should disappear in unification)
    //
    if (VAL_GET_EXT(FUNC_VALUE(c->func), EXT_FUNC_HAS_RETURN)) {
        //
        // If the closure has a native-optimized version of definitional
        // return, the local for this return should so far have just been
        // ensured in last slot...and left unset by any arg filling process.
        //
        REBVAL *last_arg = CONTEXT_VAR(context, CONTEXT_LEN(context));

    #if !defined(NDEBUG)
        REBVAL *last_param = FUNC_PARAM(c->func, FUNC_NUM_PARAMS(c->func));
        assert(VAL_TYPESET_CANON(last_param) == SYM_RETURN);
        assert(VAL_GET_EXT(last_param, EXT_TYPESET_HIDDEN));
        assert(IS_UNSET(last_arg));
    #endif

        // Now fill in the var for that local with a "hacked up" native
        // Note that FUNCTION! uses its PARAMLIST as the RETURN_FROM
        //
        *last_arg = *ROOT_RETURN_NATIVE;
        VAL_FUNC_RETURN_FROM(last_arg) = CONTEXT_VARLIST(context);
    }

    // Protect the body from garbage collection during the course of the
    // execution.  (We could also protect it by stowing it in the call
    // frame's copy of the closure value, which we might think of as its
    // "archetype", but it may be valuable to keep that as-is.)
    //
    PUSH_GUARD_ARRAY(body);

    if (Do_At_Throws(c->out, body, 0)) {
        DROP_GUARD_ARRAY(body);
        return CALL_MODE_THROWN;
    }

    // References to parts of the closure's copied body may still be
    // extant, but we no longer need to hold this reference on it
    //
    DROP_GUARD_ARRAY(body);

    return CALL_MODE_0;
}


//
//  Do_Routine_Core: C
//
enum Reb_Call_Mode Do_Routine_Core(struct Reb_Call *call_)
{
    REBARR *args = Copy_Values_Len_Shallow(
        D_ARGC > 0 ? D_ARG(1) : NULL,
        D_ARGC
    );

    Call_Routine(D_FUNC, args, D_OUT);

    Free_Array(args);

    return CALL_MODE_0; // Cannot "throw" a Rebol value across an FFI boundary
}


//
//  func: native [
//  
//  "Defines a user function with given spec and body."
//  
//      spec [block!] 
//      {Help string (opt) followed by arg words (and opt type and string)}
//      body [block!] "The body block of the function"
//  ]
//
REBNATIVE(func)
//
// Native optimized implementation of a "definitional return" function
// generator.  FUNC uses "stack-relative binding" for optimization,
// which leads to less desirable behaviors than CLOS...while more
// performant.
// 
// See comments on Make_Function for full notes.
{
    PARAM(1, spec);
    PARAM(2, body);

    const REBOOL has_return = TRUE;

    Make_Function(D_OUT, REB_FUNCTION, ARG(spec), ARG(body), has_return);

    return R_OUT;
}


//
//  clos: native [
//  
//  "Defines a closure function."
//  
//      spec [block!] 
//      {Help string (opt) followed by arg words (and opt type and string)}
//      body [block!] "The body block of the function"
//  ]
//
REBNATIVE(clos)
//
// Native optimized implementation of a "definitional return" "closure"
// generator.  Each time a CLOS-created function is called, it makes
// a copy of its body and binds all the local words in that copied
// body into a uniquely persistable object.  This provides desirable
// behaviors of "leaked" bound variables surviving the end of the
// closure's call on the stack... as well as recursive instances
// being able to uniquely identify their bound variables from each
// other.  Yet this uses more memory and puts more strain on the
// garbage collector than FUNC.
// 
// A solution that can accomplish closure's user-facing effects with
// enough efficiency to justify replacing FUNC's implementation
// with it is sought, but no adequate tradeoff has been found.
// 
// See comments on Make_Function for full notes.
{
    PARAM(1, spec);
    PARAM(2, body);

    const REBOOL has_return = TRUE;

    Make_Function(D_OUT, REB_CLOSURE, ARG(spec), ARG(body), has_return);

    return R_OUT;
}


#if !defined(NDEBUG)

//
//  FUNC_PARAM_Debug: C
//
REBVAL *FUNC_PARAM_Debug(REBFUN *f, REBCNT n) {
    assert(n != 0 && n < ARRAY_LEN(FUNC_PARAMLIST(f)));
    return ARRAY_AT(FUNC_PARAMLIST(f), (n));
}


//
//  VAL_FUNC_Debug: C
//
REBFUN *VAL_FUNC_Debug(const REBVAL *v) {
    REBFUN *func = v->payload.any_function.func;
    struct Reb_Value_Header v_header = v->header;
    struct Reb_Value_Header func_header = FUNC_VALUE(func)->header;

    assert(func == FUNC_VALUE(func)->payload.any_function.func);
    assert(ARRAY_GET_FLAG(FUNC_PARAMLIST(func), OPT_SER_ARRAY));
    assert(ARRAY_GET_FLAG(v->payload.any_function.spec, OPT_SER_ARRAY));

    switch (VAL_TYPE(v)) {
    case REB_NATIVE:
        //
        // Only the definitional return is allowed to lie and put a differing
        // field in besides
        //
        if (func != PG_Return_Func) {
            assert(
                v->payload.any_function.impl.code == FUNC_CODE(func)
            );
        }
        else {
            // !!! There's ROOT_RETURN_NATIVE and also the native in the
            // system context which have the real code in them.  If those
            // are accounted for then it might be possible to assert that
            // any returns we see are definitional...but until then we
            // don't know if it has a valid code field or not.
            //
            /*assert(
                ARRAY_GET_FLAG(v->payload.any_function.impl.body, OPT_SER_ARRAY)
            );*/
        }
        break;

    case REB_ACTION:
        assert(
            v->payload.any_function.impl.act == FUNC_ACT(func)
        );
        break;

    case REB_COMMAND:
    case REB_FUNCTION:
    case REB_CLOSURE:
        assert(
            v->payload.any_function.impl.body == FUNC_BODY(func)
        );
        break;

    case REB_CALLBACK:
    case REB_ROUTINE:
        assert(
            v->payload.any_function.impl.info == FUNC_INFO(func)
        );
        break;

    default:
        assert(FALSE);
        break;
    }

    // set OPT_VALUE_LINE on both headers for sake of comparison, we allow
    // it to be different from the value stored in frame.
    //
    // !!! Should formatting flags be moved into their own section, perhaps
    // the section currently known as "resv: reserved for future use"?
    //
    // We also set OPT_VALUE_THROWN as that is not required to be sync'd
    // with the persistent value in the function.  This bit is deprecated
    // however, for many of the same reasons it's a nuisance here.  The
    // OPT_VALUE_EXIT_FROM needs to be handled in the same way.
    //
    v_header.all |= (
        (1 << OPT_VALUE_EXIT_FROM)
        | (1 << OPT_VALUE_LINE)
        | (1 << OPT_VALUE_THROWN)
    ) << 8;
    func_header.all |= (
        (1 << OPT_VALUE_EXIT_FROM)
        | (1 << OPT_VALUE_LINE)
        | (1 << OPT_VALUE_THROWN)
    ) << 8;

    if (v_header.all != func_header.all) {
        REBVAL *func_value = FUNC_VALUE(func);
        REBOOL frameless_value = VAL_GET_EXT(v, EXT_FUNC_FRAMELESS);
        REBOOL frameless_func = VAL_GET_EXT(func_value, EXT_FUNC_FRAMELESS);
        REBOOL has_return_value = VAL_GET_EXT(v, EXT_FUNC_HAS_RETURN);
        REBOOL has_return_func = VAL_GET_EXT(func_value, EXT_FUNC_HAS_RETURN);
        REBOOL infix_value = VAL_GET_EXT(v, EXT_FUNC_INFIX);
        REBOOL infix_func = VAL_GET_EXT(func_value, EXT_FUNC_INFIX);

        Debug_Fmt("Mismatch header bits found in FUNC_VALUE from payload");
        Debug_Array(v->payload.any_function.spec);
        Panic_Array(FUNC_PARAMLIST(func));
    }

    return func;
}

#endif
