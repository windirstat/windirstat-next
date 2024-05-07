// CsvLoader.cpp
//
// WinDirStat - Directory Statistics
// Copyright (C) 2024 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "stdafx.h"
#include "langs.h"
#include "Item.h"
#include "Localization.h"
#include "CsvLoader.h"
#include "Constants.h"

#include <fstream>
#include <string>
#include <stack>
#include <sstream>
#include <unordered_map>
#include <format>
#include <chrono>
#include <array>
#include <ranges>

enum
{
    FIELD_NAME,
    FIELD_FILES,
    FIELDS_FOLDERS,
    FIELD_SIZE_LOGICAL,
    FIELD_SIZE_PHYSICAL,
    FIELD_ATTRIBUTES,
    FIELD_LASTCHANGE,
    FIELD_ATTRIBUTES_WDS,
    FIELD_OWNER,
    FIELD_COUNT
};

std::array<CHAR, FIELD_COUNT> orderMap{};
static void ParseHeaderLine(const std::vector<std::wstring>& header)
{
    orderMap.fill(-1);
    std::unordered_map<std::wstring, DWORD> resMap =
    {
        { Localization::Lookup(IDS_COL_NAME), FIELD_NAME},
        { Localization::Lookup(IDS_COL_FILES), FIELD_FILES },
        { Localization::Lookup(IDS_COL_FOLDERS), FIELDS_FOLDERS },
        { Localization::Lookup(IDS_COL_SIZE_LOGICAL), FIELD_SIZE_LOGICAL },
        { Localization::Lookup(IDS_COL_SIZE_PHYSICAL), FIELD_SIZE_PHYSICAL },
        { Localization::Lookup(IDS_COL_ATTRIBUTES), FIELD_ATTRIBUTES },
        { Localization::Lookup(IDS_COL_LASTCHANGE), FIELD_LASTCHANGE },
        { (Localization::Lookup(IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES)), FIELD_ATTRIBUTES_WDS },
        { Localization::Lookup(IDS_COL_OWNER), FIELD_OWNER }
    };

    for (std::vector<std::wstring>::size_type c = 0; c < header.size(); c++)
    {
        if (resMap.contains(header.at(c))) orderMap[resMap[header.at(c)]] = static_cast<BYTE>(c);
    }
}

static std::chrono::file_clock::time_point ToTimePoint(const FILETIME& ft)
{
    const std::chrono::file_clock::duration d{ (static_cast<int64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime };
    return std::chrono::file_clock::time_point { d };
}

static FILETIME FromTimeString(const std::wstring & s)
{
    // Parse date string
    std::wistringstream in{ s };
    std::chrono::file_clock::time_point tp;
    in >> std::chrono::parse(L"%Y-%m-%dT%H:%M:%S%Z", tp);

    // Adjust time divisor to 100ns 
    const auto tmp = std::chrono::duration_cast<std::chrono::duration<int64_t,
                                                                      std::ratio_multiply<std::hecto, std::nano>>>(tp.time_since_epoch()).count();

    // Load into file time structure
    FILETIME ft{};
    ft.dwLowDateTime = static_cast<ULONG>(tmp);
    ft.dwHighDateTime = tmp >> 32;
    return ft;
}

static std::string QuoteAndConvert(const std::wstring& inc)
{
    const int sz = WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, inc.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out = "\"";
    out.resize(static_cast<size_t>(sz) + 1);
    WideCharToMultiByte(CP_UTF8, 0, inc.data(), -1, &out[1], sz, nullptr, nullptr);
    out[sz] = '"';
    return out;
}

CItem* LoadResults(const std::wstring & path)
{
    std::ifstream reader(path);
    if (!reader.is_open()) return nullptr;

    CItem* newroot = nullptr;
    std::string linebuf;
    std::wstring line;
    std::unordered_map<const std::wstring, CItem*, std::hash<std::wstring>> parentMap;

    bool headerProcessed = false;
    while (std::getline(reader, linebuf))
    {
        if (linebuf.empty()) continue;
        std::vector<std::wstring> fields;

        // Convert to wide string
        line.resize(linebuf.size() + 1);
        const int size = MultiByteToWideChar(CP_UTF8, 0, linebuf.c_str(), -1,
            line.data(), static_cast<int>(line.size()));
        line.resize(size);

        // Parse all fields
        for (size_t pos = 0; pos < line.length(); pos++)
        {
            const size_t comma = line.find(L',', pos);
            size_t end = comma == std::wstring::npos ? line.length() : comma;

            // Adjust for quoted lines
            bool quoted = line.at(pos) == '"';
            if (quoted)
            {
                pos = pos + 1;
                end = line.find('"', pos);
                if (end == std::wstring::npos) return nullptr;
            }

            // Extra value(s)
            fields.emplace_back(line, pos, end - pos);
            pos = end + (quoted ? 1 : 0);
        }

        // Process the header if not done already
        if (!headerProcessed)
        {
            ParseHeaderLine(fields);
            headerProcessed = true;

            // Validate all necessary fields are present
            for (auto i = 0; i < static_cast<char>(orderMap.size()); i++)
            {
                if (i != FIELD_OWNER && orderMap[i] == -1) return nullptr;
            }
            continue;
        }

        // Decode item type
        const ITEMTYPE type = static_cast<ITEMTYPE>(wcstoul(fields[orderMap[FIELD_ATTRIBUTES_WDS]].c_str(), nullptr, 16));

        // Determine how to store the path if it was the root or not
        const bool isRoot = (type & ITF_ROOTITEM);
        const bool isInRoot = (type & IT_DRIVE) || (type & IT_UNKNOWN) || (type & IT_FREESPACE);
        const bool useFullPath = isRoot || isInRoot;
        const std::wstring mapPath = fields[orderMap[FIELD_NAME]];
        LPWSTR lookupPath = fields[orderMap[FIELD_NAME]].data();
        LPWSTR displayName = useFullPath ? lookupPath : wcsrchr(lookupPath, L'\\');
        if (!useFullPath && displayName != nullptr)
        {
            displayName[0] = wds::chrNull;
            displayName = &displayName[1];
        }

        // Create the tree item
        CItem* newitem = new CItem(
            type,
            displayName,
            FromTimeString(fields[orderMap[FIELD_LASTCHANGE]]),
            _wcstoui64(fields[orderMap[FIELD_SIZE_PHYSICAL]].c_str(), nullptr, 10),
            _wcstoui64(fields[orderMap[FIELD_SIZE_LOGICAL]].c_str(), nullptr, 10),
            wcstoul(fields[orderMap[FIELD_ATTRIBUTES]].c_str(), nullptr, 16),
            wcstoul(fields[orderMap[FIELD_FILES]].c_str(), nullptr, 10),
            wcstoul(fields[orderMap[FIELDS_FOLDERS]].c_str(), nullptr, 10));


        if (isRoot)
        {
            newroot = newitem;
        }
        else if (isInRoot)
        {
            newroot->AddChild(newitem, true);
        }
        else if (auto parent = parentMap.find(lookupPath); parent != parentMap.end())
        {
            parent->second->AddChild(newitem, true);
        }
        else ASSERT(FALSE);

        if (!newitem->TmiIsLeaf() && newitem->GetItemsCount() > 0)
        {
            parentMap[mapPath] = newitem;

            // Special case: also add mapping for drive without backslash
            if (newitem->IsType(IT_DRIVE)) parentMap[mapPath.substr(0, 2)] = newitem;
        }
    }

    // Sort all parent items
    for (const auto& val : parentMap | std::views::values)
    {
        val->SortItemsBySizePhysical();
    }

    return newroot;
}

bool SaveResults(const std::wstring& path, CItem * item)
{
    // Output header line to file
    std::ofstream outf;
    outf.open(path, std::ios::binary);

    // Determine columns
    std::vector<std::wstring> cols =
    {
        Localization::Lookup(IDS_COL_NAME),
        Localization::Lookup(IDS_COL_FILES),
        Localization::Lookup(IDS_COL_FOLDERS),
        Localization::Lookup(IDS_COL_SIZE_LOGICAL),
        Localization::Lookup(IDS_COL_SIZE_PHYSICAL),
        Localization::Lookup(IDS_COL_ATTRIBUTES),
        Localization::Lookup(IDS_COL_LASTCHANGE),
        Localization::Lookup(IDS_APP_TITLE) + L" " + Localization::Lookup(IDS_COL_ATTRIBUTES)
    };
    if (COptions::ShowColumnOwner)
    {
        cols.push_back(Localization::Lookup(IDS_COL_OWNER));
    }

    // Output columns to file
    for (unsigned int i = 0; i < cols.size(); i++)
    {
        outf << QuoteAndConvert(cols[i]) << ((i < cols.size() - 1) ? "," : "");
    }

    // Output all items to file
    outf << "\r\n";
    std::stack<CItem*> queue({ item });
    while (!queue.empty())
    {
        // Grab item from queue
        const CItem* qitem = queue.top();
        queue.pop();

        // Output primary columns
        const bool nonPathItem = qitem->IsType(IT_MYCOMPUTER | IT_UNKNOWN | IT_FREESPACE);
        outf << std::format("{},{},{},{},{},0x{:08X},{:%FT%TZ},0x{:04X}",
            QuoteAndConvert(nonPathItem ? qitem->GetName() : qitem->GetPath()),
            qitem->GetFilesCount(),
            qitem->GetFoldersCount(),
            qitem->GetSizeLogical(),
            qitem->GetSizePhysical(),
            qitem->GetAttributes(),
            ToTimePoint(qitem->GetLastChange()),
            static_cast<unsigned short>(qitem->GetRawType()));

        // Output additional columns
        if (COptions::ShowColumnOwner)
        {
            outf << "," << QuoteAndConvert(qitem->GetOwner(true));
        }

        // Finalize lines
        outf << "\r\n";

        // Descend into childitems
        if (qitem->IsType(IT_FILE)) continue;
        for (const auto& child : qitem->GetChildren())
        {
            queue.push(child);
        }
    }

    outf.close();
    return true;
}
