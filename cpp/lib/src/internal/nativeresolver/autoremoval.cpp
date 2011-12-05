/**************************************************************************
*   Copyright (C) 2011 by Eugene V. Lyubimkin                             *
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
#include <common/regex.hpp>

#include <cupt/config.hpp>
#include <cupt/cache.hpp>
#include <cupt/cache/binarypackage.hpp>
#include <cupt/cache/binaryversion.hpp>

#include <internal/nativeresolver/autoremoval.hpp>

namespace cupt {
namespace internal {

class AutoRemovalImpl
{
	mutable smatch __m;
	vector< sregex > __never_regexes;
	bool __can_autoremove;

	bool __never_autoremove(const string& packageName) const
	{
		FORIT(regexIt, __never_regexes)
		{
			if (regex_match(packageName, __m, *regexIt))
			{
				return true;
			}
		}
		return false;
	}
 public:
	AutoRemovalImpl(const Config& config)
	{
		__can_autoremove = config.getBool("cupt::resolver::auto-remove");

		auto neverAutoRemoveRegexStrings = config.getList("apt::neverautoremove");
		FORIT(regexStringIt, neverAutoRemoveRegexStrings)
		{
			try
			{
				__never_regexes.push_back(sregex::compile(*regexStringIt));
			}
			catch (regex_error&)
			{
				fatal2("invalid regular expression '%s'", *regexStringIt);
			}
		}
	}

	bool isAllowed(const Cache& cache, const string& packageName) const
	{
		if (!__can_autoremove)
		{
			return false;
		}
		if (!cache.isAutomaticallyInstalled(packageName))
		{
			return false;
		}
		{ // check for essentialness
			auto package = cache.getBinaryPackage(packageName);
			auto installedVersion = package->getInstalledVersion();
			if (installedVersion && installedVersion->essential)
			{
				return false;
			}
		}
		if (__never_autoremove(packageName))
		{
			return false;
		}

		return true;
	}
};

AutoRemoval::AutoRemoval(const Config& config)
{
	__impl = new AutoRemovalImpl(config);
}

AutoRemoval::~AutoRemoval()
{
	delete __impl;
}

bool AutoRemoval::isAllowed(const Cache& cache, const string& packageName) const
{
	return __impl->isAllowed(cache, packageName);
}

}
}

