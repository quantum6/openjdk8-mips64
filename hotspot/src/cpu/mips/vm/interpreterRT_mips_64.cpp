/*
 * Copyright (c) 2003, 2012, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2018, Loongson Technology. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/universe.inline.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/signature.hpp"

#define __ _masm->

// Implementation of SignatureHandlerGenerator

void InterpreterRuntime::SignatureHandlerGenerator::move(int from_offset, int to_offset) {
  __ ld(temp(), from(), Interpreter::local_offset_in_bytes(from_offset));
  __ sd(temp(), to(), to_offset * longSize);
}

void InterpreterRuntime::SignatureHandlerGenerator::box(int from_offset, int to_offset) {
  __ addi(temp(), from(),Interpreter::local_offset_in_bytes(from_offset) );
  __ lw(AT, from(), Interpreter::local_offset_in_bytes(from_offset) );

  Label L;
  __ bne(AT, R0, L);
  __ delayed()->nop();
  __ move(temp(), R0);
  __ bind(L);
  __ sw(temp(), to(), to_offset * wordSize);
}

void InterpreterRuntime::SignatureHandlerGenerator::generate(uint64_t fingerprint) {
  // generate code to handle arguments
  iterate(fingerprint);
  // return result handler
  __ li(V0, AbstractInterpreter::result_handler(method()->result_type()));
  // return
  __ jr(RA);
  __ delayed()->nop();

  __ flush();
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_int() {
  Argument jni_arg(jni_offset());
  __ lw(temp(), from(), Interpreter::local_offset_in_bytes(offset()));
  __ store_int_argument(temp(), jni_arg);
}

void InterpreterRuntime::SignatureHandlerGenerator::pass_object() {
  Argument jni_arg(jni_offset());

  Register Rtmp1 = temp();

  // the handle for a receiver will never be null
  bool do_NULL_check = offset() != 0 || is_static();
  __ ld(Rtmp1, from(), Interpreter::local_offset_in_bytes(offset()));

  Label L;
  __ bne(Rtmp1, R0, L);
  __ delayed()->daddiu(Rtmp1, from(), Interpreter::local_offset_in_bytes(offset()));
  __ move(Rtmp1, R0);
  __ bind(L);

  __ store_ptr_argument(Rtmp1, jni_arg);
}

//the jvm specifies that long type takes 2 stack spaces, so in do_long(), _offset += 2.
void InterpreterRuntime::SignatureHandlerGenerator::pass_long() {
  Argument jni_arg(jni_offset());
  __ ld(temp(), from(), Interpreter::local_offset_in_bytes(offset() + 1));
  if(jni_arg.is_Register()) {
    __ move(jni_arg.as_Register(), temp());
  } else {
    __ sd(temp(), jni_arg.as_caller_address());
  }
}

#if (defined _LP64) || (defined N32)
void InterpreterRuntime::SignatureHandlerGenerator::pass_float() {
  Argument jni_arg(jni_offset());
  __ lwc1(F4, from(), Interpreter::local_offset_in_bytes(offset()));
  __ store_float_argument(F4, jni_arg);
}


//the jvm specifies that double type takes 2 stack spaces, so in do_double(), _offset += 2.
void InterpreterRuntime::SignatureHandlerGenerator::pass_double() {
  Argument jni_arg(jni_offset());
  __ ldc1(F4, from(), Interpreter::local_offset_in_bytes(offset() + 1));
  __ store_double_argument(F4, jni_arg);
}
#endif


Register InterpreterRuntime::SignatureHandlerGenerator::from()       { return LVP; }
Register InterpreterRuntime::SignatureHandlerGenerator::to()         { return SP; }
Register InterpreterRuntime::SignatureHandlerGenerator::temp()       { return RT4; }

// Implementation of SignatureHandlerLibrary

void SignatureHandlerLibrary::pd_set_handler(address handler) {}


class SlowSignatureHandler
  : public NativeSignatureIterator {
 private:
  address   _from;
  intptr_t* _to;
  intptr_t* _reg_args;
  intptr_t* _fp_identifiers;
  unsigned int _num_args;

  virtual void pass_int()
  {
    jint from_obj = *(jint *)(_from+Interpreter::local_offset_in_bytes(0));
    _from -= Interpreter::stackElementSize;

    if (_num_args < Argument::n_register_parameters) {
      *_reg_args++ = from_obj;
      _num_args++;
    } else {
      *_to++ = from_obj;
    }
  }

  virtual void pass_long()
  {
    intptr_t from_obj = *(intptr_t*)(_from+Interpreter::local_offset_in_bytes(1));
    _from -= 2 * Interpreter::stackElementSize;

    if (_num_args < Argument::n_register_parameters) {
      *_reg_args++ = from_obj;
      _num_args++;
    } else {
      *_to++ = from_obj;
    }
  }

  virtual void pass_object()
  {
    intptr_t *from_addr = (intptr_t*)(_from + Interpreter::local_offset_in_bytes(0));
    _from -= Interpreter::stackElementSize;
    if (_num_args < Argument::n_register_parameters) {
      *_reg_args++ = (*from_addr == 0) ? NULL : (intptr_t) from_addr;
      _num_args++;
    } else {
      *_to++ = (*from_addr == 0) ? NULL : (intptr_t) from_addr;
    }
  }

  virtual void pass_float()
  {
    jint from_obj = *(jint *)(_from+Interpreter::local_offset_in_bytes(0));
    _from -= Interpreter::stackElementSize;

    if (_num_args < Argument::n_float_register_parameters) {
      *_reg_args++ = from_obj;
      *_fp_identifiers |= (0x01 << (_num_args*2)); // mark as float
      _num_args++;
    } else {
      *_to++ = from_obj;
    }
  }

  virtual void pass_double()
  {
    intptr_t from_obj = *(intptr_t*)(_from+Interpreter::local_offset_in_bytes(1));
    _from -= 2*Interpreter::stackElementSize;

    if (_num_args < Argument::n_float_register_parameters) {
      *_reg_args++ = from_obj;
      *_fp_identifiers |= (0x3 << (_num_args*2)); // mark as double
      _num_args++;
    } else {
      *_to++ = from_obj;
    }
  }

 public:
  SlowSignatureHandler(methodHandle method, address from, intptr_t* to)
    : NativeSignatureIterator(method)
  {
    _from = from;
    _to   = to;

    _reg_args = to - Argument::n_register_parameters + jni_offset() - 1;
    _fp_identifiers = to - 1;
    *(int*) _fp_identifiers = 0;
    _num_args = jni_offset();
  }
};


IRT_ENTRY(address,
          InterpreterRuntime::slow_signature_handler(JavaThread* thread,
                                                     Method* method,
                                                     intptr_t* from,
                                                     intptr_t* to))
  methodHandle m(thread, (Method*)method);
  assert(m->is_native(), "sanity check");

  // handle arguments
  SlowSignatureHandler(m, (address)from, to).iterate(UCONST64(-1));

  // return result handler
  return Interpreter::result_handler(m->result_type());
IRT_END
