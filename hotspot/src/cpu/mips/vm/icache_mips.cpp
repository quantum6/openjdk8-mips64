/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2015, 2016, Loongson Technology. All rights reserved.
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
#include "asm/macroAssembler.hpp"
#include "runtime/icache.hpp"
#include <asm/cachectl.h>
#include <sys/cachectl.h>
#include <sys/sysmips.h>

#ifdef _LP64
  #define CACHE_OPT 1
#endif

//no need, we just call cacheflush system call to flush cache
//flush cache is a very frequent operation, flush all the cache decrease the performance sharply, so i modify it.
void ICacheStubGenerator::generate_icache_flush(ICache::flush_icache_stub_t* flush_icache_stub) {};

void ICache::call_flush_stub(address start, int lines) {
  //in fact, the current os implementation simply flush all ICACHE&DCACHE
#ifndef CACHE_OPT
  /* Loongson3A supports automatic synchronization between Icache and Dcache.
         * No manual synchronization is needed. */
  cacheflush(start, lines * line_size , ICACHE);
#endif
//  sysmips(3, 0, 0, 0);
}

void ICache::invalidate_word(address addr) {
  //cacheflush(addr, 4, ICACHE);

#ifndef CACHE_OPT
  cacheflush(addr,4, ICACHE);
#endif
//  sysmips(3, 0, 0, 0);
}

void ICache::invalidate_range(address start, int nbytes) {
#ifndef CACHE_OPT
  cacheflush(start, nbytes, ICACHE);
#endif
//  sysmips(3, 0, 0, 0);
}

void ICache::invalidate_all() {
#ifndef CACHE_OPT
  sysmips(3, 0, 0, 0);
#endif
}

