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

/************************************************************************
 * $Id: Symbol.h,v 1.20 2008/11/03 15:19:24 jaw Exp $

************************************************************************/

// Present a common superclass for all Symbol aggregates. 
// We never create an Aggregate directly, but only via a child class.

#if !defined(_Aggregate_h_)
#define _Aggregate_h_

#include <iostream>
#include "Annotatable.h"

SYMTAB_EXPORT std::ostream &operator<<(std::ostream &os, const Dyninst::SymtabAPI::Aggregate &);

namespace Dyninst{
namespace SymtabAPI{

class Symbol;
class Module;
class Symtab; 
class Region; 
class Aggregate; 
class DwarfWalker;

struct SymbolCompareByAddr;

class SYMTAB_EXPORT Aggregate
{
   friend class Symtab;
   friend struct SymbolCompareByAddr;
	friend std::ostream &::operator<<(std::ostream &os, const Dyninst::SymtabAPI::Aggregate &);
   friend class DwarfWalker;
  protected:
      Aggregate();
      Aggregate(Symbol *sym);
      Aggregate(Module *m);      
  public:

      
      virtual ~Aggregate() {};

      Offset   getOffset() const;
      unsigned getSize() const;
      Module * getModule() const { return module_; }
      Region * getRegion() const;

      /***** Symbol Collection Management *****/
      bool addSymbol(Symbol *sym);
      virtual bool removeSymbol(Symbol *sym) = 0;
      bool getSymbols(std::vector<Symbol *> &syms) const;
      Symbol *getFirstSymbol() const;

      /***** Symbol naming *****/
      const std::vector<std::string>& getAllMangledNames() const;
      const std::vector<std::string>& getAllPrettyNames() const;
      const std::vector<std::string>& getAllTypedNames() const;

      /***** Aggregate updating *****/
      virtual bool addMangledName(std::string name, bool isPrimary);
      virtual bool addPrettyName(std::string name, bool isPrimary);
      virtual bool addTypedName(std::string name, bool isPrimary);

      bool setModule(Module *mod);
      bool setSize(unsigned size);
      bool setOffset(unsigned offset);
      
	  bool operator==(const Aggregate &a) const;

   protected:
      bool removeSymbolInt(Symbol *sym);
      virtual bool changeSymbolOffset(Symbol *sym);

      // Offset comes from a symbol
      // Module we keep here so we can have the correct "primary"
      // (AKA 'not DEFAULT_MODULE') module
      Module *module_;

      std::vector<Symbol *> symbols_;
      Symbol *firstSymbol;  // cached for speed
      Offset offset_;       // cached for speed

      std::vector<std::string> mangledNames_;
      std::vector<std::string> prettyNames_;
      std::vector<std::string> typedNames_;

	  void restore_type_by_id(SerializerBase *, Type *&, unsigned) THROW_SPEC (SerializerError);
	  void restore_module_by_name(SerializerBase *, std::string &) THROW_SPEC (SerializerError);
	  //void rebuild_symbol_vector(SerializerBase *, std::vector<Offset> *) THROW_SPEC (SerializerError);
	  void rebuild_symbol_vector(SerializerBase *, std::vector<Address> &) THROW_SPEC (SerializerError);
	  void serialize_aggregate(SerializerBase *, const char * = "Aggregate") THROW_SPEC (SerializerError);
     bool addMangledNameInternal(std::string name, bool isPrimary, bool demangle);
};

}
}

#endif
