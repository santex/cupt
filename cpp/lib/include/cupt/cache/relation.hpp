/**************************************************************************
*   Copyright (C) 2010 by Eugene V. Lyubimkin                             *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License                  *
*   (version 3 or above) as published by the Free Software Foundation.    *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU GPL                        *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA               *
**************************************************************************/
#ifndef CUPT_CACHE_RELATION_SEEN
#define CUPT_CACHE_RELATION_SEEN

#include <cupt/common.hpp>

namespace cupt {
namespace cache {

struct Relation
{
 private:
	bool __parse_versioned_info(string::const_iterator, string::const_iterator);
	void __init(string::const_iterator, string::const_iterator);
 public:
	struct Types
	{
		enum Type { Less, Equal, More, LessOrEqual, MoreOrEqual, None };
		static const string strings[];
	};
	string packageName;
	Types::Type relationType;
	string versionString;

	Relation(const string&);
	Relation(pair< string::const_iterator, string::const_iterator >);
	virtual ~Relation();
	string toString() const;
	bool isSatisfiedBy(const string& otherVersionString) const;
	bool operator==(const Relation& other) const;
};

struct ArchitecturedRelation: public Relation
{
 private:
	void __init(string::const_iterator, string::const_iterator);
 public:
	vector< string > architectureFilters;

	ArchitecturedRelation(const string&);
	ArchitecturedRelation(pair< string::const_iterator, string::const_iterator >);
	string toString() const;
};

struct RelationExpression: public vector< Relation >
{
 private:
	void __init(string::const_iterator, string::const_iterator);
 public:
	string toString() const;
	string getHashString() const;
	RelationExpression();
	RelationExpression(const string&);
	RelationExpression(pair< string::const_iterator, string::const_iterator >);
	virtual ~RelationExpression();
};

struct ArchitecturedRelationExpression: public vector< ArchitecturedRelation >
{
 private:
	void __init(string::const_iterator, string::const_iterator);
 public:
	string toString() const;
	string getHashString() const;
	ArchitecturedRelationExpression();
	ArchitecturedRelationExpression(const string&);
	ArchitecturedRelationExpression(pair< string::const_iterator, string::const_iterator >);
	virtual ~ArchitecturedRelationExpression();
};

struct RelationLine: public vector< RelationExpression >
{
 private:
	void __init(string::const_iterator, string::const_iterator);
 public:
	string toString() const;
	RelationLine();
	RelationLine(const string&);
	RelationLine(pair< string::const_iterator, string::const_iterator >);
	virtual ~RelationLine();
};

struct ArchitecturedRelationLine: public vector< ArchitecturedRelationExpression >
{
 private:
	void __init(string::const_iterator, string::const_iterator);
 public:
	string toString() const;
	ArchitecturedRelationLine();
	ArchitecturedRelationLine(const string&);
	ArchitecturedRelationLine(pair< string::const_iterator, string::const_iterator >);
	virtual ~ArchitecturedRelationLine();
};

RelationLine unarchitectureRelationLine(const ArchitecturedRelationLine& source,
		const string& currentArchitecture);

}
}

#endif

