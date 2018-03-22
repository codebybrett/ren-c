; Rebol []
; *****************************************************************************
; Title: Rebol core tests
; Copyright:
;     2012 REBOL Technologies
;     2013 Saphirion AG
; Author:
;     Carl Sassenrath, Ladislav Mecir, Andreas Bolka, Brian Hawley, John K
; License:
;     Licensed under the Apache License, Version 2.0 (the "License");
;     you may not use this file except in compliance with the License.
;     You may obtain a copy of the License at
;
;     http://www.apache.org/licenses/LICENSE-2.0
;
;     Unless required by applicable law or agreed to in writing, software
;     distributed under the License is distributed on an "AS IS" BASIS,
;     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
;     See the License for the specific language governing permissions and
;     limitations under the License.
; *****************************************************************************
%parse-tests.r

%datatypes/action.test.reb
%datatypes/binary.test.reb
%datatypes/bitset.test.reb
%datatypes/block.test.reb
%datatypes/char.test.reb

; CLOSURE is not supported for the moment
; https://forum.rebol.info/t/234
;%datatypes/closure.test.reb

%datatypes/datatype.test.reb
%datatypes/date.test.reb
%datatypes/decimal.test.reb
%datatypes/email.test.reb
%datatypes/error.test.reb
%datatypes/event.test.reb
%datatypes/file.test.reb
%datatypes/function.test.reb
%datatypes/get-path.test.reb
%datatypes/get-word.test.reb
%datatypes/gob.test.reb
%datatypes/hash.test.reb
%datatypes/image.test.reb
%datatypes/integer.test.reb
%datatypes/issue.test.reb
%datatypes/list.test.reb
%datatypes/lit-path.test.reb
%datatypes/lit-word.test.reb
%datatypes/logic.test.reb
%datatypes/map.test.reb
%datatypes/module.test.reb
%datatypes/money.test.reb
%datatypes/native.test.reb
%datatypes/none.test.reb
%datatypes/object.test.reb
%datatypes/op.test.reb
%datatypes/pair.test.reb
%datatypes/paren.test.reb
%datatypes/path.test.reb
%datatypes/percent.test.reb
%datatypes/port.test.reb
%datatypes/refinement.test.reb
%datatypes/set-path.test.reb
%datatypes/set-word.test.reb
%datatypes/string.test.reb
%datatypes/symbol.test.reb
%datatypes/time.test.reb
%datatypes/tuple.test.reb
%datatypes/typeset.test.reb
%datatypes/unset.test.reb
%datatypes/url.test.reb
%datatypes/varargs.test.reb
%datatypes/vector.test.reb
%datatypes/word.test.reb
%comparison/lesserq.test.reb
%comparison/maximum-of.test.reb
%comparison/equalq.test.reb
%comparison/sameq.test.reb
%comparison/strict-equalq.test.reb
%comparison/strict-not-equalq.test.reb
%context/bind.test.reb
%context/boundq.test.reb
%context/bindq.test.reb
%context/resolve.test.reb
%context/set.test.reb
%context/unset.test.reb
%context/use.test.reb
%context/valueq.test.reb
%control/all.test.reb
%control/any.test.reb
%control/apply.test.reb
%control/attempt.test.reb
%control/break.test.reb
%control/case.test.reb
%control/catch.test.reb
%control/compose.test.reb
%control/continue.test.reb
%control/default.test.reb
%control/disarm.test.reb
%control/do.test.reb
%control/either.test.reb
%control/else.test.reb
%control/for.test.reb
%control/forall.test.reb
%control/for-each.test.reb
%control/forever.test.reb
%control/forskip.test.reb
%control/halt.test.reb
%control/if.test.reb
%control/leave.test.reb
%control/loop.test.reb
%control/map-each.test.reb
%control/reduce.test.reb
%control/remove-each.test.reb
%control/repeat.test.reb
%control/return.test.reb
%control/switch.test.reb
%control/throw.test.reb
%control/try.test.reb
%control/unless.test.reb
%control/until.test.reb
%control/wait.test.reb
%control/while.test.reb
%control/quit.test.reb
%convert/as-binary.test.reb
%convert/as-string.test.reb
%convert/encode.test.reb
%convert/load.test.reb
%convert/mold.test.reb
%convert/to.test.reb
%define/func.test.reb
%convert/to-hex.test.reb
%file/clean-path.test.reb
%file/existsq.test.reb
%file/make-dir.test.reb
%file/open.test.reb
%file/file-typeq.test.reb
%functions/adapt.test.reb
%functions/apply.test.reb
%functions/chain.test.reb
%functions/enclose.test.reb
%functions/enfix.test.reb
%functions/hijack.test.reb
%functions/invisible.test.reb
%functions/redo.test.reb
%functions/specialize.test.reb
%math/absolute.test.reb
%math/add.test.reb
%math/and.test.reb
%math/arcsine.test.reb
%math/arctangent.test.reb
%math/complement.test.reb
%math/cosine.test.reb
%math/difference.test.reb
%math/divide.test.reb
%math/evenq.test.reb
%math/exp.test.reb
%math/log-10.test.reb
%math/log-2.test.reb
%math/log-e.test.reb
%math/mod.test.reb
%math/modulo.test.reb
%math/multiply.test.reb
%math/negate.test.reb
%math/negativeq.test.reb
%math/not.test.reb
%math/oddq.test.reb
%math/positiveq.test.reb
%math/power.test.reb
%math/random.test.reb
%math/remainder.test.reb
%math/round.test.reb
%math/shift.test.reb
%math/signq.test.reb
%math/sine.test.reb
%math/square-root.test.reb
%math/subtract.test.reb
%math/tangent.test.reb
%math/zeroq.test.reb
%reflectors/body-of.test.reb
%secure/protect.test.reb
%secure/unprotect.test.reb
%series/append.test.reb
%series/at.test.reb
%series/back.test.reb
%series/change.test.reb
%series/clear.test.reb
%series/copy.test.reb
%series/difference.test.reb
%series/emptyq.test.reb
%series/exclude.test.reb
%series/find.test.reb
%series/indexq.test.reb
%series/insert.test.reb
%series/intersect.test.reb
%series/last.test.reb
%series/lengthq.test.reb
%series/next.test.reb
%series/ordinals.test.reb
%series/pick.test.reb
%series/poke.test.reb
%series/remove.test.reb
%series/reverse.test.reb
%series/select.test.reb
%series/skip.test.reb
%series/sort.test.reb
%series/split.test.reb
%series/tailq.test.reb
%series/trim.test.reb
%series/union.test.reb
%string/checksum.test.reb
%string/compress.test.reb
%string/decloak.test.reb
%string/decode.test.reb
%string/encode.test.reb
%string/decompress.test.reb
%string/dehex.test.reb
%system/system.test.reb
%system/file.test.reb
%system/gc.test.reb
%call/call.test.reb
%source/text-lines.test.reb
%source/analysis.test.reb
