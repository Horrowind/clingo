//
// Copyright (c) 2016, Benjamin Kaufmann
//
// This file is part of Clasp. See http://www.cs.uni-potsdam.de/clasp/
//
// Clasp is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Clasp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Clasp; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
#ifndef CLASP_STATISTICS_H_INCLUDED
#define CLASP_STATISTICS_H_INCLUDED

#ifdef _MSC_VER
#pragma once
#endif
#include <clasp/util/platform.h>
#include <clasp/pod_vector.h>
#include <clasp/claspfwd.h>
#include <potassco/clingo.h>
namespace Clasp {

//! Discriminated union representing either a single statistic value or a composite.
class StatisticObject {
public:
	typedef Potassco::Statistics_t Type;
	struct Hasher { std::size_t operator()(const StatisticObject& o) const { return o.hash(); } };
	//! Creates an empty (invalid) object.
	StatisticObject();
	//! Creates a Value object - static_cast<double>(*obj) shall be valid.
	template <class T>
	static StatisticObject value(const T* obj) {
		return value<T, &StatisticObject::_getValue>(obj);
	}
	//! Creates a mapped Value object: f(obj) -> double
	template <class T, double(*op)(const T*)>
	static StatisticObject value(const T* obj) {
		return StatisticObject(obj, registerValue<T, op>());
	}
	//! Creates a Map object.
	/*!
	 * The following expression shall be valid:
	 * obj->size(): shall return the number of keys in obj
	 * obj->key(i): shall return the i'th key of this object (i >= 0).
	 * obj->at(const char* k): shall return the StatisticObject under the given key.
	 *  If k is invalid, shall either throw an exception or return an empty object.
	 */
	template <class T>
	static StatisticObject map(const T* obj) {
		return StatisticObject(obj, registerMap<T>());
	}
	//! Creates an Array object.
	/*!
	 * The following expression shall be valid:
	 * obj->size(): shall return the size of the array.
	 * obj->at(i): shall return the StatisticObject under the given key i >= 0.
	 *  If k is invalid, shall either throw an exception or return an empty object.
	 */
	template <class T>
	static StatisticObject array(const T* obj) {
		return StatisticObject(obj, registerArray<T>());
	}
	//! Returns the type of this object.
	Type   type()  const;
	//! Returns whether this object is empty.
	bool   empty() const;
	//! Returns the number of children of this object or 0 if this is not a composite object.
	uint32 size()  const;
	
	/*!
	 * \name Map
	 * \pre type() == Map
	 */
	//@{
	//! Returns the i'th key of this map.
	/*!
	 * \pre i < size()
	 */
	const char*     key(uint32 i)     const;
	//! Returns the object under the given key.
	/*!
	 * \pre k in key([0..size()))
	 */
	StatisticObject at(const char* k) const;
	//@}
	
	//! Returns the object at the given index.
	/*!
	 * \pre Type() == Array
	 * \pre i < size()
	 */
	StatisticObject operator[](uint32 i) const;

	//! Returns the value of this object.
	/*!
	 * \pre type() == Value
	 */
	double value() const;

	bool operator==(const StatisticObject& rhs) const {
		return this->handle_ == rhs.handle_;
	}
	bool operator<(const StatisticObject& rhs) const {
		return this->handle_ < rhs.handle_;
	}
	std::size_t hash()  const;
	uint64      toRep() const;
	static StatisticObject fromRep(uint64);
private:
	struct I {
		typedef const void* ObjPtr;
		explicit I(Type t) : type(t) {}
		Type type;
	};
	struct V : I {
		V(double(*v)(ObjPtr)) : I(Potassco::Statistics_t::Value), value(v) {}
		double(*value)(ObjPtr);
	};
	struct A : I {
		A(uint32(*sz)(ObjPtr), StatisticObject(*a)(ObjPtr, uint32)) : I(Potassco::Statistics_t::Array), size(sz), at(a) {}
		uint32(*size)(ObjPtr);
		StatisticObject(*at)(ObjPtr, uint32);
	};
	struct M : I {
		M(uint32(*sz)(ObjPtr), StatisticObject(*a)(ObjPtr, const char*), const char* (*k)(ObjPtr, uint32)) : I(Potassco::Statistics_t::Map), size(sz), at(a), key(k) {}
		uint32(*size)(ObjPtr);
		StatisticObject(*at)(ObjPtr, const char*);
		const char* (*key)(ObjPtr, uint32);
	};
	static uint32 registerType(const I* vtab) {
		types_.push_back(vtab);
		return static_cast<uint32>(types_.size() - 1);
	}
	template <class T>
	static double _getValue(const T* v) { return static_cast<double>(*v); }
	template <class T, double(*f)(const T*)>
	static uint32 registerValue();
	template <class T>
	static uint32 registerMap();
	template <class T>
	static uint32 registerArray();
	StatisticObject(const void* obj, uint32 type);
	
	typedef PodVector<const I*>::type RegVec;
	const void* self() const;
	const I*    tid()  const;
	static RegVec types_;
	uint64 handle_;
};

template <class T>
uint32 StatisticObject::registerArray() {
	static struct Array_T : A {
		Array_T() : A(&Array_T::size, &Array_T::at) {}
		static uint32          size(ObjPtr obj)         { return static_cast<const T*>(obj)->size(); }
		static StatisticObject at(ObjPtr obj, uint32 i) { return static_cast<const T*>(obj)->at(i); }
	} vtab_s;
	static uint32 id = registerType(&vtab_s);
	return id;
}
template <class T>
uint32 StatisticObject::registerMap() {
	static struct Map_T : M {
		Map_T() : M(&Map_T::size, &Map_T::at, &Map_T::key) {}
		static inline const T* cast(ObjPtr obj) { return static_cast<const T*>(obj); }
		static uint32          size(ObjPtr obj) { return cast(obj)->size(); }
		static StatisticObject at(ObjPtr obj, const char* k) { return cast(obj)->at(k); }
		static const char*     key(ObjPtr obj, uint32 i) { return cast(obj)->key(i); }
	} vtab_s;
	static uint32 id = registerType(&vtab_s);
	return id;
}

template <class T, double(*f)(const T*)>
uint32 StatisticObject::registerValue() {
	static struct Value_T : V {
		Value_T() : V(&Value_T::value) {}
		static double value(ObjPtr obj) { return f(static_cast<const T*>(obj)); }
	} vtab_s;
	static uint32 id = StatisticObject::registerType(&vtab_s);
	return id;
}

class StatsMap {
public:
	// StatisticObject
	uint32           size()              const { return keys_.size(); }
	const char*      key(uint32 i)       const { return keys_.at(i).first; }
	StatisticObject  at(const char* k)   const;
	// Own interface
	const StatisticObject* find(const char* k) const;
	bool                   add(const char* k, const StatisticObject&);
	StatisticObject        toStats() const { return StatisticObject::map(this); }
private:
	typedef PodVector<std::pair<const char*, StatisticObject> >::type MapType;
	MapType keys_;
};
template <class T, Potassco::Statistics_t::E ElemType = Potassco::Statistics_t::Map>
class StatsVec : private PodVector<T*>::type {
public:
	StatsVec() : own_(true) {}
	~StatsVec() { 
		if (own_) { for (iterator it = this->begin(), end = this->end(); it != end; ++it) { delete *it; } }
	}
	typedef typename PodVector<T*>::type base_type;
	typedef typename base_type::const_iterator const_iterator;
	typedef typename base_type::iterator iterator;
	using base_type::size;
	using base_type::operator[];
	using base_type::begin;
	using base_type::end;
	void growTo(uint32 newSize) { if (newSize > size()) this->resize(newSize); }
	void reset() { for (iterator it = this->begin(), end = this->end(); it != end; ++it) { (*it)->reset(); } }
	StatisticObject at(uint32 i) const { return get_(this->base_type::at(i), bk_lib::detail::int2type<ElemType>()); }
	StatisticObject toStats()    const { return StatisticObject::array(this); }
	void acquire() { own_ = true; }
	void release() { own_ = false; }
private:
	static StatisticObject get_(const T* ptr, bk_lib::detail::int2type<Potassco::Statistics_t::Map>)   { return StatisticObject::map(ptr); }
	static StatisticObject get_(const T* ptr, bk_lib::detail::int2type<Potassco::Statistics_t::Array>) { return StatisticObject::array(ptr); }
	static StatisticObject get_(const T* ptr, bk_lib::detail::int2type<Potassco::Statistics_t::Value>) { return StatisticObject::value(ptr); }
	StatsVec(const StatsVec&);
	StatsVec& operator=(const StatsVec&);
	bool own_;
};

class ClaspStatistics : public Potassco::AbstractStatistics {
public:
	typedef Potassco::Statistics_t Type;
	ClaspStatistics();
	~ClaspStatistics();
	// Base interface
	virtual Key_t       root()          const;
	virtual Type        type(Key_t key) const;
	virtual size_t      size(Key_t key) const;
	virtual Key_t       at(Key_t arrK, size_t index) const;
	virtual const char* key(Key_t mapK, size_t i) const;
	virtual Key_t       get(Key_t mapK, const char* key) const;
	virtual double      value(Key_t key) const;

	// Register interface
	Key_t setRoot(const StatisticObject&);
	bool  removeStat(const StatisticObject&, bool recurse);
	bool  removeStat(Key_t k, bool recurse);
	void  update();
	StatisticObject findObject(Key_t root, const char* path, Key_t* track = 0) const;
	StatisticObject getObject(Key_t k) const;
private:
	ClaspStatistics(const ClaspStatistics&);
	ClaspStatistics& operator=(const ClaspStatistics&);
	Key_t root_;
	struct Impl;
	Impl*  impl_;
};

struct SolverStats;
struct JumpStats;
struct ExtendedStats;
struct ProblemStats;

//! Interface for printing statistics.
class StatsVisitor {
public:
	enum Operation { Enter, Leave } ;
	virtual ~StatsVisitor();
	// compound
	virtual bool visitGenerator(Operation op); // default: return true
	virtual bool visitThreads(Operation op);   // default: return true
	virtual bool visitTester(Operation op);    // default: return true
	virtual bool visitHccs(Operation op);      // default: return true
	
	// leafs
	virtual void visitThread(uint32, const SolverStats& stats);
	virtual void visitHcc(uint32, const ProblemStats& p, const SolverStats& s);
	virtual void visitLogicProgramStats(const Asp::LpStats& stats) = 0;
	virtual void visitProblemStats(const ProblemStats& stats) = 0;
	virtual void visitSolverStats(const SolverStats& stats) = 0;
};

}
#endif
