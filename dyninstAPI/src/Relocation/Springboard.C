/*
 * See the dyninst/COPYRIGHT file for copyright information.
 * 
 * We provide the Paradyn Tools (below described as "Paradyn")
 * on an AS IS basis, and do not warrant its validity or performance.
 * We reserve the right to update, modify, or discontinue this
 * software at any time.  We shall have no obligation to supply such
 * updates or modifications or any other form of support to you.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "CFG.h"
#include "Springboard.h"
#include "dyninstAPI/src/debug.h"
#include "dyninstAPI/src/codegen.h"

#include "dyninstAPI/src/addressSpace.h"
#include "dyninstAPI/src/function.h"

using namespace Dyninst;
using namespace Relocation;

extern int dyn_debug_trap;

const int InstalledSpringboards::Allocated(0);
const int InstalledSpringboards::UnallocatedStart(1);

SpringboardBuilder::SpringboardBuilder(AddressSpace* a)
 : addrSpace_(a), 
   installed_springboards_(a->getInstalledSpringboards())
{
}


template <typename BlockIter> 
SpringboardBuilder::Ptr SpringboardBuilder::create(BlockIter begin,
						   BlockIter end,
						   AddressSpace *as) {
  Ptr ret = Ptr(new SpringboardBuilder(as));
  if (!ret) return ret;

  if (!ret->addBlocks(begin, end)) return Ptr();
  return ret;
}

SpringboardBuilder::Ptr SpringboardBuilder::createFunc(FuncSet::const_iterator begin,
						       FuncSet::const_iterator end,
						       AddressSpace *as) 
{
  Ptr ret = Ptr(new SpringboardBuilder(as));
  if (!ret) return ret;
  for (; begin != end; ++begin) {
     func_instance *func = *begin;
     //if (!ret->addBlocks(func->blocks().begin(), func->blocks().end(), func, id++)) {
     //if (!ret->addBlocks(func->blocks().begin(), func->blocks().end())) {
     if(!ret->installed_springboards_->addFunc(func)) 
     {
        return Ptr();
     }
  }
  return ret;
}

bool SpringboardBuilder::generateInt(std::list<codeGen> &springboards,
                                     SpringboardMap &input,
                                     Priority p) {
   // We want to do a reverse iteration so that we don't have a situation
   // where an earlier springboard overlaps a later one.
   //
   
   for (SpringboardMap::reverse_iterator iter = input.rbegin(p); 
        iter != input.rend(p); ++iter) {
      const SpringboardReq &req = iter->second;
      
      switch (generateSpringboard(springboards, req, input)) {
         case Failed:
            if (p == Required) {
               return false;
            }
            // Otherwise we didn't need it anyway.
            break;
         case MultiNeeded:
            // We want to try some multi-step jump to get to the relocated code. 
            // We have to wait until all the primaries are done first; effectively
            // a greedy algorithm of largest first.
            multis_.push_back(req);
            break;
         case Succeeded:
            // Good!
            break;
      }
   }
   return true;
}

bool SpringboardBuilder::generate(std::list<codeGen> &springboards,
				  SpringboardMap &input) {
  // The SpringboardMap gives us a, well, mapping of desired
  // from: addresses and to: addresses. Our job is to create a series
  // of branches (and traps) that will take care of that. 

  // Currently we use a greedy algorithm rather than some sort of scheduling thing.
  // It's a heck of a lot easier that way. 

   if (!generateInt(springboards, input, Required))
      return false;

   if (!generateInt(springboards, input, Suggested))
      return false;

   // Catch up with instrumentation
   if (!generateInt(springboards, input, RelocRequired)) 
      return false;
   if (!generateInt(springboards, input, RelocSuggested))
      return false;

   return true;
}

template <typename BlockIter>
bool SpringboardBuilder::addBlocks(BlockIter begin, BlockIter end)
{
  return installed_springboards_->addBlocks(begin, end);
  
}

bool InstalledSpringboards::addFunc(func_instance* func)
{
  if(!addBlocks(func->blocks().begin(), func->blocks().end())) return false;
  nextFuncID_++;
  return true;
}


template <typename BlockIter>
bool InstalledSpringboards::addBlocks(BlockIter begin, BlockIter end) {
  // TODO: map these addresses to relocated blocks as well so we 
  // can do our thang.
  for (; begin != end; ++begin) {
     block_instance *bbl = SCAST_BI(*begin);

     if (bbl->wasUserAdded()) continue;
     // Don't try to springboard a user-added block...

    // Check for overlapping blocks. Lovely.
    Address LB, UB; int id;
    Address start = bbl->start();
    Address end = bbl->end();

#if 0
    // HACK for small, aligned functions
    if ((f->blocks().size() == 1) &&
        (start % 16 == 0) &&
        ((end - start) < 16)) {
       end = start + 16;
    }
#endif

    for (Address lookup = start; lookup < end; ) 
    {/* there may be more than one range that overlaps with bbl, 
      * so we update lookup and start to after each conflict
      * to match UB, and loop until lookup >= end
      */
       if (validRanges_.find(lookup, LB, UB, id)) 
       {
          /* The ranges overlap and we must split them into non-
           * overlapping ranges, possible range splits are listed
           * below:
           *
           * [LB UB)                     already in validRanges_, remove if it gets split
           * [LB bbl->start())
           * [bbl->start() LB)
           * [start LB)         when bbl overlaps multiple ranges and LB is for an N>1 range
           * [bbl->start() UB)           possible if LB < bbl->start()
           * [start UB)         possible if LB < bbl->start() and bbl overlaps multiple ranges
           * [LB end)             if last existing range includes end
           * [end UB)             if last existing range includes end
           * [UB end)             don't add until after loop exits as there might be more overlapping ranges
           */
          if (LB < start) { 
             validRanges_.remove(LB); // replace [LB UB)...
             validRanges_.insert(LB, start, nextFuncID_); // with  [LB start)
             if (UB <= end) { // [start UB)
                validRanges_.insert(start, UB, nextFuncID_);
             } else { // [start end) or [start end) and [end UB) 
                validRanges_.insert(start, end, nextFuncID_);
                validRanges_.insert(end, UB, nextFuncID_);
             }
          } 
          else {
             if (start < LB) { // add [start LB) or [start LB)
                validRanges_.insert(start, LB, nextFuncID_);
             }
             if (UB > end) { // [LB end) and [end UB) 
                validRanges_.insert(LB, end, nextFuncID_);
                validRanges_.insert(end, UB, nextFuncID_);
             } // otherwise [LB UB) is already in validRanges_
          }
          lookup = UB;
          start = UB;
       }
       else {
          lookup++;
       }
    }
    if (start < end) { // [start end) or [UB end)
        validRanges_.insert(start, end, nextFuncID_);
    }
  }
  return true;
}

SpringboardBuilder::generateResult_t 
SpringboardBuilder::generateSpringboard(std::list<codeGen> &springboards,
					const SpringboardReq &r,
                                        SpringboardMap &input) {
   codeGen gen;

   bool usedTrap = false;
   // Arbitrarily select the first function containing this springboard, since only one can win. 
   generateBranch(r.from, r.destinations.begin()->second, gen);
   unsigned size = gen.used();
   
   if (r.useTrap || conflict(r.from, r.from + gen.used(), r.fromRelocatedCode)) {
      // Errr...
      // Fine. Let's do the trap thing. 

      usedTrap = true;
      if (conflict(r.from, r.from + 1, r.fromRelocatedCode)) return Failed;
      if(!addrSpace_->canUseTraps()) return Failed;
      
      generateTrap(r.from, r.destinations.begin()->second, gen);
      size = 1;
   }

   if (r.includeRelocatedCopies) {
      createRelocSpringboards(r, usedTrap, input);
   }
   
   registerBranch(r.from, r.from + size, r.destinations, r.fromRelocatedCode);
   springboards.push_back(gen);

   return Succeeded;
}

bool SpringboardBuilder::generateMultiSpringboard(std::list<codeGen> &,
						  const SpringboardReq &) {
   //debugRanges();
   //if (false) cerr << "Request to generate multi-branch springboard skipped @ " << hex << r.from << dec << endl;
   // For now we give up and hope it all works out for the best. 
   return true;
}

bool InstalledSpringboards::conflict(Address start, Address end, bool inRelocated) {
   if (inRelocated) 
       return conflictInRelocated(start, end);

   // We require springboards to stay within a particular block
   // so that we don't have issues with jumping into the middle
   // of a branch (... ew). Therefore, there is a conflict if
   // the end address lies within a different range than the start.
   //
   // Technically, end can be the start of another range; therefore
   // we search for (end-1).

    // We also don't want to start in one function's dead space and cross
   // into another's. So check to see if state suddenly changes.


   // The above comment is wholly wrong, but leaving it in and responding to it for the moment.
   // Springboards *should* stay within a particular block, but we attempt to address this by
   // ensuring that higher priority springboards are installed before lower priority ones
   // (which is to say mandatory before optional), and by processing them from high addresses
   // to low ones in each priority group. We glue arbitrary blocks together at each priority level.

   // NOTE: this approach may be responsible for our reinstrumentation bug, where we have the same
   // problem with "installed 2 byte branch, think we now have space for 5 byte branch"
   // that we do with "installed trap, think we now have space for a branch". Need to determine how to properly
   // avoid expanding springboards when reinstrumenting in a general way, not just in the trap/branch case.

   // And now that we're working with a per-address-space set of data structures, we have to keep track of
   // multiple relocation invocations' worth of requests. However, we know that in a given batch of springboard
   // requests, we can't have two springboards starting at the same address. SO:
   // If we find that we're overwriting at the same address as an existing springboard
   // and we're not running into another interval, there's no conflict (replacing a springboard).
   // If we find *any* other case where we're hitting allocated, conflict.
   Address working = start;
   Address LB;
   Address UB = 0;
   int state = -1;
   int lastState = state;
   springboard_cerr << "Conflict called for " << hex << start << "->" << end << dec << endl;
   
   while (end > working) {
        springboard_cerr << "\t looking for " << hex << working << dec << endl;
       if (!validRanges_.find(working, LB, UB, state)) {
         springboard_cerr << "\t Conflict: unable to find entry for " << hex << working << dec << endl;
         return true;
      }
      springboard_cerr << "\t\t Found " << hex << LB << " -> " << UB << " /w/ state " << state << dec << endl;
      if (state == Allocated) {
	if(LB == start && UB >= end) 
	{
	  springboard_cerr << "\t Starting range matches already allocated springboard, assuming overwrite, ret OK" << endl;
	  return false;
	}
	
         springboard_cerr << "\t Starting range already allocated, ret conflict" << endl;
         return true;
      }
      if (lastState != -1 &&
          state != lastState) {
          springboard_cerr << "\t Crossed into a different function, ret conflict" << endl;
          return true;
      }
      working = UB;
      lastState = state;
   }
   if (UB < end) {
       return true;
   }
   springboard_cerr << "\t No conflict, we're good" << endl;
   return false;
}

bool InstalledSpringboards::conflictInRelocated(Address start, Address end) {
   // Much simpler case: do we overlap something already in the range set, 
   // or did we use a trap for this block initially
   for (Address i = start; i < end; ++i) {
      Address lb, ub;
      bool val;
      if (overwrittenRelocatedCode_.find(i, lb, ub, val)) {
         // oops!
         return true;
      }
   }
   if ( (end-start) > 1 && relocTraps_.end() != relocTraps_.find(start) ) {
#if 0
       malware_cerr << "Springboard conflict for " << hex << start  
           << " our previous springboard here needed a trap, "
           << "but due to overwrites we may (erroneously) think "
           << "a branch can fit" << dec << endl;
       springboard_cerr << "Springboard conflict for " << hex << start  
           << " our previous springboard here needed a trap, "
           << "but due to overwrites we may (erroneously) think "
           << "a branch can fit" << dec << endl;
#endif
       return true;
   }

   return false;
}

void InstalledSpringboards::registerBranch
(Address start, Address end, const SpringboardReq::Destinations & dest, bool inRelocated) 
{
   // Remove the valid ranges for everything between start and end, using much the 
   // same logic as above.

   if ( 1 == (end - start) ) {
       for (SpringboardReq::Destinations::const_iterator dit=dest.begin();
            dit != dest.end();
            dit++)
       {
           relocTraps_.insert(start);
           relocTraps_.insert(dit->second);// if we relocate again it will need a trap too
       }
   }
    
   if (inRelocated) {
      return registerBranchInRelocated(start, end);
   }

   Address working = start;
   Address LB = 0, UB = 0;
   Address lb = 0, ub = 0;
   springboard_cerr << "Adding branch: " << hex << start << " -> " << end << dec << endl;
   int idToUse = -1;
   while (end > working) {
      int state = 0;
      validRanges_.find(working, lb, ub, state);
      validRanges_.remove(lb);

      if (idToUse == -1) idToUse = state;
      else {
         if (idToUse != state) {
            cerr << "Error: idToUse " << idToUse << " and state " << state << endl;
         }
         assert(idToUse == state);
      }
      if (LB == 0) LB = lb;
      working = ub;
   }
   if (UB == 0) UB = ub;

   // Add three ranges:
   // [lb..start] as true
   // [start..end] as false
   // [end..ub] as true
   if (LB < start) {
        springboard_cerr << "\tInserting prior space " << hex << LB << " -> " << start << " /w/ range " << idToUse << dec << endl;
       validRanges_.insert(LB, start, idToUse);
   }
    springboard_cerr << "\t Inserting taken space " << hex << start << " -> " << end << " /w/ range " << Allocated << dec << endl;
   validRanges_.insert(start, end, Allocated);
   if (UB > end) {
        springboard_cerr << "\tInserting post space " << hex << end << " -> " << UB << " /w/ range " << idToUse << dec << endl;
      validRanges_.insert(end, UB, idToUse);
   }
}

void InstalledSpringboards::registerBranchInRelocated(Address start, Address end) {
   overwrittenRelocatedCode_.insert(start, end, true);
}


void InstalledSpringboards::debugRanges() {
  std::vector<std::pair<std::pair<Address, Address>, int> > elements;
  validRanges_.elements(elements);
  if (false) cerr << "Range debug: " << endl;
  for (unsigned i = 0; i < elements.size(); ++i) {
     if (false) cerr << "\t" << hex << elements[i].first.first
	 << ".." << elements[i].first.second << dec
	 << " -> " << elements[i].second << endl;
  }
  if (false) cerr << "-------------" << endl;
}

void SpringboardBuilder::generateBranch(Address from, Address to, codeGen &gen) {
  gen.invalidate();
  gen.allocate(16);

  gen.setAddrSpace(addrSpace_);
  gen.setAddr(from);

  insnCodeGen::generateBranch(gen, from, to);

  springboard_cerr << "Generated springboard branch " << hex << from << "->" << to << dec << endl;
}

void SpringboardBuilder::generateTrap(Address from, Address to, codeGen &gen) {
   // This has to be an AddressSpace method, since we use trap instructions
   // for the rewriter and ProcControl methods for dynamic mode.
   addrSpace_->addTrap(from, to, gen);
}

bool SpringboardBuilder::createRelocSpringboards(const SpringboardReq &req, 
                                                 bool useTrap, SpringboardMap &input) {
   assert(!req.fromRelocatedCode);

   // Just the requests for now.
   springboard_cerr << "createRelocSpringboards for " << hex << req.from << dec << endl;
   std::list<Address> relocAddrs;
   block_instance *bbl = req.block;
   for (SpringboardReq::Destinations::const_iterator b_iter = req.destinations.begin(); 
       b_iter != req.destinations.end(); ++b_iter) {
      func_instance *func = b_iter->first;
      Address addr = b_iter->second;
      springboard_cerr << "Looking for addr " << hex << addr << " in function " << func->name() << dec << endl;

      addrSpace_->getRelocAddrs(req.from, bbl, func, relocAddrs, true);
      addrSpace_->getRelocAddrs(req.from, bbl, func, relocAddrs, false);
      for (std::list<Address>::const_reverse_iterator a_iter = relocAddrs.rbegin(); 
           a_iter != relocAddrs.rend(); ++a_iter) { 
         //springboard_cerr << "\t Mapped address " << hex << *a_iter << dec << endl;
         if (*a_iter == addr) continue;
         Priority newPriority;
         switch(req.priority) {
            case Suggested:
               newPriority = RelocSuggested;
               break;
            case Required:
               newPriority = RelocRequired;
               break;
            default:
               assert(0);
               break;
         }
         bool curUseTrap = useTrap;
         if ( !useTrap && installed_springboards_->forceTrap(*a_iter)) {
            springboard_cerr << "Springboard conflict for " << hex 
                             << req.from << "[" << (*a_iter) 
                             << "] our previous springboard here needed a trap, "
                             << "but due to overwrites we may (erroneously) think "
                             << "a branch can fit" << dec << endl;
            curUseTrap = true;
         }
         springboard_cerr << "Adding springboard from " << hex << *a_iter << " to " << addr << dec << endl;
         input.addRaw(*a_iter, addr, 
                      newPriority, func, bbl,
                      req.checkConflicts, 
                      false, true, curUseTrap);
         
      }
   }
   return true;
}


