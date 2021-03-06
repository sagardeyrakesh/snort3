//--------------------------------------------------------------------------
// Copyright (C) 2016-2016 Cisco and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------

// ips_sd_pattern.cc author Victor Roemer <viroemer@cisco.com>

// FIXIT-H: Use Hyperscan

#include <string.h>
#include <assert.h>
#include <string>

#include "framework/cursor.h"
#include "framework/ips_option.h"
#include "framework/module.h"
#include "detection/detection_defines.h"
#include "detection/pattern_match_data.h"
#include "hash/sfhashfcn.h"
#include "main/snort_config.h"
#include "main/thread.h"
#include "parser/parser.h"
#include "profiler/profiler.h"
#include "sd_pattern_match.h"

#define s_name "sd_pattern"
#define s_help "rule option for detecting sensitive data"

struct SdPatternConfig
{
    std::string pii;
    unsigned threshold = 1;
};

static THREAD_LOCAL ProfileStats sd_pattern_perf_stats;

//-------------------------------------------------------------------------
// option
//-------------------------------------------------------------------------

class SdPatternOption : public IpsOption
{
public:
    SdPatternOption(const SdPatternConfig&);
    ~SdPatternOption();

    uint32_t hash() const override;
    bool operator==(const IpsOption&) const override;

    int eval(Cursor&, Packet* p) override;

private:
    unsigned SdSearch(const uint8_t* buf, uint16_t buflen);

    const SdPatternConfig config;
    SdOptionData* sd_data;
    SdContext* sd_context;
};

SdPatternOption::SdPatternOption(const SdPatternConfig& c) :
    IpsOption(s_name, RULE_OPTION_TYPE_BUFFER_USE), config(c)
{
    sd_data = new SdOptionData(config.pii, config.threshold);
    sd_context = new SdContext(sd_data);
}

SdPatternOption::~SdPatternOption()
{
    delete(sd_context);
}

uint32_t SdPatternOption::hash() const
{
    uint32_t a = 0, b = 0, c = 0;
    mix_str(a, b, c, config.pii.c_str());
    mix_str(a, b, c, get_name());
    finalize(a, b, c);
    return c;
}

bool SdPatternOption::operator==(const IpsOption& ips) const
{
    if ( !IpsOption::operator==(ips) )
        return false;

    const SdPatternOption& rhs = static_cast<const SdPatternOption&>(ips);

    if ( config.pii == rhs.config.pii
        and config.threshold == rhs.config.threshold )
        return true;

    return false;
}

unsigned SdPatternOption::SdSearch(const uint8_t* buf, uint16_t buflen)
{
    unsigned count = 0;
    const uint8_t* end = buf + buflen;

    SdSessionData ssn;
    memset(&ssn, 0, sizeof(ssn));

    while (buf < end && count < config.threshold)
    {
        SdTreeNode* matched_node;
        uint16_t match_len = 0;

        matched_node = FindPii(sd_context->head_node, buf, &match_len, buflen, &ssn);
        if ( matched_node )
        {
            buf += match_len;
            buflen -= match_len;
            count++;
        }
        else
        {
            buf++;
            buflen--;
        }
    }

    return count;
}

int SdPatternOption::eval(Cursor& c, Packet*)
{
    Profile profile(sd_pattern_perf_stats);

    unsigned matches = SdSearch(c.start(), c.length());
    if ( matches >= config.threshold )
        return DETECTION_OPTION_MATCH;

    return DETECTION_OPTION_NO_MATCH;
}

//-------------------------------------------------------------------------
// module
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "~pattern", Parameter::PT_STRING, nullptr, nullptr,
      "The pattern to search for" },

    { "threshold", Parameter::PT_INT, "1", nullptr,
      "number of matches before alerting" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

class SdPatternModule : public Module
{
public:
    SdPatternModule() : Module(s_name, s_help, s_params) { }

    bool begin(const char*, int, SnortConfig*) override;
    bool set(const char*, Value& v, SnortConfig*) override;

    ProfileStats* get_profile() const override
    { return &sd_pattern_perf_stats; }

    void get_data(SdPatternConfig& c)
    {
        c = config;
    }

private:
    SdPatternConfig config;
};

bool SdPatternModule::begin(const char*, int, SnortConfig*)
{
    config = SdPatternConfig();
    return true;
}

bool SdPatternModule::set(const char*, Value& v, SnortConfig*)
{
    if ( v.is("~pattern") )
    {
        config.pii = v.get_string();
        // remove quotes
        config.pii.erase(0, 1);
        config.pii.erase(config.pii.length()-1, 1);
    }
    else if ( v.is("threshold") )
        config.threshold = v.get_long();
    else
        return false;

    return true;
}

//-------------------------------------------------------------------------
// api methods
//-------------------------------------------------------------------------

static Module* mod_ctor()
{
    return new SdPatternModule;
}

static void mod_dtor(Module* p)
{
    delete p;
}

static IpsOption* sd_pattern_ctor(Module* m, OptTreeNode*)
{
    SdPatternModule* mod = (SdPatternModule*)m;
    SdPatternConfig c;
    mod->get_data(c);
    return new SdPatternOption(c);
}

static void sd_pattern_dtor(IpsOption* p)
{ delete p; }

static const IpsApi sd_pattern_api =
{
    {
        PT_IPS_OPTION,
        sizeof(IpsApi),
        IPSAPI_VERSION,
        0,
        API_RESERVED,
        API_OPTIONS,
        s_name,
        s_help,
        mod_ctor,
        mod_dtor
    },
    OPT_TYPE_DETECTION,
    0, 0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    sd_pattern_ctor,
    sd_pattern_dtor,
    nullptr
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &sd_pattern_api.base,
    nullptr
};
#else
const BaseApi* ips_sd_pattern = &sd_pattern_api.base;
#endif

