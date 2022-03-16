/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, mySQL APRS Injector                                        **
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
 $Id: DCC.h,v 1.8 2003/09/04 00:22:00 omni Exp $
 **************************************************************************/

#ifndef APNSPUSHER_STORE_H
#define APNSPUSHER_STORE_H

#include <openframe/openframe.h>
#include <openstats/StatsClient_Interface.h>

#include "DBI.h"

namespace apnspusher {

/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/

/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/
  struct apns_register_t {
    std::string id;
    std::string device_token;
    std::string environment;
  }; // struct apns_register_t

  typedef std::deque<apns_register_t *> apns_registers_t;
  typedef apns_registers_t::iterator apns_registers_itr;
  typedef apns_registers_t::const_iterator apns_registers_citr;
  typedef apns_registers_t::size_type apns_registers_st;

  class MemcachedController;
  class Store : public openframe::LogObject,
                public openstats::StatsClient_Interface {
    public:
      static const time_t kDefaultReportInterval;

        Store(const thread_id_t thread_id,
              const std::string &host,
              const std::string &user,
              const std::string &pass,
              const std::string &db,
              std::string memcached_host,
              const time_t expire_interval,
              const time_t report_interval=kDefaultReportInterval);
        virtual ~Store();
        Store &init();
        void onDescribeStats();
        void onDestroyStats();

        void try_stats();

        bool getMessageFromMemcached(const std::string &hash, std::string &ret);
        bool setMessageInMemcached(const std::string &hash, const std::string &buf, const time_t expire);

        bool getApnsRegisterFromMemcached(const std::string &callsign, std::string &ret);
        bool setApnsRegisterInMemcached(const std::string &callsign, const std::string &buf, const time_t expire);

        apns_registers_st getApnsRegisterByCallsign(const std::string &callsign,
                                                    apns_registers_t &ret);
        openframe::DBI::simpleResultSizeType setApnsPush(const std::string &id,
                                                         const std::string &message);

//        openframe::DBI::resultSizeType getApnsRegisterByCallsign(const std::string &callsign,
//                                                                 openframe::DBI::resultType &res);

    // ### Variables ###

    protected:
      void try_stompstats();
      bool isMemcachedOk() const { return _last_cache_fail_at < time(NULL) - 60; }

    private:
      DBI_Apns *_dbi;			// new Injection handler
      MemcachedController *_memcached;	// memcached controller instance
      openframe::Stopwatch *_profile;

      // contructor vars
      std::string _host;
      std::string _user;
      std::string _pass;
      std::string _db;
      std::string _memcached_host;
      time_t _expire_interval;
      time_t _last_cache_fail_at;

      struct memcache_stats_t {
        unsigned int hits;
        unsigned int misses;
        unsigned int tries;
        unsigned int stored;
      }; // memcache_stats_t

      struct sql_stats_t {
        unsigned int hits;
        unsigned int misses;
        unsigned int tries;
        unsigned int inserted;
        unsigned int failed;
      };

      struct obj_stats_t {
        memcache_stats_t cache_message;
        memcache_stats_t cache_register;
        sql_stats_t sql_register;
        time_t last_report_at;
        time_t report_interval;
        time_t created_at;
      } _stats;
      obj_stats_t _stompstats;

      void init_stats(obj_stats_t &stats, const bool startup=false);
  }; // Store

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace aprscreate
#endif
