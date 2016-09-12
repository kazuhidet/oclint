#include "oclint/Analytics.h"

#ifndef COUNTLY_ANALYTICS

void oclint::Analytics::send() {}
void oclint::Analytics::ruleConfiguration(std::string key, std::string value) {}
void oclint::Analytics::languageOption(clang::LangOptions langOptions) {}

#else

#include <map>
#include <sstream>

#include <countly/Countly.h>
#include <countly/Device.h>
#include <countly/Metrics.h>

#include "oclint/Options.h"
#include "oclint/RuleSet.h"
#include "oclint/Version.h"

static std::map<std::string, std::string>* _ruleConfigurations = nullptr;
static std::map<std::string, int>* _languageCounter = nullptr;

static std::string hashedWorkingPath()
{
  std::string workingPath = oclint::option::workingPath();
  if (workingPath == "")
  {
    return "unknown-project-id";
  }

  std::ostringstream hashed;
  hashed << std::hash<std::string>{}(workingPath);
  return hashed.str();
}

static void sendEnvironment(countly::Countly &cntly)
{
  std::map<std::string, std::string> segments;
  // anonymous identifiers to deduplicate events
  segments["device_id"] = countly::Device::id();
    // https://github.com/ryuichis/countly-cpp/blob/master/lib/Device.cpp
    // this is a hashed string of this network card's mac address
  segments["project_id"] = hashedWorkingPath();

  segments["version"] = oclint::Version::identifier();
  segments["bin_path"] = oclint::option::binPath();
  segments["os"] = countly::Metrics::os() + "/" + countly::Metrics::osVersion();

  cntly.recordEvent("DevEnvironment", segments);
}

static void sendConfiguration(countly::Countly &cntly)
{
  std::map<std::string, std::string> segments;
  segments["report_type"] = oclint::option::reportType();
  segments["global_analysis"] =
    oclint::option::enableGlobalAnalysis() ? "1" : "0";
  segments["clang_checker"] =
    oclint::option::enableClangChecker() ? "1" : "0";
  segments["allow_duplications"] =
    oclint::option::allowDuplicatedViolations() ? "1" : "0";
  segments["p1_violations"] = std::to_string(oclint::option::maxP1());
  segments["p2_violations"] = std::to_string(oclint::option::maxP2());
  segments["p3_violations"] = std::to_string(oclint::option::maxP3());

  cntly.recordEvent("DevConfiguration", segments);
}

static void sendLoadedRules(countly::Countly &cntly)
{
  std::map<std::string, std::string> segments;

  std::vector<std::string> rules =
    oclint::option::rulesetFilter().filteredRuleNames();
  for (int ruleIdx = 0, numRules = oclint::RuleSet::numberOfRules();
       ruleIdx < numRules;
       ruleIdx++)
  {
    oclint::RuleBase *rule = oclint::RuleSet::getRuleAtIndex(ruleIdx);
    std::string ruleId = rule->identifier();
    bool enabled = std::find(rules.begin(), rules.end(), ruleId) != rules.end();
    segments[ruleId] = enabled ? "1" : "0";
  }

  cntly.recordEvent("DevLoadedRules", segments);
}

static void sendRuleConfiguration(countly::Countly &cntly)
{
  if (_ruleConfigurations && _ruleConfigurations->size() > 0)
  {
    cntly.recordEvent("DevRuleConfigurations", *_ruleConfigurations);
  }
}

static void sendLanguageCount(countly::Countly &cntly)
{
  if (_languageCounter)
  {
    std::map<std::string, std::string> segments;
    for (auto &lang : *_languageCounter)
    {
      if (lang.second > 0)
      {
        segments[lang.first] = std::to_string(lang.second);
      }
    }
    if (segments.size() > 0)
    {
      cntly.recordEvent("DevLanguageCount", segments);
    }
  }
  else {
  }
}

void oclint::Analytics::send()
{
  countly::Countly cntly;
  cntly.start("countly.ryuichisaito.com",
    "873c792b2ead515f27f0ccba01a976ae9a4cc425");

  sendEnvironment(cntly);
  sendConfiguration(cntly);
  sendLoadedRules(cntly);
  sendRuleConfiguration(cntly);
  sendLanguageCount(cntly);

  cntly.suspend();
}

void oclint::Analytics::ruleConfiguration(std::string key, std::string value)
{
  if (_ruleConfigurations == nullptr)
  {
      _ruleConfigurations = new std::map<std::string, std::string>();
  }

  _ruleConfigurations->operator[](key) = value;
}

void oclint::Analytics::languageOption(clang::LangOptions langOpts)
{
  if (_languageCounter == nullptr)
  {
      _languageCounter = new std::map<std::string, int>();
      _languageCounter->operator[]("c") = 0;
      _languageCounter->operator[]("cpp") = 0;
      _languageCounter->operator[]("objc") = 0;
  }
  if (langOpts.ObjC1)
  {
      int objc = _languageCounter->operator[]("objc");
      _languageCounter->operator[]("objc") = objc + 1;
  }
  if (langOpts.CPlusPlus)
  {
      int cpp = _languageCounter->operator[]("cpp");
      _languageCounter->operator[]("cpp") = cpp + 1;
  }
  if (langOpts.C99)
  {
      int c99 = _languageCounter->operator[]("c");
      _languageCounter->operator[]("c") = c99 + 1;
  }
}

#endif
