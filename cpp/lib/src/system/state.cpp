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
#include <map>

#include <cupt/file.hpp>
#include <cupt/system/state.hpp>
#include <cupt/cache/releaseinfo.hpp>
#include <cupt/cache.hpp>
#include <cupt/cache/version.hpp>
#include <cupt/cache/package.hpp>
#include <cupt/config.hpp>
#include <cupt/regex.hpp>

#include <internal/cacheimpl.hpp>

namespace cupt {

namespace internal {

using std::map;

typedef system::State::InstalledRecord InstalledRecord;

struct StateData
{
	shared_ptr< const Config > config;
	internal::CacheImpl* cacheImpl;
	map< string, shared_ptr< const InstalledRecord > > installedInfo;

	void parseDpkgStatus();
};

void StateData::parseDpkgStatus()
{
	string path = config->getString("dir::state::status");
	string openError;
	shared_ptr< File > file(new File(path, "r", openError));
	if (!openError.empty())
	{
		fatal("unable to open dpkg status file '%s': %s", path.c_str(), openError.c_str());
	}

	/*
	 Status lines are similar to apt Packages ones, with two differences:
	 1) 'Status' field: see header for possible values
	 2) purged packages contain only 'Package', 'Status', 'Priority'
	    and 'Section' fields.
	*/

	// filling release info
	shared_ptr< ReleaseInfo > releaseInfo(new ReleaseInfo);
	releaseInfo->archive = "installed";
	releaseInfo->codename = "now";
	releaseInfo->vendor = "dpkg";
	releaseInfo->verified = false;
	releaseInfo->notAutomatic = false;

	cacheImpl->releaseInfoAndFileStorage.push_back(make_pair(releaseInfo, file));
	internal::CacheImpl::PrePackageRecord prePackageRecord;
	prePackageRecord.releaseInfoAndFile = &*(cacheImpl->releaseInfoAndFileStorage.rbegin());

	cacheImpl->binaryReleaseData.push_back(releaseInfo);
	auto preBinaryPackages = &(cacheImpl->preBinaryPackages);

	try
	{
		string block;
		smatch m;

		auto lineAccepter = [](const char* buf, size_t size) -> bool
		{
#define TAG_ACCEPT(str) \
			if (size > sizeof(str) - 1 && !memcmp(str, buf, sizeof(str) - 1)) \
			{ \
				return true; \
			} \
			else

			TAG_ACCEPT("Package: ")
			TAG_ACCEPT("Status: ")
			TAG_ACCEPT("Version: ")
			TAG_ACCEPT("Provides: ")
			return false;
#undef TAG_ACCEPT
		};
		while ((prePackageRecord.offset = file->tell()), ! file->getBlock(block, lineAccepter).eof())
		{
			shared_ptr< InstalledRecord > installedRecord(new InstalledRecord);

			static sregex installedRegex(sregex::compile("^Package: (.*?)$.*?^Status: (.*?)$.*?^Version: .*?$(?:.*?^Provides: (.*?)$)?",
						regex_constants::optimize));

			// we don't check package name for correctness - even if it's incorrent, we can't decline installed packages :(

			if (!regex_search(block, m, installedRegex))
			{
				continue;
			}
			string packageName = m[1];

			vector< string > statusStrings = split(' ', m[2]);
			if (statusStrings.size() != 3)
			{
				fatal("malformed 'Status' line (for package '%s')", packageName.c_str());
			}

			{ // want
				const string& want = statusStrings[0];
#define CHECK_WANT(str, value) if (want == str) { installedRecord->want = InstalledRecord::Want:: value; } else
				CHECK_WANT("install", Install)
				CHECK_WANT("deinstall", Deinstall)
				CHECK_WANT("unknown", Unknown)
				CHECK_WANT("hold", Hold)
				CHECK_WANT("purge", Purge)
				{ // else
					fatal("malformed 'desired' status indicator (for package '%s')", packageName.c_str());
				}
#undef CHECK_WANT
			}

			{ // flag
				const string& flag = statusStrings[1];
#define CHECK_FLAG(str, value) if (flag == str) { installedRecord->flag = InstalledRecord::Flag:: value; } else
				CHECK_FLAG("ok", Ok)
				CHECK_FLAG("reinstreq", Reinstreq)
				CHECK_FLAG("hold", Hold)
				CHECK_FLAG("hold-reinstreq", HoldAndReinstreq)
				{ // else
					fatal("malformed 'error' status indicator (for package '%s')", packageName.c_str());
				}
#undef CHECK_FLAG
			}

			{ // status
				const string& status = statusStrings[2];
#define CHECK_STATUS(str, value) if (status == str) { installedRecord->status = InstalledRecord::Status:: value; } else
				CHECK_STATUS("installed", Installed)
				CHECK_STATUS("not-installed", NotInstalled)
				CHECK_STATUS("config-files", ConfigFiles)
				CHECK_STATUS("unpacked", Unpacked)
				CHECK_STATUS("half-configured", HalfConfigured)
				CHECK_STATUS("half-installed", HalfInstalled)
				CHECK_STATUS("post-inst-failed", PostInstFailed)
				CHECK_STATUS("removal-failed", RemovalFailed)
				{ // else
					static sregex triggerRegex(sregex::compile("^trigger", regex_constants::optimize));
					if (regex_search(status, m, triggerRegex))
					{
						fatal("some dpkg triggers are not processed, please run 'dpkg --triggers-only -a' as root");
					}
					else
					{
						fatal("malformed 'status' status indicator (for package '%s')", packageName.c_str());
					}
				}
			}

			if (installedRecord->flag == InstalledRecord::Flag::Ok)
			{
				if (installedRecord->status != InstalledRecord::Status::NotInstalled &&
					installedRecord->status != InstalledRecord::Status::ConfigFiles)
				{
					// this conditions mean that package is installed or
					//   semi-installed, regardless it has full entry info, so
					//  add it (info) to cache

					static const vector< internal::CacheImpl::PrePackageRecord > emptyPrePackageRecords;
					auto it = preBinaryPackages->insert(
							make_pair(packageName, emptyPrePackageRecords)).first;
					it->second.push_back(prePackageRecord);

					if (m[3].matched)
					{
						const char* blockData = block.c_str();
						cacheImpl->processProvides(it->first,
								blockData + (m[3].first - block.begin()), blockData + (m[3].second - block.begin()));
					}
				}

				// add parsed info to installed_info
				installedInfo.insert(std::make_pair(packageName, installedRecord));
			}
		}
	}
	catch (exception&)
	{
		fatal("error parsing system status file '%s'", path.c_str());
	}
}

}

namespace system {

State::State(shared_ptr< const Config > config, internal::CacheImpl* cacheImpl)
	: __data(new internal::StateData)
{
	__data->config = config;
	__data->cacheImpl = cacheImpl;
	__data->parseDpkgStatus();
}

State::~State()
{
	delete __data;
}

shared_ptr< const State::InstalledRecord > State::getInstalledInfo(const string& packageName) const
{
	auto it = __data->installedInfo.find(packageName);
	if (it == __data->installedInfo.end())
	{
		return shared_ptr< const InstalledRecord >();
	}
	else
	{
		return it->second;
	}
}

vector< string > State::getInstalledPackageNames() const
{
	vector< string > result;

	FORIT(it, __data->installedInfo)
	{
		const InstalledRecord& installedRecord = *(it->second);

		if (installedRecord.status == InstalledRecord::Status::NotInstalled ||
			installedRecord.status == InstalledRecord::Status::ConfigFiles)
		{
			continue;
		}

		result.push_back(it->first);

	}

	return result;
}

const string State::InstalledRecord::Status::strings[] = {
	__("not installed"), __("unpacked"), __("half-configured"), __("half-installed"),
	__("config files"), __("postinst failed"), __("removal failed"), __("installed")
};

}
}

