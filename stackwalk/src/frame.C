/*
 * Copyright (c) 1996-2007 Barton P. Miller
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

#include "stackwalk/h/frame.h"
#include "stackwalk/h/walker.h"
#include "stackwalk/h/swk_errors.h"
#include "stackwalk/h/symlookup.h"

#include <assert.h>

using namespace Dyninst;
using namespace Dyninst::Stackwalker;

Frame::Frame(Walker *parent_walker) :
  ra(0x0),
  fp(0x0),
  sp(0x0),
  sym_value(NULL),
  name_val_set(nv_unset),
  bottom_frame(false),
  frame_complete(false),
  stepper(NULL),
  walker(parent_walker)
{
  assert(walker);
  ra_loc.location = loc_unknown;
  ra_loc.val.addr = 0x0;
  fp_loc.location = loc_unknown;
  fp_loc.val.addr = 0x0;
  sp_loc.location = loc_unknown;
  sp_loc.val.addr = 0x0;
  
  sw_printf("[%s:%u] - Created frame at %p\n", __FILE__, __LINE__, this);
}

Frame *Frame::newFrame(regval_t pc, regval_t sp, regval_t fp, Walker *walker) {
  sw_printf("[%s:%u] - Manually creating frame with %lx, %lx, %lx, %p\n",
	    __FILE__, __LINE__, pc, sp, fp, walker);
  if (!walker) {
    sw_printf("[%s:%u] - Trying to create Frame with NULL Walker\n",
	      __FILE__, __LINE__);
    setLastError(err_badparam, "Walker parameter cannot be NULL when creating frame");
  }
  
  Frame *newframe = new Frame(walker);
  
  newframe->setRA(pc);
  newframe->setSP(sp);
  newframe->setFP(fp);
  
  return newframe;
}

void Frame::setStepper(FrameStepper *newstep) {
  sw_printf("[%s:%u] - Setting frame %p's stepper to %p\n", 
	    __FILE__, __LINE__, this, newstep);
  stepper = newstep;
}

void Frame::markBottomFrame() {
  sw_printf("[%s:%u] - Marking frame %p as bottom\n", 
	    __FILE__, __LINE__, this);
  bottom_frame = true;
}

regval_t Frame::getRA() const {
  return ra;
}

regval_t Frame::getSP() const {
  return sp;
}

regval_t Frame::getFP() const {
  return fp;
}

location_t Frame::getRALocation() const {
  return ra_loc;
}

location_t Frame::getSPLocation() const {
  return sp_loc;
}

location_t Frame::getFPLocation() const {
  return fp_loc;
}

void Frame::setRA(regval_t newval) {
  sw_printf("[%s:%u] - Setting ra of frame %p to %x\n",
	    __FILE__, __LINE__, this, newval);
  ra = newval;
  frame_complete = true;
}

void Frame::setFP(regval_t newval) {
  sw_printf("[%s:%u] - Setting fp of frame %p to %x\n",
			  __FILE__, __LINE__, this, newval);
  fp = newval;
}

void Frame::setSP(regval_t newval) {
  sw_printf("[%s:%u] - Setting sp of frame %p to %x\n",
	    __FILE__, __LINE__, this, newval);
  sp = newval;
}

static void debug_print_location(const char *s, Frame *f, location_t val) {
  if (val.location == loc_address)
    sw_printf("[%s:%u] - Setting frame %p %s location to address %lx\n",
	      __FILE__, __LINE__, f, s, val.val.addr);
  else if (val.location == loc_register)
    sw_printf("[%s:%u] - Setting frame %p %s location to register %lx\n",
	      __FILE__, __LINE__, f, s, val.val.reg);
  else if (val.location == loc_unknown)
    sw_printf("[%s:%u] - Setting frame %p %s location to unknown\n",
	      __FILE__, __LINE__, f, s);
}

void Frame::setRALocation(location_t newval) {
  if (dyn_debug_stackwalk) {
    debug_print_location("RA", this, newval);
  }
  ra_loc = newval;
}

void Frame::setSPLocation(location_t newval) {
  if (dyn_debug_stackwalk) {
    debug_print_location("SP", this, newval);
  }
  sp_loc = newval;
}

void Frame::setFPLocation(location_t newval) {
  if (dyn_debug_stackwalk) {
    debug_print_location("FP", this, newval);
  }
  fp_loc = newval;
}

void Frame::setNameValue() {
  if (name_val_set == nv_set || name_val_set == nv_err)
    return;
  
  if (!walker) {
    setLastError(err_nosymlookup, "No Walker object was associated with this frame");
    sw_printf("[%s:%u] - Error, No walker found.\n", __FILE__, __LINE__);
    name_val_set = nv_err;
    return;
  }
  
  SymbolLookup *lookup = walker->getSymbolLookup();
  if (!lookup) {
    setLastError(err_nosymlookup, "No SymbolLookup object was associated with the Walker");
    sw_printf("[%s:%u] - Error, No symbol lookup found.\n", __FILE__, __LINE__);
    name_val_set = nv_err;
    return;
  }
  
  bool result = lookup->lookupAtAddr(getRA(), sym_name, sym_value);
  if (!result) {
    sw_printf("[%s:%u] - Error, returned by lookupAddr.\n", __FILE__, __LINE__);
    name_val_set = nv_err;
  }
  
  sw_printf("[%s:%u] - Successfully looked up symbol for frame %p\n",
	    __FILE__, __LINE__, this);
  
  name_val_set = nv_set;
}

bool Frame::getName(std::string &str) {
  setNameValue();
  if (name_val_set == nv_set) {
    str = sym_name;
    sw_printf("[%s:%u] - Frame::getName (frame %p) returning %s\n",
	      __FILE__, __LINE__, this, str.c_str());
    return true;
  }
  else {
    sw_printf("[%s:%u] - Frame::getName (frame %p) returning error\n",
	      __FILE__, __LINE__, this);
    return false;
  }
}

bool Frame::getObject(void* &obj) {
  setNameValue();
  if (name_val_set == nv_set) {
    obj = sym_value;
    sw_printf("[%s:%u] - Frame::getObject (frame %p) returning %p\n",
	      __FILE__, __LINE__, this, obj);
    return true;
  }
  else {
    sw_printf("[%s:%u] - Frame::getObject (frame %p) returning error\n",
	      __FILE__, __LINE__, this);
    return false;
  }
}

bool Frame::isBottomFrame() const {
  return bottom_frame;
}
	
FrameStepper *Frame::getStepper() const {
  return stepper;
}

Walker *Frame::getWalker() const {
  return walker;
}

bool Frame::isFrameComplete() const {
  return frame_complete;
}

Frame::~Frame() {
  sw_printf("[%s:%u] - Destroying frame %p\n", __FILE__, __LINE__, this);
}
