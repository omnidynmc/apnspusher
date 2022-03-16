/**************************************************************************
 ** Dynamic Networking Solutions                                         **
 **************************************************************************
 ** OpenAPRS, Internet APRS MySQL Injector                               **
 ** Copyright (C) 1999 Gregory A. Carter                                 **
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
 $Id: Vars.cpp,v 1.1 2005/11/21 18:16:04 omni Exp $
 **************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>
#include <iostream>
#include <string>
#include <exception>
#include <sstream>

#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#include <math.h>

#include <openframe/openframe.h>

#include "DBI.h"

namespace apnspusher {
  using namespace openframe::loglevel;

  /**************************************************************************
   ** DBI_Inject Class                                                     **
   **************************************************************************/

  /******************************
   ** Constructor / Destructor **
   ******************************/

  DBI_Apns::DBI_Apns(const thread_id_t thread_id,
                     const std::string &db,
                     const std::string &host,
                     const std::string &user,
                     const std::string &pass)
           : LogObject(thread_id),
             DBI(db, host, user, pass) {
  } // DBI_Apns::DBI_Apns

  DBI_Apns::~DBI_Apns() {
  } // DBI_Apns::~DBI_Apns

  void DBI_Apns::prepare_queries() {
    add_query("i_apns_feedback", " \
      INSERT INTO apns_feedback \
                  (apns_timestamp, device_token, create_ts) \
           VALUES ('%d', '%s', UNIX_TIMESTAMP())");

    add_query("s_apns_register", "\
      SELECT apns_register.id, apns_register.device_token, apns_register.environment \
        FROM apns_register \
             INNER JOIN web_users ON web_users.id = apns_register.user_id \
       WHERE apns_register.callsign = %0q:callsign \
         AND apns_register.active = 'Y' \
         AND web_users.active = 'Y'");

    add_query("i_apns_push", "\
      INSERT INTO apns_push \
                  (apns_register_id, badge, alertmsg, sent, create_ts) \
           VALUES (%0q:id, '1', %1q:alertmsg, 'Y', UNIX_TIMESTAMP() )");
  } // DBI_Apns::prepare_queries

  openframe::DBI::resultSizeType DBI_Apns::getApnsRegisterByCallsign(const std::string &callsign,
                                                                     openframe::DBI::resultType &res) {
    DBI::resultSizeType numRows = 0;

    mysqlpp::Query *query = q("s_apns_register");

    try {
      res = query->store(callsign);
      numRows = res.num_rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getApnsRegisterByCallsign}: #"
                    << e.errnum()
                    << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{getApnsRegisterByCallsign}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI_Apns::getApnsRegisterByCallsign

  openframe::DBI::simpleResultSizeType DBI_Apns::setApnsPush(const std::string &id,
                                                             const std::string &message) {
    int numRows = 0;

    mysqlpp::Query *query = q("i_apns_push");

    DBI::simpleResultType res;
    try {
      res = query->execute(id, message);

      numRows = res.rows();

      while(query->more_results()) query->store_next();
    } // try
    catch(const mysqlpp::BadQuery &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setApnsPush}: #"
                    << e.errnum()
                   << " " << e.what()
                    << std::endl);
    } // catch
    catch(const mysqlpp::Exception &e) {
      TLOG(LogWarn, << "*** MySQL++ Error{setApnsPush}: "
                    << " " << e.what()
                    << std::endl);
    } // catch

    return numRows;
  } // DBI_Apns::setApnsPush

} // namespace apnspusher
