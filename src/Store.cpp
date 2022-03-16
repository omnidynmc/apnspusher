/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** HAL9000, Internet Relay Chat Bot                                     **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
 **                    Daniel Robert Karrels                             **
 **                    Dynamic Networking Solutions                      **
 **                                                                      **
 ** This program is free software; you can redistribute it and/or modify **
 ** it under the terms of the GNU General Public License as published by **
 ** the Free Software Foundation; either version 1, or (at your option)  **
 ** any later version.                                                   **
 **                                                                      **
 ** This program is distributed in the hope that it will be useful,      **
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of       **
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        **
 ** GNU General Public License for more details.                         **
 **                                                                      **
 ** You should have received a copy of the GNU General Public License    **
 ** along with this program; if not, write to the Free Software          **
 ** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.            **
 **************************************************************************
 $Id: APNS.cpp,v 1.12 2003/09/05 22:23:41 omni Exp $
 **************************************************************************/

#include <string>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <new>
#include <iostream>
#include <sstream>

#include <errno.h>
#include <time.h>
#include <math.h>

#include <openframe/openframe.h>
#include <openstats/StatsClient_Interface.h>

#include "DBI.h"
#include "MemcachedController.h"
#include "Store.h"

namespace apnspusher {
  using namespace openframe::loglevel;

/**************************************************************************
 ** Store Class                                                         **
 **************************************************************************/
  const time_t Store::kDefaultReportInterval			= 3600;

  Store::Store(const thread_id_t thread_id,
               const std::string &host,
               const std::string &user,
               const std::string &pass,
               const std::string &db,
               const std::string memcached_host, const time_t expire_interval, const time_t report_interval)
        : LogObject(thread_id),
          _host(host),
          _user(user),
          _pass(pass),
          _db(db),
          _memcached_host(memcached_host),
          _expire_interval(expire_interval) {


    init_stats(_stats, true);
    init_stats(_stompstats, true);

    _stats.report_interval = report_interval;
    _stompstats.report_interval = 5;

    _last_cache_fail_at = 0;

    _dbi = NULL;
    _memcached = NULL;
    _profile = NULL;
  } // Store::Store

  Store::~Store() {
    if (_memcached) delete _memcached;
    if (_dbi) delete _dbi;
    if (_profile) delete _profile;
  } // Store::~Store

  Store &Store::init() {
    try {
      _dbi = new DBI_Apns(thread_id(), _host, _user, _pass, _db);
      _dbi->set_elogger( elogger(), elog_name() );
      _dbi->init();
    } // try
    catch(std::bad_alloc xa) {
      assert(false);
    } // catch

    _memcached = new MemcachedController(_memcached_host);
    _memcached->expire(_expire_interval);

    _profile = new openframe::Stopwatch();
    _profile->add("memcached.message", 300);
    _profile->add("memcached.register", 300);

    return *this;
  } // Store::init

  void Store::init_stats(obj_stats_t &stats, const bool startup) {
    memset(&stats.cache_message, 0, sizeof(memcache_stats_t) );
    memset(&stats.cache_register, 0, sizeof(memcache_stats_t) );

    memset(&stats.sql_register, 0, sizeof(sql_stats_t) );

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // init_stats

  void Store::onDescribeStats() {
    describe_root_stat("store.num.cache.message.hits", "store/cache/message/num hits - message", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.message.misses", "store/cache/message/num misses - message", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.message.tries", "store/cache/message/num tries - message", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.message.stored", "store/cache/message/num stored - message", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.message.hitrate", "store/cache/message/num hitrate - message", openstats::graphTypeGauge, openstats::dataTypeFloat);

    describe_root_stat("store.num.cache.register.hits", "store/cache/register/num hits - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.register.misses", "store/cache/register/num misses - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.register.tries", "store/cache/register/num tries - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.register.stored", "store/cache/register/num stored - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.cache.register.hitrate", "store/cache/register/num hitrate - register", openstats::graphTypeGauge, openstats::dataTypeFloat);

    describe_root_stat("store.num.sql.register.hits", "store/sql/register/num hits - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.register.misses", "store/sql/register/num misses - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.register.tries", "store/sql/register/num tries - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.register.inserted", "store/sql/register/num inserted - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.register.failed", "store/sql/register/num failed - register", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_root_stat("store.num.sql.register.hitrate", "store/sql/register/num hitrate - register", openstats::graphTypeGauge, openstats::dataTypeFloat);
  } // Store::onDescribeStats

  void Store::onDestroyStats() {
    destroy_stat("store.num.*");
  } // Store::onDestroyStats

  void Store::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    TLOG(LogNotice, << "Memcached{message} hits "
                    << _stats.cache_message.hits
                    << ", misses "
                    << _stats.cache_message.misses
                    << ", tries "
                    << _stats.cache_message.tries
                    << ", rate %"
                    << std::fixed << std::setprecision(2)
                    << OPENSTATS_PERCENT(_stats.cache_message.hits, _stats.cache_message.tries)
                    << " average "
                    << std::fixed << std::setprecision(4)
                    << _profile->average("memcached.message")
                    << "s"
                    << std::endl);

    TLOG(LogNotice, << "Memcached{register} hits "
                    << _stats.cache_register.hits
                    << ", misses "
                    << _stats.cache_register.misses
                    << ", tries "
                    << _stats.cache_register.tries
                    << ", rate %"
                    << std::fixed << std::setprecision(2)
                    << OPENSTATS_PERCENT(_stats.cache_register.hits, _stats.cache_register.tries)
                    << " average "
                    << std::fixed << std::setprecision(4)
                    << _profile->average("memcached.register")
                    << "s"
                    << std::endl);


    TLOG(LogNotice, << "Sql{register} hits "
                    << _stats.sql_register.hits
                    << ", misses "
                    << _stats.sql_register.misses
                    << ", tries "
                    << _stats.sql_register.tries
                    << ", rate %"
                    << std::fixed << std::setprecision(2)
                    << OPENSTATS_PERCENT(_stats.sql_register.hits, _stats.sql_register.tries)
                    << std::endl);

    init_stats(_stats);
  } // Store::try_stats

  void Store::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    // this prevents stompstats from having to lookup strings in
    // its hash tables over and over again in realtime at ~35 pps

    datapoint("store.num.sql.register.tries", _stompstats.sql_register.tries);
    datapoint("store.num.sql.register.hits", _stompstats.sql_register.hits);
    datapoint_float("store.num.sql.register.hitrate", OPENSTATS_PERCENT(_stompstats.sql_register.hits, _stompstats.sql_register.tries) );
    datapoint("store.num.sql.register.misses", _stompstats.sql_register.misses);
    datapoint("store.num.sql.register.inserted", _stompstats.sql_register.inserted);
    datapoint("store.num.sql.register.failed", _stompstats.sql_register.failed);

    datapoint("store.num.cache.message.tries", _stompstats.cache_message.tries);
    datapoint("store.num.cache.message.misses", _stompstats.cache_message.misses);
    datapoint("store.num.cache.message.hits", _stompstats.cache_message.hits);
    datapoint_float("store.num.cache.message.hitrate", OPENSTATS_PERCENT(_stompstats.cache_message.hits, _stompstats.cache_message.tries) );
    datapoint("store.num.cache.message.stored", _stompstats.cache_message.stored);

    datapoint("store.num.cache.register.tries", _stompstats.cache_register.tries);
    datapoint("store.num.cache.register.misses", _stompstats.cache_register.misses);
    datapoint("store.num.cache.register.hits", _stompstats.cache_register.hits);
    datapoint_float("store.num.cache.register.hitrate", OPENSTATS_PERCENT(_stompstats.cache_register.hits, _stompstats.cache_register.tries) );
    datapoint("store.num.cache.register.stored", _stompstats.cache_register.stored);

    init_stats(_stompstats);
  } // Store::try_stompstats()

  //
  // Memcache Apns Register
  //
  bool Store::getApnsRegisterFromMemcached(const std::string &callsign, std::string &ret) {
    MemcachedController::memcachedReturnEnum mcr;
    openframe::Stopwatch sw;

    if (!isMemcachedOk()) return false;

    std::string key = openframe::StringTool::toUpper(callsign);

    _stats.cache_register.tries++;
    _stompstats.cache_register.tries++;

    sw.Start();

    std::string buf;
    try {
      mcr = _memcached->get("apnsregister", key, buf);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message()
                     << std::endl);
      _last_cache_fail_at = time(NULL);
    } // catch

    _profile->average("memcached.register", sw.Time());

    if (mcr != MemcachedController::MEMCACHED_CONTROLLER_SUCCESS) {
      _stats.cache_register.misses++;
      _stompstats.cache_register.misses++;
      return false;
    } // if

    _stats.cache_register.hits++;
    _stompstats.cache_register.hits++;

    ret = buf;

    TLOG(LogDebug, << "memcached{register} found key "
                   << key
                   << std::endl);
    TLOG(LogDebug, << "memcached{register} data: "
                   << buf
                   << std::endl);


    return true;
  } // Store::getApnsRegisterFromMemcached

  bool Store::setApnsRegisterInMemcached(const std::string &callsign, const std::string &buf, const time_t expire) {
    bool isOK = true;

    if (!isMemcachedOk()) return false;

    std::string key = openframe::StringTool::toUpper(callsign);

    try {
      _memcached->put("apnsregister", key, buf, expire);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message()
                     << std::endl);
      _last_cache_fail_at = time(NULL);
      return false;
    } // catch

    _stats.cache_register.stored++;
    _stompstats.cache_register.stored++;
    return isOK;
  } // Store::setApnsRegisterInMemcached

  //
  // Memcache Acks
  //
  bool Store::getMessageFromMemcached(const std::string &hash, std::string &ret) {
    MemcachedController::memcachedReturnEnum mcr;
    openframe::Stopwatch sw;

    if (!isMemcachedOk()) return false;

    _stats.cache_message.tries++;
    _stompstats.cache_message.tries++;

    sw.Start();

    std::string buf;
    try {
      mcr = _memcached->get("pushmessage", hash, buf);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message()
                     << std::endl);
      _last_cache_fail_at = time(NULL);
    } // catch

    _profile->average("memcached.message", sw.Time());

    if (mcr != MemcachedController::MEMCACHED_CONTROLLER_SUCCESS) {
      _stats.cache_message.misses++;
      _stompstats.cache_message.misses++;
      return false;
    } // if

    _stats.cache_message.hits++;
    _stompstats.cache_message.hits++;

    ret = buf;

    TLOG(LogDebug, << "memcached{message} found key "
                   << hash
                   << std::endl);
    TLOG(LogDebug, << "memcached{message} data: "
                   << buf
                   << std::endl);


    return true;
  } // Store::getMessageFromMemcached

  bool Store::setMessageInMemcached(const std::string &hash, const std::string &buf, const time_t expire) {
    bool isOK = true;

    if (!isMemcachedOk()) return false;

    try {
      _memcached->put("pushmessage", hash, buf, expire);
    } // try
    catch(MemcachedController_Exception e) {
      TLOG(LogError, << e.message()
                     << std::endl);
      _last_cache_fail_at = time(NULL);
      return false;
    } // catch

    _stats.cache_message.stored++;
    _stompstats.cache_message.stored++;
    return isOK;
  } // Store::setMessageInMemcached

  openframe::DBI::simpleResultSizeType Store::setApnsPush(const std::string &id,
                                                          const std::string &message) {
    return _dbi->setApnsPush(id, message);
  } // Store::setApnsPush

  apns_registers_st Store::getApnsRegisterByCallsign(const std::string &callsign,
                                                     apns_registers_t &ret) {
    // First try and find whether we either have an 'found'
    // or a not 'found' from memcached
    std::string buf;
    bool ok = getApnsRegisterFromMemcached(callsign, buf);
    if (ok) {
      openframe::Vars v(buf);
      if ( v.is("fnd") ) {
        // not found
        if (v["fnd"] == "0") {
          TLOG(LogDebug, << "got not found from memcached for "
                        << callsign
                        << std::endl);
          return 0;
        } // if
        else if (v["fnd"] == "1" && v.is("bdy") ) {
          // found valid packet
          openframe::StringToken st;
          st.setDelimiter(',');
          st = v["bdy"];
          if (st.size() && st.size() % 3 == 0) {
            for(size_t i = 0; i < st.size(); i += 3) {
              apns_register_t *ar = new apns_register_t;
              ar->id = st[i];
              ar->device_token = st[i+1];
              ar->environment = st[i+2];
              ret.push_back(ar);
            } // for

            TLOG(LogDebug, << "found and parsed "
                           << ret.size()
                           << " from memcached for "
                           << callsign
                           << std::endl);

            return ret.size();
          } // if
          else TLOG(LogInfo, << "got invalid found packet from memcached for "
                             << callsign
                             << "; "
                             << v["bdy"]
                             << std::endl);
        } // if
      } // if
    } // if

    _stats.sql_register.tries++;
    _stompstats.sql_register.tries++;

    openframe::DBI::resultType res;
    ok = _dbi->getApnsRegisterByCallsign(callsign, res);
    if (!ok) {
      _stats.sql_register.misses++;
      _stompstats.sql_register.misses++;

      openframe::Vars v;
      v.add("fnd", "0");
      ok = setApnsRegisterInMemcached(callsign, v.compile(), 3600);
      TLOG(LogDebug, << "setting not found in memcached for "
                     << callsign
                     << std::endl);
      return 0;
    } // if

    _stats.sql_register.hits++;
    _stompstats.sql_register.hits++;

    std::stringstream bdy;
    for(openframe::DBI::resultSizeType i = 0; i < res.num_rows(); i++) {
      apns_register_t *ar = new apns_register_t;
      res[i]["id"].to_string(ar->id);
      res[i]["device_token"].to_string(ar->device_token);
      res[i]["environment"].to_string(ar->environment);
      ret.push_back(ar);

      bdy << (i ? "," : "") << ar->id << "," << ar->device_token << "," << ar->environment;
    } // for

    openframe::Vars v;
    v.add("bdy", bdy.str() );
    v.add("fnd", "1");
    ok = setApnsRegisterInMemcached(callsign, v.compile(), 3600);
    TLOG(LogDebug, << "setting found "
                   << ok
                   << " in memcached with "
                   << ret.size()
                   << " rows for "
                   << callsign
                   << std::endl);

    return ret.size();
  } // Store::getApnsRegisterByCallsign

/*
  bool Store::getApnsRegisterByCallsign(const std::string &callsign,
                                        std::string &ret_id,
                                        std::string &ret_device_token,
                                        std::string &ret_environment) {
    std::string buf;
    bool ok = getApnsRegisterFromMemcached(callsign, buf);
    if (ok) {
      openframe::Vars v(buf);
      if (v->is("id,dt,en")) {
        ret_id = v["id"];
        ret_device_token = v["dt"];
        ret_environment = v["en"];
        return true;
      } // if
    } // if

    // couldn't find it in memcache find it in SQL
    ok = _dbi->getApnsRegisterByCallsign(callsign, res);
    // no match we're done
    if (!ok) return false;

    res[0]["id"].to_string(ret_id);
    res[0]["device_token"].to_string(ret_device_token);
    res[0]["environment"].to_string(ret_environment);

    // we found it, set it in memcached
    openframe::Vars v;
    v.add("id", ret_id);
    v.add("dt", ret_device_token);
    v.add("en", ret_environment);
    ok = setApnsRegisterInMemcached(callsign, v.compile() );

    return true;
  } // Store::getApnsRegisterByCallsign
*/
} // namespace apnspusher
