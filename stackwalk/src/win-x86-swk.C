/*
 * Copyright (c) 1996-2008 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stackwalk/h/walker.h"
#include "stackwalk/h/framestepper.h"
#include "stackwalk/h/steppergroup.h"
#include "stackwalk/h/swk_errors.h"
#include "common/h/ntHeaders.h"

#include <windows.h>

using namespace Dyninst;
using namespace Dyninst::Stackwalker;

bool Walker::createDefaultSteppers()
{
  FrameStepper *stepper;
  bool result = true;

  stepper = new FrameFuncStepper(this);
  result = addStepper(stepper);
  if (!result)
     goto error;
  sw_printf("[%s:%u] - Stepper %p is FrameFuncStepper\n",
            __FILE__, __LINE__, stepper);

  return true;
 error:
  sw_printf("[%s:%u] - Error adding stepper %p\n", stepper);
  return false;
}

bool ProcSelf::getThreadIds(std::vector<THR_ID> &threads)
{
  bool result;
  THR_ID tid;

  result = getDefaultThread(tid);
  if (!result) {
    sw_printf("[%s:%u] - Could not read default thread\n",
	       __FILE__, __LINE__);
    return false;
  }
  threads.clear();
  threads.push_back(tid);
  return true;
}

bool ProcSelf::getDefaultThread(THR_ID &default_tid)
{
  default_tid = GetCurrentThread();
  return true;
}

bool ProcSelf::readMem(void *dest, Address source, size_t size)
{
   memcpy(dest, (const void *) source, size);
   return true;
}

ProcSelf::ProcSelf() :
   ProcessState(_getpid())
{
}

void ProcSelf::initialize()
{
}

ProcDebug *ProcDebug::newProcDebug(PID pid)
{
  return NULL;
}

ProcDebug *ProcDebug::newProcDebug(const std::string &executable, 
                                   const std::vector<std::string> &argv)
{
  return NULL;
}

DebugEvent ProcDebug::debug_get_event(bool block)
{
  return DebugEvent();
}